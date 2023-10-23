// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import android.app.Activity;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.RequestCoordinatorBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.document.ChromeAsyncTabLauncher;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;

/**
 * Extension of {@link SuggestionsNavigationDelegate} with specific methods of MV tiles on Start
 * Surface.
 */
public class MostVisitedTileNavigationDelegate extends SuggestionsNavigationDelegate {
    private final Supplier<Tab> mParentTabSupplier;
    private final ChromeAsyncTabLauncher mChromeAsyncTabLauncher;

    /**
     * Creates a new {@link MostVisitedTileNavigationDelegate}.
     * @param activity The Android activity.
     * @param profile The currently applicable profile.
     * @param parentTabSupplier Supplies the StartSurface's parent tab.
     */
    public MostVisitedTileNavigationDelegate(
            Activity activity, Profile profile, Supplier<Tab> parentTabSupplier) {
        super(activity, profile, /*host=*/null, /*tabModelSelector=*/null, /*tab=*/null);
        mParentTabSupplier = parentTabSupplier;
        mChromeAsyncTabLauncher = new ChromeAsyncTabLauncher(false);
    }

    @Override
    public boolean isOpenInNewTabInGroupEnabled() {
        return false;
    }

    /**
     * Opens the suggestions page without recording metrics.
     *
     * @param windowOpenDisposition How to open (new window, current tab, etc).
     * @param url The url to navigate to.
     */
    @Override
    public void navigateToSuggestionUrl(int windowOpenDisposition, String url, boolean inGroup) {
        assert !inGroup;
        switch (windowOpenDisposition) {
            case WindowOpenDisposition.CURRENT_TAB:
            case WindowOpenDisposition.NEW_BACKGROUND_TAB:
                ReturnToChromeUtil.handleLoadUrlFromStartSurface(
                        new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK),
                        windowOpenDisposition
                                == org.chromium.ui.mojom.WindowOpenDisposition.NEW_BACKGROUND_TAB,
                        /*incognito=*/null, mParentTabSupplier.get());
                break;
            case WindowOpenDisposition.OFF_THE_RECORD:
                ReturnToChromeUtil.handleLoadUrlFromStartSurface(
                        new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK), true /*incognito*/,
                        mParentTabSupplier.get());
                break;
            case WindowOpenDisposition.NEW_WINDOW:
                openUrlInNewWindow(new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK));
                break;
            case WindowOpenDisposition.SAVE_TO_DISK:
                // TODO(crbug.com/1202321): Downloading toast is not shown maybe due to the
                // webContent is null for start surface.
                saveUrlForOffline(url);
                break;
            default:
                assert false;
        }
    }

    private void saveUrlForOffline(String url) {
        // TODO(crbug.com/1193816): Namespace shouldn't be NTP_SUGGESTIONS_NAMESPACE since it's
        // not on NTP.
        RequestCoordinatorBridge.getForProfile(Profile.getLastUsedRegularProfile())
                .savePageLater(
                        url, OfflinePageBridge.NTP_SUGGESTIONS_NAMESPACE, true /* userRequested */);
    }

    private void openUrlInNewWindow(LoadUrlParams loadUrlParams) {
        mChromeAsyncTabLauncher.launchTabInOtherWindow(
                loadUrlParams,
                mActivity,
                mParentTabSupplier.get() == null ? -1 : mParentTabSupplier.get().getId(),
                MultiWindowUtils.getAdjacentWindowActivity(mActivity));
    }
}
