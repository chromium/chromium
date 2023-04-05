// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.ViewGroup;

import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
import org.chromium.chrome.browser.commerce.ShoppingFeatures;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.SearchDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/** Responsible for setting up sub-components and routing incoming/outgoing signals */
public class BookmarkManagerCoordinator
        implements SearchDelegate, BackPressHandler, OnAttachStateChangeListener {
    private static final int FAVICON_MAX_CACHE_SIZE_BYTES =
            10 * ConversionUtils.BYTES_PER_MEGABYTE; // 10MB

    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier =
            new ObservableSupplierImpl<>();
    private final ViewGroup mMainView;
    private final SelectableListLayout<BookmarkId> mSelectableListLayout;
    private final RecyclerView mRecyclerView;
    private final BookmarkOpener mBookmarkOpener;
    private final BookmarkToolbarCoordinator mBookmarkToolbarCoordinator;
    private final BookmarkManagerMediator mMediator;
    private final ImageFetcher mImageFetcher;
    private final SnackbarManager mSnackbarManager;
    private final BookmarkPromoHeader mPromoHeaderManager;
    private final BookmarkModel mBookmarkModel;
    private final Profile mProfile;

    /**
     * Creates an instance of {@link BookmarkManager}. It also initializes resources,
     * bookmark models and jni bridges.
     * @param context The current {@link Context} used to obtain resources or inflate views.
     * @param openBookmarkComponentName The component to use when opening a bookmark.
     * @param isDialogUi Whether the main bookmarks UI will be shown in a dialog, not a NativePage.
     * @param isIncognito Whether the tab model loading the bookmark manager is for incognito mode.
     * @param snackbarManager The {@link SnackbarManager} used to display snackbars.
     * @param profile The profile which the manager is running in.
     */
    public BookmarkManagerCoordinator(Context context, ComponentName openBookmarkComponentName,
            boolean isDialogUi, boolean isIncognito, SnackbarManager snackbarManager,
            Profile profile) {
        mProfile = profile;
        mImageFetcher =
                ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                        profile.getProfileKey(), GlobalDiscardableReferencePool.getReferencePool());
        mSnackbarManager = snackbarManager;

        mMainView = (ViewGroup) LayoutInflater.from(context).inflate(R.layout.bookmark_main, null);
        mBookmarkModel = BookmarkModel.getForProfile(profile);
        mBookmarkOpener = new BookmarkOpener(mBookmarkModel, context, openBookmarkComponentName);
        if (ShoppingFeatures.isShoppingListEligible()) {
            ShoppingServiceFactory.getForProfile(profile).scheduleSavedProductUpdate();
        }

        SelectionDelegate<BookmarkId> selectionDelegate = new SelectionDelegate<>() {
            @Override
            public boolean toggleSelectionForItem(BookmarkId bookmark) {
                if (mBookmarkModel.getBookmarkById(bookmark) != null
                        && !mBookmarkModel.getBookmarkById(bookmark).isEditable()) {
                    return false;
                }
                return super.toggleSelectionForItem(bookmark);
            }
        };

        @SuppressWarnings("unchecked")
        SelectableListLayout<BookmarkId> selectableList =
                mMainView.findViewById(R.id.selectable_list);
        mSelectableListLayout = selectableList;
        mSelectableListLayout.initializeEmptyView(R.string.bookmarks_folder_empty);

        BookmarkItemsAdapter bookmarkItemsAdapter =
                new BookmarkItemsAdapter(context, this::createView, this::bindView);
        mRecyclerView = mSelectableListLayout.initializeRecyclerView(
                (RecyclerView.Adapter<RecyclerView.ViewHolder>) bookmarkItemsAdapter);

        // Using OneshotSupplier as an alternative to a 2-step initialization process.
        OneshotSupplierImpl<BookmarkDelegate> bookmarkDelegateSupplier =
                new OneshotSupplierImpl<>();
        mBookmarkToolbarCoordinator = new BookmarkToolbarCoordinator(mSelectableListLayout,
                selectionDelegate, /*searchDelegate=*/this, bookmarkItemsAdapter, isDialogUi,
                bookmarkDelegateSupplier, mBookmarkModel, mBookmarkOpener);
        mSelectableListLayout.configureWideDisplayStyle();

        LargeIconBridge largeIconBridge = new LargeIconBridge(mProfile);
        largeIconBridge.createCache(computeCacheMaxSize());

        BookmarkUndoController bookmarkUndoController =
                new BookmarkUndoController(context, mBookmarkModel, snackbarManager);
        mMediator = new BookmarkManagerMediator(context, mBookmarkModel, mBookmarkOpener,
                mSelectableListLayout, selectionDelegate, mRecyclerView, bookmarkItemsAdapter,
                largeIconBridge, isDialogUi, isIncognito, mBackPressStateSupplier, mProfile,
                bookmarkUndoController);
        mPromoHeaderManager = mMediator.getPromoHeaderManager();

        bookmarkDelegateSupplier.set(/*bookmarkDelegate=*/mMediator);

        mMainView.addOnAttachStateChangeListener(this);

        RecordUserAction.record("MobileBookmarkManagerOpen");
        if (!isDialogUi) {
            RecordUserAction.record("MobileBookmarkManagerPageOpen");
        }
    }

    // Public API implementation.

    /**
     * Destroys and cleans up itself. This must be called after done using this class.
     */
    public void onDestroyed() {
        RecordUserAction.record("MobileBookmarkManagerClose");
        mMainView.removeOnAttachStateChangeListener(this);
        mSelectableListLayout.onDestroyed();
        mMediator.onDestroy();
    }

    /**
     * @return The view that shows the main browsing history UI.
     */
    public View getView() {
        return mMainView;
    }

    /**
     * Sets the listener that reacts upon the change of the UI state of bookmark manager.
     */
    // TODO(crbug.com/1418859): Create abstraction between BookmarkManager & BasicNativePage.
    public void setBasicNativePage(BasicNativePage nativePage) {
        mMediator.setBasicNativePage(nativePage);
    }

    /**
     * Updates UI based on the new URL. If the bookmark model is not loaded yet, cache the url and
     * it will be picked up later when the model is loaded. This method is supposed to align with
     * {@link BookmarkPage#updateForUrl(String)}
     * <p>
     * @param url The url to navigate to.
     */
    public void updateForUrl(String url) {
        mMediator.updateForUrl(url);
    }

    /**
     * Called when the user presses the back key. This is only going to be called on Phone.
     * @return True if manager handles this event, false if it decides to ignore.
     */
    public boolean onBackPressed() {
        return mMediator.onBackPressed();
    }

    /** Opens the given BookmarkId. */
    public void openBookmark(BookmarkId bookmarkId) {
        mMediator.openBookmark(bookmarkId);
    }

    // OnAttachStateChangeListener implementation.

    @Override
    public void onViewAttachedToWindow(@NonNull View view) {
        mMediator.onAttachedToWindow();
    }

    @Override
    public void onViewDetachedFromWindow(@NonNull View view) {
        mMediator.onDetachedFromWindow();
    }

    // BackPressHandler implementation.

    @Override
    public @BackPressResult int handleBackPress() {
        return onBackPressed() ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressStateSupplier;
    }

    // SearchDelegate implementation.

    @Override
    public void onSearchTextChanged(String query) {
        mMediator.search(query);
    }

    @Override
    public void onEndSearch() {
        mMediator.onEndSearch();
    }

    // Private methods.

    private int computeCacheMaxSize() {
        ActivityManager activityManager =
                ((ActivityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.ACTIVITY_SERVICE));
        return Math.min(activityManager.getMemoryClass() / 4 * ConversionUtils.BYTES_PER_MEGABYTE,
                FAVICON_MAX_CACHE_SIZE_BYTES);
    }

    // View creation methods.

    /**
     * Should only be called after fully initialized.
     * @param parent The parent to which the new {@link View} will be added as a child.
     * @param viewType The type of row being created.
     * @return A new View that can be added to the view hierarchy.
     */
    public View createView(@NonNull ViewGroup parent, @ViewType int viewType) {
        // The shopping-specific bookmark row is only shown with the visual refresh. When
        // there's a mismatch, the ViewType is downgraded to ViewType.BOOKMARK.
        if (viewType == ViewType.SHOPPING_POWER_BOOKMARK
                && !BookmarkFeatures.isBookmarksVisualRefreshEnabled()) {
            viewType = ViewType.BOOKMARK;
        }

        switch (viewType) {
            case ViewType.PERSONALIZED_SIGNIN_PROMO:
            case ViewType.PERSONALIZED_SYNC_PROMO:
                return buildPersonalizedPromoView(parent);
            case ViewType.SYNC_PROMO:
                return buildLegacyPromoView(parent);
            case ViewType.SECTION_HEADER:
                return buildSectionHeaderView(parent);
            case ViewType.FOLDER:
                return buildBookmarkFolderView(parent);
            case ViewType.BOOKMARK:
                return buildBookmarkItemView(parent);
            case ViewType.SHOPPING_POWER_BOOKMARK:
                return buildShoppingItemView(parent);
            case ViewType.DIVIDER:
                return buildDividerView(parent);
            case ViewType.SHOPPING_FILTER:
                return buildShoppingFilterView(parent);
            default:
                assert false;
                return null;
        }
    }

    public void bindView(View view, @ViewType int viewType, PropertyModel model) {
        ViewBinder<PropertyModel, View, PropertyKey> viewBinder = null;
        switch (viewType) {
            case ViewType.PERSONALIZED_SIGNIN_PROMO:
            case ViewType.PERSONALIZED_SYNC_PROMO:
                viewBinder = BookmarkManagerViewBinder::bindPersonalizedPromoView;
                break;
            case ViewType.SYNC_PROMO:
                viewBinder = BookmarkManagerViewBinder::bindLegacyPromoView;
                break;
            case ViewType.SECTION_HEADER:
                viewBinder = BookmarkManagerViewBinder::bindSectionHeaderView;
                break;
            case ViewType.FOLDER:
                viewBinder = BookmarkManagerViewBinder::bindBookmarkFolderView;
                break;
            case ViewType.BOOKMARK:
                viewBinder = BookmarkManagerViewBinder::bindBookmarkItemView;
                break;
            case ViewType.SHOPPING_POWER_BOOKMARK:
                viewBinder = BookmarkManagerViewBinder::bindShoppingItemView;
                break;
            case ViewType.DIVIDER:
                viewBinder = BookmarkManagerViewBinder::bindDividerView;
                break;
            case ViewType.SHOPPING_FILTER:
                viewBinder = BookmarkManagerViewBinder::bindShoppingFilterView;
                break;
            default:
                assert false;
        }
        PropertyModelChangeProcessor.create(model, view, viewBinder);
    }

    private View buildPersonalizedPromoView(ViewGroup parent) {
        return mPromoHeaderManager.createPersonalizedSigninAndSyncPromoHolder(parent);
    }

    private View buildLegacyPromoView(ViewGroup parent) {
        return mPromoHeaderManager.createSyncPromoHolder(parent);
    }

    private View buildSectionHeaderView(ViewGroup parent) {
        return inflate(parent, org.chromium.chrome.R.layout.bookmark_section_header);
    }

    private View buildBookmarkFolderView(ViewGroup parent) {
        return inflateBookmarkRow(parent, org.chromium.chrome.R.layout.bookmark_folder_row);
    }

    private View buildBookmarkItemView(ViewGroup parent) {
        return inflateBookmarkRow(parent, org.chromium.chrome.R.layout.bookmark_item_row);
    }

    private View buildShoppingItemView(ViewGroup parent) {
        PowerBookmarkShoppingItemRow row = (PowerBookmarkShoppingItemRow) inflateBookmarkRow(
                parent, org.chromium.chrome.R.layout.power_bookmark_shopping_item_row);
        // TODO(https://crbug.com/1416611): Move init to view binding.
        row.init(mImageFetcher, mBookmarkModel, mSnackbarManager, mProfile);
        return row;
    }

    private View buildDividerView(ViewGroup parent) {
        return inflate(parent, org.chromium.chrome.R.layout.horizontal_divider);
    }

    private View buildShoppingFilterView(ViewGroup parent) {
        return inflate(parent, org.chromium.chrome.R.layout.shopping_filter_row);
    }

    private View inflate(ViewGroup parent, @LayoutRes int layoutId) {
        Context context = parent.getContext();
        return LayoutInflater.from(context).inflate(layoutId, parent, false);
    }

    private View inflateBookmarkRow(ViewGroup parent, @LayoutRes int layoutId) {
        BookmarkRow row = (BookmarkRow) inflate(parent, layoutId);
        // TODO(https://crbug.com/1416611): Move onDelegateInitialized to view binding.
        (row).onDelegateInitialized(/*delegate=*/mMediator);
        return row;
    }

    // Testing methods.

    public BookmarkToolbar getToolbarForTesting() {
        return mBookmarkToolbarCoordinator.getToolbarForTesting(); // IN-TEST
    }

    public BookmarkUndoController getUndoControllerForTesting() {
        return mMediator.getUndoControllerForTesting();
    }

    public RecyclerView getRecyclerViewForTesting() {
        return mRecyclerView;
    }

    public static void preventLoadingForTesting(boolean preventLoading) {
        BookmarkManagerMediator.preventLoadingForTesting(preventLoading);
    }

    public BookmarkOpener getBookmarkOpenerForTesting() {
        return mBookmarkOpener;
    }

    public BookmarkDelegate getBookmarkDelegateForTesting() {
        return mMediator;
    }

    public TestingDelegate getTestingDelegate() {
        return mMediator;
    }
}
