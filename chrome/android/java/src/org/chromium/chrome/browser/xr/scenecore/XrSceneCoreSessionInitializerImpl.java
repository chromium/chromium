// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.WindowFocusChangedObserver;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionInitializer;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;

/** This class implements {@link XrSceneCoreSessionInitializer} interface. */
@NullMarked
public class XrSceneCoreSessionInitializerImpl
        implements XrSceneCoreSessionInitializer, WindowFocusChangedObserver {
    private @Nullable ActivityLifecycleDispatcher mLifecycleDispatcher;
    private @Nullable XrSceneCoreSessionManager mXrSceneCoreSessionManager;
    private @Nullable Boolean mInitialFullSpaceMode;

    /**
     * Create an instance of {@link XrSceneCoreSessionInitializerImpl}.
     *
     * @param lifecycleDispatcher The {@link ActivityLifecycleDispatcher} instance to allow
     *     listening for the window focus events.
     * @param xrSceneCoreSessionManager The {@link XrSceneCoreSessionManager} to switch between
     *     space modes on XR.
     */
    public XrSceneCoreSessionInitializerImpl(
            ActivityLifecycleDispatcher lifecycleDispatcher,
            XrSceneCoreSessionManager xrSceneCoreSessionManager) {
        mLifecycleDispatcher = lifecycleDispatcher;
        mXrSceneCoreSessionManager = xrSceneCoreSessionManager;
    }

    private boolean inDesiredXrSpaceMode() {
        return assumeNonNull(mInitialFullSpaceMode).booleanValue()
                == assumeNonNull(mXrSceneCoreSessionManager).isXrFullSpaceMode();
    }

    @Override
    public void initialize(boolean isFullSpaceMode) {
        assert mInitialFullSpaceMode == null
                : "XrSceneCoreSessionInitializerImpl is already initialized.";
        assert mLifecycleDispatcher != null && mXrSceneCoreSessionManager != null
                : "XrSceneCoreSessionInitializerImpl was already destroyed.";

        mInitialFullSpaceMode = isFullSpaceMode;

        // Check if the activity is in desired XR space mode and request mode switch if it is not.
        // This can happen when the activity gets recreated because of the
        // Activity#onConfigurationChanged while being in a non-default mode.
        if (!inDesiredXrSpaceMode()) {
            if (!mXrSceneCoreSessionManager.requestSpaceModeChange(false)) {
                // If unable to switch back to the Home Space mode due to the focus issue -
                // add a focus listener and try again when the app window receives focus.
                mLifecycleDispatcher.register(this);
            }
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        if (hasFocus) {
            if (!inDesiredXrSpaceMode()) {
                assumeNonNull(mXrSceneCoreSessionManager).requestSpaceModeChange(false);
            }
            assumeNonNull(mLifecycleDispatcher).unregister(this);
        }
    }

    @Override
    public void destroy() {
        if (mLifecycleDispatcher != null) {
            mLifecycleDispatcher.unregister(this);
        }
        mInitialFullSpaceMode = null;
        mXrSceneCoreSessionManager = null;
        mLifecycleDispatcher = null;
    }
}
