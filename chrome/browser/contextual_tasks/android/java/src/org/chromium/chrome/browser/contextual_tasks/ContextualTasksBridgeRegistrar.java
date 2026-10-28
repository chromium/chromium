// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.browser_window.AndroidBrowserWindowObserver;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeatureKey;
import org.chromium.ui.base.ActivityWindowAndroid;

/**
 * Registers a {@link ContextualTasksBridge} for each browser window in an Activity. An Activity can
 * host a regular and an incognito BrowserWindowInterface, and each bridge serves exactly one
 * window. Incognito bridges are only registered when kAllowSignedOutUserInDesktopAndroid is
 * enabled. Bridges are owned and torn down by ChromeAndroidTask.
 */
@NullMarked
public class ContextualTasksBridgeRegistrar {
    private final ChromeAndroidTask mChromeAndroidTask;
    private final ActivityWindowAndroid mWindowAndroid;
    private final TabModelSelector mTabModelSelector;
    private final AndroidBrowserWindowObserver mBrowserWindowObserver =
            new AndroidBrowserWindowObserver() {
                @Override
                public void onBrowserWindowAdded(AndroidBrowserWindowInfo windowInfo) {
                    handleBrowserWindowAdded(
                            windowInfo.mProfile, windowInfo.mActivityWindowAndroid);
                }

                @Override
                public void onBrowserWindowRemoved(AndroidBrowserWindowInfo windowInfo) {
                    // No-op: ChromeAndroidTask removes the bridge feature before destroying the
                    // window.
                }
            };

    /**
     * @param chromeAndroidTask The ChromeAndroidTask hosting the Activity.
     * @param windowAndroid The ActivityWindowAndroid of the Activity.
     * @param tabModelSelector The TabModelSelector of the Activity.
     */
    public ContextualTasksBridgeRegistrar(
            ChromeAndroidTask chromeAndroidTask,
            ActivityWindowAndroid windowAndroid,
            TabModelSelector tabModelSelector) {
        mChromeAndroidTask = chromeAndroidTask;
        mWindowAndroid = windowAndroid;
        mTabModelSelector = tabModelSelector;
    }

    /** Registers bridges for existing windows and observes windows added later. */
    public void init() {
        for (Profile profile : mChromeAndroidTask.getValidProfilesForActivity(mWindowAndroid)) {
            maybeAddBridge(profile);
        }
        mChromeAndroidTask.addAndroidBrowserWindowObserver(mBrowserWindowObserver);
    }

    /** Stops observing windows. */
    public void destroy() {
        mChromeAndroidTask.removeAndroidBrowserWindowObserver(mBrowserWindowObserver);
    }

    /**
     * Registers a bridge for an added window if this Activity hosts it.
     *
     * @param profile The Profile of the added window.
     * @param activityWindowAndroid The ActivityWindowAndroid hosting the added window.
     */
    @VisibleForTesting
    void handleBrowserWindowAdded(
            Profile profile, @Nullable ActivityWindowAndroid activityWindowAndroid) {
        if (activityWindowAndroid != mWindowAndroid) return;
        maybeAddBridge(profile);
    }

    private void maybeAddBridge(Profile profile) {
        boolean isOffTheRecord = profile.isOffTheRecord();
        if (isOffTheRecord && !ContextualTasksUtils.isAllowSignedOutUserInDesktopAndroidEnabled()) {
            return;
        }

        // Include the TabModel in the key so that the incognito bridge is removed when the
        // incognito TabModel becomes empty, before its window is destroyed.
        mChromeAndroidTask.addFeature(
                new ChromeAndroidTaskFeatureKey(
                        ContextualTasksBridge.class,
                        profile,
                        mWindowAndroid,
                        isOffTheRecord ? mTabModelSelector.getModel(/* incognito= */ true) : null),
                () -> new ContextualTasksBridge(profile, mWindowAndroid));
    }
}
