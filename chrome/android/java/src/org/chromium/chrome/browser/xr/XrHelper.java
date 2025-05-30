// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr;

import android.app.Activity;
import android.os.Build;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;
import androidx.xr.scenecore.impl.JxrPlatformAdapterAxr;

import org.chromium.base.Log;
import org.chromium.base.task.ChromiumExecutorServiceFactory;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.util.XrUtils;

/** A singleton utility class to manages XR session and UI environment. */
@NullMarked
public class XrHelper {
    private static final String TAG = "XrHelper";

    // For spatialization of Chrome app using Jetpack XR.
    private @Nullable JxrPlatformAdapterAxr mJxrPlatformAdapter;
    private boolean mModeSwitchInProgress;

    /**
     * Initialize the class and store info that will be used during spatialization and maintaining
     * viewing state.
     */
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public XrHelper(@NonNull Activity activity) {
        // Note: The activity/window handle is only used in the function during initialization of XR
        // and not saved in the class to prevent activity leaks.
        if (!XrUtils.isXrDevice()) return;

        // Initialization of XR for spatialization will occur here using JXR.
        mJxrPlatformAdapter = createJxrPlatformAdapter(activity);
        if (mJxrPlatformAdapter == null) return;
        mJxrPlatformAdapter
                .getActivitySpace()
                .addOnBoundsChangedListener(
                        dimensions -> {
                            if (mJxrPlatformAdapter == null) return;

                            if (mModeSwitchInProgress) {
                                mModeSwitchInProgress = false;
                                Log.i(TAG, "SPA completed switch to FSM/HSM");
                                mJxrPlatformAdapter.getMainPanelEntity().setHidden(false);
                            }
                        });
    }

    /** Reset the class and clear the JXR session. */
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void reset() {
        if (!XrUtils.isXrDevice()) return;
        if (mJxrPlatformAdapter != null) {
            mJxrPlatformAdapter.dispose();
            mJxrPlatformAdapter = null;
        }
    }

    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    private JxrPlatformAdapterAxr createJxrPlatformAdapter(@NonNull Activity activity) {
        return JxrPlatformAdapterAxr.create(
                activity,
                ChromiumExecutorServiceFactory.create(TaskTraits.BEST_EFFORT_MAY_BLOCK),
                /* useSplitEngine= */ false);
    }

    /**
     * Initialize viewing of the XR environment in the full space mode in which only the single
     * activity is visible to the user and all other activities are hidden out.
     */
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void viewInFullSpaceMode() {
        if (mJxrPlatformAdapter == null) return;
        // Requesting of full space mode using JXR.
        mModeSwitchInProgress = true;
        Log.i(TAG, "SPA requesting FullSpaceMode");
        XrUtils.getInstance().setFullSpaceMode(true);
        mJxrPlatformAdapter.requestFullSpaceMode();
        mJxrPlatformAdapter.getMainPanelEntity().setHidden(true);
    }

    /**
     * Initiate viewing of the XR environment in the default home space mode in which all open
     * activity is visible to the user similar to desktop environment.
     */
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void viewInHomeSpaceMode() {
        if (mJxrPlatformAdapter == null) return;

        // Requesting return to home space mode using JXR.
        mModeSwitchInProgress = true;
        Log.i(TAG, "SPA requesting HomeSpaceMode");
        mJxrPlatformAdapter.requestHomeSpaceMode();
        XrUtils.getInstance().setFullSpaceMode(false);
        mJxrPlatformAdapter.getMainPanelEntity().setHidden(true);
    }

    boolean isXrInitializedForTesting() {
        return mJxrPlatformAdapter != null;
    }
}
