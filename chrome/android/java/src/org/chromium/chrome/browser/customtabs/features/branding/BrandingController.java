// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import android.os.SystemClock;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackController;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Controls the strategy to start branding, and the duration to show branding.
 */
public class BrandingController {
    // The maximum time allowed from CCT Toolbar initialized until it should show the URL and title.
    @VisibleForTesting
    static final int TOTAL_BRANDING_DELAY_MS = 1800;

    private final CallbackController mCallbackController = new CallbackController();

    private ToolbarBrandingDelegate mToolbarBrandingDelegate;
    private long mToolbarInitializedTime;
    private boolean mIsBrandingShowing;

    /**
     * Register the {@link ToolbarBrandingDelegate} from CCT Toolbar.
     * @param delegate {@link ToolbarBrandingDelegate} instance from CCT Toolbar.
     */
    public void onToolbarInitialized(@NonNull ToolbarBrandingDelegate delegate) {
        mToolbarInitializedTime = SystemClock.elapsedRealtime();
        mToolbarBrandingDelegate = delegate;

        // Set location bar to empty as controller is waiting for picking the strategy to use.
        // This should not cause any UI jank even if a decision is made immediately, as
        // state set in CustomTabToolbar#showEmptyLocationBar should be unset in any newer state.
        // TODO(https://crrev.com/c/3770803): Provide more context in java doc when this state
        // becomes more useful.
        mToolbarBrandingDelegate.showEmptyLocationBar();

        maybeMakeBrandingDecision();
    }

    // Make a collective decision with different signal collected from controller.
    private void maybeMakeBrandingDecision() {
        long delay = SystemClock.elapsedRealtime() - mToolbarInitializedTime;
        long duration = TOTAL_BRANDING_DELAY_MS - delay;

        showToolbarBranding(duration);
    }

    private void showToolbarBranding(long durationMs) {
        mIsBrandingShowing = true;
        mToolbarBrandingDelegate.showBrandingLocationBar();

        Runnable hideToolbarBranding = () -> {
            mIsBrandingShowing = false;
            mToolbarBrandingDelegate.showRegularToolbar();
        };
        PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT,
                mCallbackController.makeCancelable(hideToolbarBranding), durationMs);
    }

    /** Destroy this instance an cancel all scheduled callbacks */
    public void destroy() {
        mCallbackController.destroy();
    }

    @VisibleForTesting
    public boolean getIsBrandingShowing() {
        return mIsBrandingShowing;
    }
}
