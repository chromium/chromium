// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.feed;

import android.os.SystemClock;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

/**
 * Records stats related to a page visit, such as the time spent on the website, or if the user
 * comes back to the starting point. Use through {@link #record(Tab, Callback)}.
 */
public class NavigationRecorder extends EmptyTabObserver {
    private final Callback<VisitData> mVisitEndCallback;

    @Nullable private final WebContentsObserver mWebContentsObserver;

    private long mStartTimeMs;

    /**
     * Sets up visit recording for the provided tab.
     * @param tab Tab where the visit should be recorded
     * @param visitEndCallback The callback where the visit data is sent when it completes.
     */
    public static void record(Tab tab, Callback<VisitData> visitEndCallback) {
        tab.addObserver(new NavigationRecorder(tab, visitEndCallback));
    }

    /** Private because users should not hold to references to the instance. */
    private NavigationRecorder(final Tab tab, Callback<VisitData> visitEndCallback) {
        mVisitEndCallback = visitEndCallback;

        // onLoadUrl below covers many exit conditions to stop recording but not all,
        // such as navigating back. We therefore stop recording if a navigation stack change
        // indicates we are back to our starting point.
        WebContents webContents = tab.getWebContents();
        if (webContents != null) {
            // if no WebContents is available now, the navigation has been started in a new tab.
            // Svelte devices do not start loading until we switch to the new tab, see
            // https://crbug.com/748916. In that case, closing or moving away will be the end
            // trigger anyway, no need to care about the navigation stack.
            final NavigationController navController = webContents.getNavigationController();
            int startStackIndex = navController.getLastCommittedEntryIndex();
            mWebContentsObserver =
                    new WebContentsObserver() {
                        @Override
                        public void navigationEntryCommitted(LoadCommittedDetails details) {
                            if (startStackIndex != navController.getLastCommittedEntryIndex()) {
                                return;
                            }
                            endRecording(tab, tab.getUrl());
                        }
                    };
            webContents.addObserver(mWebContentsObserver);
        } else {
            mWebContentsObserver = null;
        }

        if (!tab.isHidden()) mStartTimeMs = SystemClock.elapsedRealtime();
    }

    @Override
    public void onShown(Tab tab, @TabSelectionType int type) {
        if (mStartTimeMs == 0) mStartTimeMs = SystemClock.elapsedRealtime();
    }

    @Override
    public void onHidden(Tab tab, @TabHidingType int type) {
        endRecording(tab, null);
    }

    @Override
    public void onDestroyed(Tab tab) {
        endRecording(tab, null);
    }

    @Override
    public void onLoadUrl(Tab tab, LoadUrlParams params, LoadUrlResult loadUrlResult) {
        // End recording if a new URL gets loaded e.g. after entering a new query in
        // the omnibox. This doesn't cover the navigate-back case so we also need to observe
        // changes to WebContent's navigation entries.
        int transitionTypeMask =
                PageTransition.FROM_ADDRESS_BAR
                        | PageTransition.HOME_PAGE
                        | PageTransition.CHAIN_START
                        | PageTransition.CHAIN_END
                        | PageTransition.FROM_API;

        if ((params.getTransitionType() & transitionTypeMask) != 0) endRecording(tab, null);
    }

    private void endRecording(@Nullable Tab removeObserverFromTab, @Nullable GURL endUrl) {
        if (removeObserverFromTab != null) {
            removeObserverFromTab.removeObserver(this);
            if (removeObserverFromTab.getWebContents() != null && mWebContentsObserver != null) {
                removeObserverFromTab.getWebContents().removeObserver(mWebContentsObserver);
            }
        }

        long visitTimeMs = SystemClock.elapsedRealtime() - mStartTimeMs;
        mVisitEndCallback.onResult(new VisitData(visitTimeMs, endUrl));
    }

    /** Plain holder for the data of a recorded visit. */
    public static class VisitData {
        public final long duration;
        public final GURL endUrl;

        public VisitData(long duration, GURL endUrl) {
            this.duration = duration;
            this.endUrl = endUrl;
        }
    }
}
