// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.CallbackUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinatorSupplier;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

/** Factory for creating co-browse content. */
@NullMarked
public class CoBrowseViewFactory {

    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final NonNullObservableSupplier<Profile> mProfileSupplier;
    private final SnackbarManager mSnackbarManager;
    private final ContextMenuPopulatorFactory mContextMenuPopulatorFactory;
    private final SelectionDropdownMenuDelegate mSelectionDropdownMenuDelegate;
    private final NullableObservableSupplier<Tab> mActivityTabProvider;
    private final MonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final PriceDropNotificationManager mPriceDropNotificationManager;
    private final BookmarkManagerOpener mBookmarkManagerOpener;

    /**
     * Factory responsible for creating co-browse content.
     *
     * @param activity The current {@link Activity} instance.
     * @param profileSupplier A supplier for the current {@link Profile}.
     * @param windowAndroid The {@link WindowAndroid} for managing window-level operations.
     * @param lifecycleDispatcher The {@link ActivityLifecycleDispatcher} for managing activity
     *     lifecycle.
     * @param snackbarManager The {@link SnackbarManager} for managing snackbar messages.
     * @param contextMenuPopulatorFactory The {@link ContextMenuPopulatorFactory} to show context
     *     menu on the ThinWebView.
     * @param selectionDropdownMenuDelegate The {@link SelectionDropdownMenuDelegate} to handle
     *     selection dropdown menus.
     * @param activityTabProvider The supplier for the active tab.
     * @param tabModelSelectorSupplier The supplier for the active tab model selector.
     * @param priceDropNotificationManager The {@link PriceDropNotificationManager} for bookmarks.
     * @param bookmarkManagerOpener The {@link BookmarkManagerOpener} for opening bookmarks.
     */
    public CoBrowseViewFactory(
            Activity activity,
            NonNullObservableSupplier<Profile> profileSupplier,
            WindowAndroid windowAndroid,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            SnackbarManager snackbarManager,
            ContextMenuPopulatorFactory contextMenuPopulatorFactory,
            SelectionDropdownMenuDelegate selectionDropdownMenuDelegate,
            NullableObservableSupplier<Tab> activityTabProvider,
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            PriceDropNotificationManager priceDropNotificationManager,
            BookmarkManagerOpener bookmarkManagerOpener) {
        mActivity = activity;
        mProfileSupplier = profileSupplier;
        mWindowAndroid = windowAndroid;
        mSnackbarManager = snackbarManager;
        mContextMenuPopulatorFactory = contextMenuPopulatorFactory;
        mSelectionDropdownMenuDelegate = selectionDropdownMenuDelegate;
        mActivityTabProvider = activityTabProvider;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mPriceDropNotificationManager = priceDropNotificationManager;
        mBookmarkManagerOpener = bookmarkManagerOpener;

        TabBottomSheetUtils.attachFactoryToWindow(windowAndroid, this);
    }

    public void destroy() {
        TabBottomSheetUtils.detachFactoryFromWindow(mWindowAndroid);
    }

    /**
     * Called to build the co-browse view. This method is common for glic and contextual tasks.
     * Contextual tasks uses a fusebox overlayed on top of content area while glic only needs the
     * WebContents showing in a ThinWebView.
     *
     * @param webContents The {@link WebContents} to be displayed in the thin web view.
     * @param backgroundColor The background color for the content.
     * @param clientType The client using coBrowseViews.
     * @param containerType The type of container hosting the views.
     * @param requestFocus Whether focus should be requested for the web contents.
     * @return The {@link CoBrowseViews} instance.
     */
    CoBrowseViews buildCoBrowseViews(
            @Nullable WebContents webContents,
            @ColorInt int backgroundColor,
            @TabBottomSheetClientType int clientType,
            @CoBrowseContainerType int containerType,
            boolean requestFocus,
            @Nullable CoBrowseComponentProvider bottomSheetContentProvider) {
        View containerView =
                LayoutInflater.from(mActivity).inflate(R.layout.tab_bottom_sheet, null);

        TabBottomSheetWebUi webUi =
                createWebUi(containerView, backgroundColor, clientType, containerType, webContents);

        webUi.setWebContents(webContents, requestFocus);

        return new CoBrowseViews(
                containerView,
                clientType,
                containerType,
                webUi,
                backgroundColor,
                bottomSheetContentProvider,
                () -> createPeekViewManagerIfNeeded(bottomSheetContentProvider));
    }

    @CalledByNative
    @VisibleForTesting
    public static @Nullable CoBrowseViews buildCoBrowseViews(
            @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid,
            @Nullable @JniType("content::WebContents*") WebContents webContents,
            @TabBottomSheetClientType int clientType,
            @CoBrowseContainerType int containerType,
            boolean requestFocus,
            @Nullable CoBrowseComponentProvider bottomSheetContentProvider) {
        CoBrowseViewFactory factory = TabBottomSheetUtils.getFactoryFromWindow(windowAndroid);
        if (factory == null) {
            return null;
        }

        @ColorInt
        int backgroundColor =
                clientType == TabBottomSheetClientType.GLIC
                        ? factory.mActivity.getColor(R.color.tab_bottom_sheet_glic_bg)
                        : factory.mActivity.getColor(R.color.tab_bottom_sheet_base_bg);
        return factory.buildCoBrowseViews(
                webContents,
                backgroundColor,
                clientType,
                containerType,
                requestFocus,
                bottomSheetContentProvider);
    }

    private TabBottomSheetWebUi createWebUi(
            View containerView,
            @ColorInt int backgroundColor,
            @TabBottomSheetClientType int clientType,
            @CoBrowseContainerType int containerType,
            @Nullable WebContents webContents) {
        return new TabBottomSheetWebUi(
                mActivity,
                containerView,
                mWindowAndroid,
                mContextMenuPopulatorFactory,
                mSelectionDropdownMenuDelegate,
                backgroundColor,
                clientType,
                containerType,
                // Passes a callback to the components layer to open ephemeral tabs,
                // avoiding a circular dependency since the components layer cannot depend
                // on chrome/ UI coordinators directly.
                (GURL url, String title) -> openInEphemeralTab(url, title, webContents),
                this::addToReadingList);
    }

    private void openInEphemeralTab(GURL url, String title, @Nullable WebContents webContents) {
        var ephemeralTabCoordinatorSupplier = EphemeralTabCoordinatorSupplier.from(mWindowAndroid);
        if (ephemeralTabCoordinatorSupplier == null) return;
        EphemeralTabCoordinator coordinator = ephemeralTabCoordinatorSupplier.get();
        if (coordinator == null) return;

        Profile profile = mProfileSupplier.get();
        if (profile == null) return;

        Origin initiatorOrigin = null;
        if (webContents != null && webContents.getMainFrame() != null) {
            initiatorOrigin = webContents.getMainFrame().getLastCommittedOrigin();
        }

        coordinator.requestOpenSheet(
                url,
                /* fullPageUrl= */ null,
                title,
                profile,
                /* canPromoteToNewTab= */ true,
                /* shouldHaveContextMenu= */ true,
                initiatorOrigin,
                /* additionalNavigationParams= */ null,
                /* requestDeniedCallback= */ CallbackUtils.emptyRunnable());
    }

    private void addToReadingList(GURL url, String title) {
        Profile profile = mProfileSupplier.get();
        if (profile == null) return;
        BookmarkModel bookmarkModel = BookmarkModel.getForProfile(profile);
        bookmarkModel.finishLoadingBookmarkModel(
                () -> {
                    BottomSheetController bottomSheetController =
                            BottomSheetControllerProvider.from(mWindowAndroid);
                    if (bottomSheetController == null) return;
                    BookmarkUtils.addToReadingList(
                            mActivity,
                            bookmarkModel,
                            title,
                            url,
                            mSnackbarManager,
                            profile,
                            bottomSheetController,
                            mBookmarkManagerOpener,
                            mPriceDropNotificationManager);
                });
    }

    private @Nullable PeekViewManager createPeekViewManagerIfNeeded(
            @Nullable CoBrowseComponentProvider bottomSheetContentProvider) {
        TabBottomSheetManager manager = TabBottomSheetUtils.getManagerFromWindow(mWindowAndroid);
        assert bottomSheetContentProvider != null;
        assert manager != null;

        return bottomSheetContentProvider.createPeekViewManager(
                manager,
                mProfileSupplier,
                mActivityTabProvider,
                (tabId) -> {
                    TabModelSelector selector = mTabModelSelectorSupplier.get();
                    if (selector != null) {
                        TabModelUtils.selectTabById(selector, tabId, TabSelectionType.FROM_USER);
                    }
                });
    }
}
