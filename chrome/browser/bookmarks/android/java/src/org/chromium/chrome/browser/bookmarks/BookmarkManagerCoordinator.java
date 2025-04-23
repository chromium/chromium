// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.ViewGroup;

import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.lifecycle.LifecycleOwner;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemAnimator;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.signin.signin_promo.BookmarkSigninPromoDelegate;
import org.chromium.chrome.browser.ui.signin.signin_promo.SigninPromoCoordinator;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.SearchDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.function.Consumer;

/** Responsible for setting up sub-components and routing incoming/outgoing signals */
// TODO(crbug.com/40268641): Add a new coordinator so this class doesn't own everything.
public class BookmarkManagerCoordinator
        implements SearchDelegate, BackPressHandler, OnAttachStateChangeListener {

    private final SelectionDelegate<BookmarkId> mSelectionDelegate =
            new SelectionDelegate<>() {
                @Override
                public boolean toggleSelectionForItem(BookmarkId bookmark) {
                    if (mBookmarkModel.getBookmarkById(bookmark) != null
                            && !mBookmarkModel.getBookmarkById(bookmark).isEditable()) {
                        return false;
                    }
                    return super.toggleSelectionForItem(bookmark);
                }
            };

    private static final class DragAndCancelAdapter extends DragReorderableRecyclerViewAdapter {
        DragAndCancelAdapter(Context context, ModelList modelList) {
            super(context, modelList);
        }

        @Override
        public boolean onFailedToRecycleView(@NonNull ViewHolder holder) {
            // The view has transient state, which is probably because there's an outstanding
            // fade animation. Theoretically we could clear it and let the RecyclerView continue
            // normally, but it seems sometimes this is called after bind, and the transient
            // state is really just the fade in animation of the new content. For more details
            // see https://crbug.com/1496181. Instead, return true to tell the RecyclerView to
            // reuse the view regardless. The view binding code should be robust enough to
            // handle an in progress animation anyway.
            return true;
        }

        @Override
        public void onViewRecycled(ViewHolder holder) {
            if (holder.itemView instanceof CancelableAnimator cancelable) {
                // Try to eagerly clean up any in progress animations if there are anything. This
                // should reduce the amount of transient state the view has, which could get in the
                // way of view recycling. This approach is likely not strictly necessary, but no
                // point to run animations after a view is recycled anyway.
                cancelable.cancelAnimation();
            }
            super.onViewRecycled(holder);
        }
    }

    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier =
            new ObservableSupplierImpl<>();
    private final Context mContext;
    private final ViewGroup mMainView;
    private final SelectableListLayout<BookmarkId> mSelectableListLayout;
    private final RecyclerView mRecyclerView;
    private final BookmarkOpener mBookmarkOpener;
    private final BookmarkToolbarCoordinator mBookmarkToolbarCoordinator;
    private final BookmarkManagerMediator mMediator;
    private final ImageFetcher mImageFetcher;
    private final SnackbarManager mSnackbarManager;
    private final SigninPromoCoordinator mSigninPromoCoordinator;
    private final BookmarkPromoHeader mPromoHeaderManager;
    private final BookmarkModel mBookmarkModel;
    private final Profile mProfile;
    private final BookmarkUiPrefs mBookmarkUiPrefs;
    private final ModalDialogManager mModalDialogManager;
    private final ModelList mModelList;

    /**
     * Creates an instance of {@link BookmarkManagerCoordinator}. It also initializes resources,
     * bookmark models and jni bridges.
     *
     * @param context The current {@link Context} used to obtain resources or inflate views.
     * @param isDialogUi Whether the main bookmarks UI will be shown in a dialog, not a NativePage.
     * @param snackbarManager The {@link SnackbarManager} used to display snackbars.
     * @param profile The profile which the manager is running in.
     * @param bookmarkUiPrefs Manages prefs for bookmarks ui.
     * @param bookmarkOpener Helper class to open bookmarks.
     * @param bookmarkManagerOpener Helper class to open bookmark activities.
     * @param priceDropNotificationManager Manages price drop notifications.
     */
    public BookmarkManagerCoordinator(
            @NonNull Context context,
            boolean isDialogUi,
            @NonNull SnackbarManager snackbarManager,
            @NonNull Profile profile,
            @NonNull BookmarkUiPrefs bookmarkUiPrefs,
            @NonNull BookmarkOpener bookmarkOpener,
            @NonNull BookmarkManagerOpener bookmarkManagerOpener,
            @NonNull PriceDropNotificationManager priceDropNotificationManager) {
        mContext = context;
        mProfile = profile;
        mImageFetcher =
                ImageFetcherFactory.createImageFetcher(
                        ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                        profile.getProfileKey(),
                        GlobalDiscardableReferencePool.getReferencePool());
        mSnackbarManager = snackbarManager;

        mMainView = (ViewGroup) LayoutInflater.from(context).inflate(R.layout.bookmark_main, null);
        mBookmarkModel = BookmarkModel.getForProfile(profile);
        mBookmarkOpener = bookmarkOpener;
        ShoppingService service = ShoppingServiceFactory.getForProfile(profile);
        if (CommerceFeatureUtils.isShoppingListEligible(service)) {
            service.scheduleSavedProductUpdate();
        }
        mBookmarkUiPrefs = bookmarkUiPrefs;

        @SuppressWarnings("unchecked")
        SelectableListLayout<BookmarkId> selectableList =
                mMainView.findViewById(R.id.selectable_list);
        mSelectableListLayout = selectableList;

        mModelList = new ModelList();
        DragReorderableRecyclerViewAdapter dragReorderableRecyclerViewAdapter =
                new DragAndCancelAdapter(context, mModelList);
        mRecyclerView =
                mSelectableListLayout.initializeRecyclerView(dragReorderableRecyclerViewAdapter);

        // Disable everything except move animations. Switching between folders should be as
        // seamless as possible without flickering caused by these animations. While dragging
        // should still pick up the slide animation from moves.
        ItemAnimator itemAnimator = mRecyclerView.getItemAnimator();
        itemAnimator.setChangeDuration(0);
        itemAnimator.setAddDuration(0);
        itemAnimator.setRemoveDuration(0);

        mModalDialogManager =
                new ModalDialogManager(new AppModalPresenter(context), ModalDialogType.APP);

        // Using OneshotSupplier as an alternative to a 2-step initialization process.
        OneshotSupplierImpl<BookmarkDelegate> bookmarkDelegateSupplier =
                new OneshotSupplierImpl<>();
        mBookmarkToolbarCoordinator =
                new BookmarkToolbarCoordinator(
                        context,
                        mProfile,
                        mSelectableListLayout,
                        mSelectionDelegate,
                        /* searchDelegate= */ this,
                        dragReorderableRecyclerViewAdapter,
                        isDialogUi,
                        bookmarkDelegateSupplier,
                        mBookmarkModel,
                        mBookmarkOpener,
                        mBookmarkUiPrefs,
                        mModalDialogManager,
                        this::onEndSearch,
                        () -> IncognitoUtils.isIncognitoModeEnabled(profile),
                        bookmarkManagerOpener,
                        /* nextFocusableView= */ mMainView.findViewById(R.id.list_content));
        mSelectableListLayout.configureWideDisplayStyle();

        final @BookmarkRowDisplayPref int displayPref =
                mBookmarkUiPrefs.getBookmarkRowDisplayPref();
        BookmarkImageFetcher bookmarkImageFetcher =
                new BookmarkImageFetcher(
                        profile,
                        context,
                        mBookmarkModel,
                        mImageFetcher,
                        BookmarkViewUtils.getRoundedIconGenerator(context, displayPref));

        BookmarkUndoController bookmarkUndoController =
                new BookmarkUndoController(context, mBookmarkModel, snackbarManager);
        Consumer<OnScrollListener> onScrollListenerConsumer =
                onScrollListener -> mRecyclerView.addOnScrollListener(onScrollListener);
        mMediator =
                new BookmarkManagerMediator(
                        (Activity) context,
                        (LifecycleOwner) context,
                        mModalDialogManager,
                        mBookmarkModel,
                        mBookmarkOpener,
                        mSelectableListLayout,
                        mSelectionDelegate,
                        mRecyclerView,
                        dragReorderableRecyclerViewAdapter,
                        isDialogUi,
                        mBackPressStateSupplier,
                        mProfile,
                        bookmarkUndoController,
                        mModelList,
                        mBookmarkUiPrefs,
                        this::hideKeyboard,
                        bookmarkImageFetcher,
                        ShoppingServiceFactory.getForProfile(mProfile),
                        mSnackbarManager,
                        this::canShowSigninPromo,
                        onScrollListenerConsumer,
                        bookmarkManagerOpener,
                        priceDropNotificationManager);
        mPromoHeaderManager = mMediator.getPromoHeaderManager();

        bookmarkDelegateSupplier.set(/* object= */ mMediator);

        mMainView.addOnAttachStateChangeListener(this);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP)) {
            mSigninPromoCoordinator =
                    new SigninPromoCoordinator(
                            context,
                            mProfile.getOriginalProfile(),
                            new BookmarkSigninPromoDelegate(
                                    context,
                                    mProfile.getOriginalProfile(),
                                    SigninAndHistorySyncActivityLauncherImpl.get(),
                                    mMediator::onPromoVisibilityChange,
                                    this::openSettings));
            dragReorderableRecyclerViewAdapter.registerType(
                    ViewType.SIGNIN_PROMO,
                    mSigninPromoCoordinator::buildPromoView,
                    // SigninPromoCoordinator owns the model and keys for the promo inside it.
                    // The PropertyModel and BookmarkManagerProperties key passed to this binder
                    // method are thus not needed.
                    (model, view, key) -> mSigninPromoCoordinator.setView(view));
            dragReorderableRecyclerViewAdapter.registerType(
                    ViewType.BATCH_UPLOAD_CARD,
                    this::buildBatchUploadCardView,
                    BookmarkManagerViewBinder::bindBatchUploadCardView);
        } else {
            mSigninPromoCoordinator = null;
            dragReorderableRecyclerViewAdapter.registerType(
                    ViewType.SIGNIN_PROMO,
                    this::buildPersonalizedPromoView,
                    BookmarkManagerViewBinder::bindPersonalizedPromoView);
        }
        dragReorderableRecyclerViewAdapter.registerType(
                ViewType.SECTION_HEADER,
                this::buildSectionHeaderView,
                BookmarkManagerViewBinder::bindSectionHeaderView);
        dragReorderableRecyclerViewAdapter.registerType(
                ViewType.DIVIDER,
                BookmarkManagerCoordinator::buildDividerView,
                BookmarkManagerViewBinder::bindDividerView);
        dragReorderableRecyclerViewAdapter.registerDraggableType(
                ViewType.IMPROVED_BOOKMARK_VISUAL,
                BookmarkManagerCoordinator::buildVisualImprovedBookmarkRow,
                ImprovedBookmarkRowViewBinder::bind,
                (viewHolder, itemTouchHelper) -> {},
                mMediator.getDraggabilityProvider());
        dragReorderableRecyclerViewAdapter.registerDraggableType(
                ViewType.IMPROVED_BOOKMARK_COMPACT,
                BookmarkManagerCoordinator::buildCompactImprovedBookmarkRow,
                ImprovedBookmarkRowViewBinder::bind,
                (viewHolder, itemTouchHelper) -> {},
                mMediator.getDraggabilityProvider());
        dragReorderableRecyclerViewAdapter.registerType(
                ViewType.SEARCH_BOX,
                this::buildSearchBoxRow,
                BookmarkSearchBoxRowViewBinder.createViewBinder());
        dragReorderableRecyclerViewAdapter.registerType(
                ViewType.EMPTY_STATE,
                this::buildEmptyStateView,
                BookmarkManagerEmptyStateViewBinder::bindEmptyStateView);

        RecordUserAction.record("MobileBookmarkManagerOpen");
        if (!isDialogUi) {
            RecordUserAction.record("MobileBookmarkManagerPageOpen");
        }
    }

    // Public API implementation.

    /** Destroys and cleans up itself. This must be called after done using this class. */
    public void onDestroyed() {
        RecordUserAction.record("MobileBookmarkManagerClose");
        mMainView.removeOnAttachStateChangeListener(this);
        mSelectableListLayout.onDestroyed();
        mMediator.onDestroy();
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP)) {
            assert mSigninPromoCoordinator != null;
            mSigninPromoCoordinator.destroy();
        }
    }

    /** Returns the view that shows the main bookmarks UI. */
    public View getView() {
        return mMainView;
    }

    /** Sets the listener that reacts upon the change of the UI state of bookmark manager. */
    // TODO(crbug.com/40257874): Create abstraction between BookmarkManager & BasicNativePage.
    public void setBasicNativePage(BasicNativePage nativePage) {
        mMediator.setBasicNativePage(nativePage);
    }

    /**
     * Updates UI based on the new URL. If the bookmark model is not loaded yet, cache the url and
     * it will be picked up later when the model is loaded. This method is supposed to align with
     * {@link BookmarkPage#updateForUrl(String)}
     *
     * <p>
     *
     * @param url The url to navigate to.
     */
    public void updateForUrl(String url) {
        mMediator.updateForUrl(url);
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
    /**
     * Called when the user presses the back key. This is only going to be called on Phone.
     *
     * @return True if manager handles this event, false if it decides to ignore.
     */
    private boolean onBackPressed() {
        return mMediator.onBackPressed();
    }

    @VisibleForTesting
    View buildPersonalizedPromoView(ViewGroup parent) {
        return mPromoHeaderManager.createPersonalizedSigninAndSyncPromoHolder(parent);
    }

    @VisibleForTesting
    View buildLegacyPromoView(ViewGroup parent) {
        return mPromoHeaderManager.createSyncPromoHolder(parent);
    }

    @VisibleForTesting
    View buildBatchUploadCardView(ViewGroup parent) {
        // The signin_settings_card_view is used for Batch Upload Cards.
        return inflate(parent, R.layout.signin_settings_card_view);
    }

    @VisibleForTesting
    View buildSectionHeaderView(ViewGroup parent) {
        return inflate(
                parent,
                mBookmarkModel.areAccountBookmarkFoldersActive()
                        ? R.layout.bookmark_section_header_v2
                        : R.layout.bookmark_section_header);
    }

    static @VisibleForTesting View buildDividerView(ViewGroup parent) {
        return inflate(parent, R.layout.list_section_divider);
    }

    static ImprovedBookmarkRow buildCompactImprovedBookmarkRow(ViewGroup parent) {
        ImprovedBookmarkRow row = ImprovedBookmarkRow.buildView(parent.getContext(), false);
        return row;
    }

    static ImprovedBookmarkRow buildVisualImprovedBookmarkRow(ViewGroup parent) {
        ImprovedBookmarkRow row = ImprovedBookmarkRow.buildView(parent.getContext(), true);
        return row;
    }

    View buildSearchBoxRow(ViewGroup parent) {
        return inflate(parent, R.layout.bookmark_search_box_row);
    }

    View buildEmptyStateView(ViewGroup parent) {
        ViewGroup emptyStateView = (ViewGroup) inflate(parent, R.layout.empty_state_view);
        emptyStateView.setTouchscreenBlocksFocus(true);
        return emptyStateView;
    }

    boolean canShowSigninPromo() {
        assert mSigninPromoCoordinator != null;
        return mSigninPromoCoordinator.canShowPromo();
    }

    private static View inflate(ViewGroup parent, @LayoutRes int layoutId) {
        Context context = parent.getContext();
        return LayoutInflater.from(context).inflate(layoutId, parent, false);
    }

    private void hideKeyboard() {
        KeyboardVisibilityDelegate.getInstance().hideKeyboard(mMainView);
    }

    // Testing methods.

    public BookmarkToolbarCoordinator getToolbarCoordinatorForTesting() {
        return mBookmarkToolbarCoordinator;
    }

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

    public void finishLoadingForTesting() {
        mMediator.finishLoadingForTesting(); // IN-TEST
    }

    public BookmarkOpener getBookmarkOpenerForTesting() {
        return mBookmarkOpener;
    }

    public BookmarkDelegate getBookmarkDelegateForTesting() {
        return mMediator;
    }

    public BookmarkManagerTestingDelegate getTestingDelegate() {
        return new BookmarkManagerTestingDelegate() {
            @Override
            public BookmarkId getBookmarkIdByPositionForTesting(int position) {
                return mMediator.getIdByPositionForTesting(position);
            }

            @Override
            public ImprovedBookmarkRow getBookmarkRowByPosition(int position) {
                return (ImprovedBookmarkRow) getBookmarkViewHolderByPosition(position).itemView;
            }

            @Override
            public ViewHolder getBookmarkViewHolderByPosition(int position) {
                return getViewHolderByPosition(getBookmarkStartIndex() + position);
            }

            @Override
            public ViewHolder getViewHolderByPosition(int position) {
                return mRecyclerView.findViewHolderForAdapterPosition(position);
            }

            @Override
            public int getBookmarkCount() {
                int bookmarkCount = 0;
                for (ListItem item : mModelList) {
                    if (item.type == BookmarkListEntry.ViewType.IMPROVED_BOOKMARK_VISUAL
                            || item.type == BookmarkListEntry.ViewType.IMPROVED_BOOKMARK_COMPACT) {
                        bookmarkCount++;
                    }
                }

                return bookmarkCount;
            }

            @Override
            public int getBookmarkStartIndex() {
                return mMediator.getBookmarkItemStartIndex();
            }

            @Override
            public int getBookmarkEndIndex() {
                return mMediator.getBookmarkItemEndIndex();
            }

            @Override
            public void searchForTesting(@Nullable String query) {
                mMediator.search(query);
            }

            @Override
            public void simulateSignInForTesting() {
                mMediator.simulateSignInForTesting();
            }
        };
    }

    public ModelList getModelListForTesting() {
        return mModelList;
    }

    public BookmarkUiPrefs getBookmarkUiPrefsForTesting() {
        return mBookmarkUiPrefs;
    }

    private void openSettings() {
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(
                        mContext,
                        ManageSyncSettings.class,
                        ManageSyncSettings.createArguments(false));
    }
}
