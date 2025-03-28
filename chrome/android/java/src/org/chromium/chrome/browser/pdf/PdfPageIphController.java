// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

// TODO(crbug.com/405813894): Placeholder class to be implemented.
/** Controller to manage PDF page in-product-help messages to users. */
public class PdfPageIphController {
    private final WindowAndroid mWindowAndroid;
    private final ActivityTabProvider mActivityTabProvider;
    private ActivityTabTabObserver mActivityTabTabObserver;

    /**
     * Creates and initializes the controller. Registers an {@link ActivityTabTabObserver} that
     * attempts to show the pdf download IPH when the download button is not in the omnibox.
     *
     * @param windowAndroid The window associated with the activity.
     * @param activityTabProvider The provider of the current activity tab.
     */
    public static PdfPageIphController create(
            WindowAndroid windowAndroid, ActivityTabProvider activityTabProvider) {
        return new PdfPageIphController(windowAndroid, activityTabProvider);
    }

    PdfPageIphController(WindowAndroid windowAndroid, ActivityTabProvider activityTabProvider) {
        mWindowAndroid = windowAndroid;
        mActivityTabProvider = activityTabProvider;
        createActivityTabTabObserver();
    }

    public void destroy() {
        if (mActivityTabTabObserver != null) {
            mActivityTabTabObserver.destroy();
        }
    }

    private void createActivityTabTabObserver() {
        mActivityTabTabObserver =
                new ActivityTabTabObserver(mActivityTabProvider) {
                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        if (tab == null || !tab.isNativePage() || !tab.getNativePage().isPdf()) {
                            return;
                        }
                        showDownloadIph();
                    }
                };
    }

    private void showDownloadIph() {
        @SuppressWarnings("UnusedVariable")
        int highlightMenuItemId;
        if (DeviceFormFactor.isWindowOnTablet(mWindowAndroid)) {
            highlightMenuItemId = R.id.download_page_id;
        } else {
            highlightMenuItemId = R.id.offline_page_id;
        }

        @SuppressWarnings("UnusedVariable")
        int textId = R.string.pdf_page_download_iph_text;
    }
}
