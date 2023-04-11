// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;

/** Interface for Feed actions implemented by the Browser.*/
public interface FeedActionDelegate {
    /** Information about a page visit. */
    public class VisitResult {
        // Total page visit time.
        public long visitTimeMs;
    }

    /**
     * Opens a url that was presented to the user as suggested content.
     * @param disposition A `org.chromium.ui.mojom.WindowOpenDisposition` value.
     * @param params What to load.
     * @param inGroup Whether to open the url in a tab in group.
     * @param onPageLoaded Called when the page completes loading.
     * @param onVisitComplete Called when the user closes or navigates away from the page.
     */
    void openSuggestionUrl(int disposition, LoadUrlParams params, boolean inGroup,
            Runnable onPageLoaded, Callback<VisitResult> onVisitComplete);

    /**
     * Opens a page.
     * @param disposition A `org.chromium.ui.mojom.WindowOpenDisposition` value.
     * @param params What to load.
     */
    default void openUrl(int disposition, LoadUrlParams params) {}

    /**
     * Downloads a web page.
     * @param url url of the page to download.
     */
    default void downloadPage(String url) {}

    /**
     * Opens the NTP help page.
     */
    default void openHelpPage() {}

    /**
     * Add an item to the reading list.
     */
    default void addToReadingList(String title, String url) {}

    /**
     * Opens a specific WebFeed by name.
     * @param webFeedName the relevant web feed name.
     */
    default void openWebFeed(String webFeedName, @SingleWebFeedEntryPoint int entryPoint) {}

    //
    // Optional methods for handing events.
    //

    /**
     * Informs that content on the Feed has changed.
     */
    default void onContentsChanged() {}

    /**
     * Informs that the stream was created.
     */
    default void onStreamCreated() {}

    /**
     * Shows a sign in activity as a result of a feed user action.
     * @param signinAccessPoint the entry point for the signin.
     */
    default void showSyncConsentActivity(@SigninAccessPoint int signinAccessPoint) {}

    /**
     * Shows a sign in interstitial as a result of a feed user action.
     * @param signinAccessPoint the entry point for the signin.
     * @param mBottomSheetController bottomsheet controller attached to the activity.
     * @param mWindowAndroid window used by the feed.
     */
    default void showSignInInterstitial(@SigninAccessPoint int signinAccessPoint,
            BottomSheetController mBottomSheetController, WindowAndroid mWindowAndroid) {}
}
