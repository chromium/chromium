// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import org.jni_zero.CalledByNative;

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
    private final FullscreenManager mFullscreenManager;
    private final ActivityTabProvider.ActivityTabTabObserver mActiveTabObserver;
    @Nullable private Tab mActiveTab;

    @CalledByNative
    public static ExclusiveAccessContext create(
            FullscreenManager fullscreenManager, ActivityTabProvider activityTabProvider) {
        return new ExclusiveAccessContext(fullscreenManager, activityTabProvider);
    }

    public ExclusiveAccessContext(
            FullscreenManager fullscreenManager, ActivityTabProvider activityTabProvider) {
        mFullscreenManager = fullscreenManager;
        mActiveTabObserver =
                new ActivityTabProvider.ActivityTabTabObserver(activityTabProvider) {
                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        mActiveTab = tab;
                    }
                };
    }

    @Override
    @CalledByNative
    public void destroy() {
        mActiveTabObserver.destroy();
    }

    @CalledByNative
    public @Nullable Profile getProfile() {
        return mActiveTab != null ? mActiveTab.getProfile() : null;
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
    public void enterFullscreenModeForTab() {
        if (mActiveTab != null) {
            mFullscreenManager.onEnterFullscreen(mActiveTab, new FullscreenOptions(false, false));
        }
    }

    @CalledByNative
    public void exitFullscreenModeForTab() {
        if (mActiveTab != null) {
            mFullscreenManager.onExitFullscreen(mActiveTab);
        }
    }
}
