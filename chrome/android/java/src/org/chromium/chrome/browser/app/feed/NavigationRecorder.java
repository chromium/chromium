// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.feed;

import android.os.SystemClock;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
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

/**
 * Records stats related to a page visit, such as the time spent on the website, or if the user
 * comes back to the starting point. Use through {@link #record(Tab, Profile, int)}.
 */
@JNINamespace("feed::android")
@NullMarked
public class NavigationRecorder extends EmptyTabObserver {
    private @Nullable Profile mProfile;
    private final int mSurfaceId;

    private final @Nullable WebContentsObserver mWebContentsObserver;

    private long mStartTimeMs;

    /**
     * Sets up visit recording for the provided tab.
     *
     * @param tab Tab where the visit should be recorded
     * @param profile The current profile instance.
     * @param surfaceId The ID of the Feeds surface.
     */
    public static void record(Tab tab, Profile profile, int surfaceId) {
        tab.addObserver(new NavigationRecorder(tab, profile, surfaceId));
    }

    /** Private because users should not hold to references to the instance. */
    private NavigationRecorder(final Tab tab, Profile profile, int surfaceId) {
        mProfile = profile;
        mSurfaceId = surfaceId;

        // onLoadUrl below covers many exit conditions to stop recording but not all,
        // such as navigating back. We therefore stop recording if a navigation stack change
        // indicates we are back to our starting point.
        WebContents webContents = tab.getWebContents();
        if (webContents != null) {
            // if no WebContents is available now, the navigation has been started in a new tab.
            // Svelte devices do not start loading until we switch to the new tab, see
            // https://crbug.com/41335796. In that case, closing or moving away will be the end
            // trigger anyway, no need to care about the navigation stack.
            final NavigationController navController = webContents.getNavigationController();
            int startStackIndex = navController.getLastCommittedEntryIndex();
            mWebContentsObserver =
                    new WebContentsObserver(webContents) {
                        @Override
                        public void navigationEntryCommitted(LoadCommittedDetails details) {
                            if (startStackIndex != navController.getLastCommittedEntryIndex()) {
                                return;
                            }
                            endRecording(tab);
                        }
                    };
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
        endRecording(tab);
    }

    @Override
    public void onDestroyed(Tab tab) {
        endRecording(tab);
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

        if ((params.getTransitionType() & transitionTypeMask) != 0) endRecording(tab);
    }

    private void endRecording(@Nullable Tab removeObserverFromTab) {
        if (removeObserverFromTab != null) {
            removeObserverFromTab.removeObserver(this);
            if (mWebContentsObserver != null) {
                mWebContentsObserver.observe(null);
            }
        }

        if (mProfile != null) {
            long visitTimeMs = mStartTimeMs == 0 ? 0 : SystemClock.elapsedRealtime() - mStartTimeMs;
            NavigationRecorderJni.get().reportOpenVisitComplete(mProfile, mSurfaceId, visitTimeMs);
            mProfile = null;
        }
    }

    @NativeMethods
    public interface Natives {
        void reportOpenVisitComplete(
                @JniType("Profile*") Profile profile, int surfaceId, long visitTimeMs);
    }
}
