// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.content.Context;

import org.jni_zero.CalledByNative;

import org.chromium.base.Log;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

/**
 * This class is the Java counterpart of ExclusiveAccessContextAndroid responsible for forwarding
 * WebContents calls to the system. The native class is responsible for Java object creation and
 * destruction.
 */
@NullMarked
public class ExclusiveAccessContext implements Destroyable {
    private static final String TAG = "ExclusiveAccessCtx";

    private final Context mContext;
    private final FullscreenManager mFullscreenManager;
    private final ActivityTabProvider.ActivityTabTabObserver mActiveTabObserver;
    @Nullable private Tab mActiveTab;

    @CalledByNative
    public static ExclusiveAccessContext create(
            Context context,
            FullscreenManager fullscreenManager,
            ActivityTabProvider activityTabProvider) {
        return new ExclusiveAccessContext(context, fullscreenManager, activityTabProvider);
    }

    public ExclusiveAccessContext(
            Context context,
            FullscreenManager fullscreenManager,
            ActivityTabProvider activityTabProvider) {
        mContext = context;
        mFullscreenManager = fullscreenManager;
        mActiveTabObserver =
                new ActivityTabProvider.ActivityTabTabObserver(
                        activityTabProvider, /* shouldTrigger= */ true) {
                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        if (mActiveTab == null || tab == null) {
                            Log.i(TAG, "onObservingDifferentTab is new tab null? " + (tab == null));
                        }

                        mActiveTab = tab;
                    }
                };
    }

    Context getAppContext() {
        return mContext;
    }

    @Override
    @CalledByNative
    public void destroy() {
        mActiveTabObserver.destroy();
    }

    @CalledByNative
    public @Nullable Profile getProfile() {
        if (mActiveTab == null) {
            Log.e(TAG, "mActiveTab is null in getProfile");
            return null;
        }

        return mActiveTab.getProfile();
    }

    @CalledByNative
    public @Nullable WebContents getWebContentsForExclusiveAccess() {
        return mActiveTab != null ? mActiveTab.getWebContents() : null;
    }

    @CalledByNative
    boolean isFullscreen() {
        return mFullscreenManager.getPersistentFullscreenMode();
    }

    @CalledByNative
    public void enterFullscreenModeForTab(
            long displayId, boolean showNavigationBar, boolean showStatusBar) {
        if (mActiveTab != null) {
            mFullscreenManager.onEnterFullscreen(
                    mActiveTab, new FullscreenOptions(showNavigationBar, showStatusBar, displayId));
        }
    }

    @CalledByNative
    public void exitFullscreenModeForTab() {
        if (mActiveTab != null) {
            mFullscreenManager.onExitFullscreen(mActiveTab);
        }
    }

    @CalledByNative
    public void forceActiveTab(Tab tab) {
        mActiveTab = tab;
    }
}
