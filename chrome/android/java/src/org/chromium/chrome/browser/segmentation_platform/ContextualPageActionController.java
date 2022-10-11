// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import org.chromium.base.Callback;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.segmentation_platform.SegmentSelectionResult;
import org.chromium.url.GURL;

/**
 * Central class for contextual page actions bridging between UI and backend. Registers itself with
 * segmentation platform for on-demand model execution on page load triggers. Provides updated
 * button data to the toolbar when asked for it.
 */
public class ContextualPageActionController {
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final ObservableSupplier<Tab> mTabSupplier;
    private final Supplier<ShoppingService> mShoppingServiceSupplier;
    private final Supplier<BookmarkModel> mBookmarkModelSupplier;
    private final AdaptiveToolbarButtonController mAdaptiveToolbarButtonController;
    private CurrentTabObserver mCurrentTabObserver;

    /**
     * Constructor.
     * @param profileSupplier The supplier for current profile.
     * @param tabSupplier The supplier of the current tab.
     * @param adaptiveToolbarButtonController The {@link AdaptiveToolbarButtonController} that
     *         handles the logic to decide between multiple buttons to show.
     */
    public ContextualPageActionController(ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<Tab> tabSupplier,
            AdaptiveToolbarButtonController adaptiveToolbarButtonController,
            Supplier<ShoppingService> shoppingServiceSupplier,
            Supplier<BookmarkModel> bookmarkModelSupplier) {
        mProfileSupplier = profileSupplier;
        mTabSupplier = tabSupplier;
        mShoppingServiceSupplier = shoppingServiceSupplier;
        mBookmarkModelSupplier = bookmarkModelSupplier;
        mAdaptiveToolbarButtonController = adaptiveToolbarButtonController;
        profileSupplier.addObserver(profile -> {
            if (profile.isOffTheRecord()) return;

            // The profile supplier observer will be invoked every time the profile is changed.
            // Ignore the subsequent calls since we are only interested in initializing tab
            // observers once.
            if (mCurrentTabObserver != null) return;

            if (!AdaptiveToolbarFeatures.isContextualPageActionsEnabled()) return;

            // TODO(shaktisahu): Observe the right method to handle tab switch, same-page
            // navigations. Also handle chrome:// URLs if not already handled.
            mCurrentTabObserver = new CurrentTabObserver(tabSupplier, new EmptyTabObserver() {
                @Override
                public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                    if (tab != null) maybeShowContextualPageAction();
                }
            }, this::activeTabChanged);
        });
    }

    /** Called on destroy. */
    public void destroy() {
        if (mCurrentTabObserver != null) mCurrentTabObserver.destroy();
    }

    private void activeTabChanged(Tab tab) {
        // If the tab is loading or if it's going to load later then we'll also get a call to
        // onPageLoadFinished.
        if (tab != null && !tab.isLoading() && !tab.isFrozen()) {
            maybeShowContextualPageAction();
        }
    }

    private void maybeShowContextualPageAction() {
        Tab tab = mTabSupplier.get();
        if (tab == null || tab.isIncognito() || tab.isDestroyed()) {
            // On incognito tabs revert back to static action.
            mAdaptiveToolbarButtonController.showDynamicAction(
                    AdaptiveToolbarButtonVariant.UNKNOWN);
            return;
        }
        collectSignals(tab);
    }

    private void collectSignals(Tab tab) {
        final BookmarkModel bookmarkModel = mBookmarkModelSupplier.get();
        bookmarkModel.finishLoadingBookmarkModel(() -> {
            BookmarkId bookmarkId = bookmarkModel.getUserBookmarkIdForTab(tab);
            boolean isAlreadyPriceTracked =
                    PowerBookmarkUtils.isBookmarkPriceTracked(bookmarkModel, bookmarkId);
            if (isAlreadyPriceTracked) {
                findBestAction(tab, /*canTrackPrice=*/false);
            } else {
                mShoppingServiceSupplier.get().getProductInfoForUrl(tab.getUrl(), (url, info) -> {
                    boolean canTrackPrice = info != null;
                    findBestAction(tab, canTrackPrice);
                });
            }
        });
    }

    private void findBestAction(Tab tab, boolean canTrackPrice) {
        ContextualPageActionControllerJni.get().computeContextualPageAction(
                mProfileSupplier.get(), tab.getUrl(), canTrackPrice, result -> {
                    if (tab.isDestroyed()) return;

                    boolean isSameTab =
                            mTabSupplier.get() != null && mTabSupplier.get().getId() == tab.getId();
                    if (!isSameTab) return;

                    if (!AdaptiveToolbarFeatures.isContextualPageActionUiEnabled()) return;
                    mAdaptiveToolbarButtonController.showDynamicAction(
                            AdaptiveToolbarStatePredictor
                                    .getAdaptiveToolbarButtonVariantFromSegmentId(
                                            result.selectedSegment));
                });
    }

    @NativeMethods
    interface Natives {
        void computeContextualPageAction(Profile profile, GURL url, boolean canTrackPrice,
                Callback<SegmentSelectionResult> callback);
    }
}
