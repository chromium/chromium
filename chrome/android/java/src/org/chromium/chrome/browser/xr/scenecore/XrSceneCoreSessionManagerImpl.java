// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Build;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.xr.runtime.internal.ActivitySpace;
import androidx.xr.runtime.internal.Dimensions;
import androidx.xr.scenecore.impl.JxrPlatformAdapterAxr;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.ChromiumExecutorServiceFactory;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;

/**
 * The class wraps usage of {@link androidx.xr.scenecore.impl.JxrPlatformAdapterAxr} and implements
 * {@link XrSceneCoreSessionManager}.
 */
@SuppressLint("RestrictedApi")
@RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
@NullMarked
public class XrSceneCoreSessionManagerImpl implements XrSceneCoreSessionManager {
    private JxrPlatformAdapterAxr mJxrPlatformAdapter;
    private Activity mActivity;

    // If not null, a request to change XR space mode is in progress.
    private @Nullable Boolean mIsFullSpaceModeRequested;
    private @Nullable Runnable mXrModeSwitchCallback;
    private final ObservableSupplierImpl<Boolean> mIsFullSpaceModeNowSupplier;
    private final ActivitySpace.OnBoundsChangedListener mBoundsChangedListener =
            this::boundsChangeCallback;

    public XrSceneCoreSessionManagerImpl(Activity activity) {
        assert DeviceInfo.isXr();
        mActivity = activity;
        mJxrPlatformAdapter = createJxrPlatformAdapter(mActivity);
        mJxrPlatformAdapter.getActivitySpace().addOnBoundsChangedListener(mBoundsChangedListener);

        // Initialize the supplier with the current mode.
        boolean isXrFullSpaceMode =
                mJxrPlatformAdapter.getActivitySpace().getBounds().width == Float.POSITIVE_INFINITY;
        mIsFullSpaceModeNowSupplier = new ObservableSupplierImpl<>(isXrFullSpaceMode);
    }

    private JxrPlatformAdapterAxr createJxrPlatformAdapter(Activity activity) {
        return JxrPlatformAdapterAxr.create(
                activity,
                ChromiumExecutorServiceFactory.create(TaskTraits.BEST_EFFORT_MAY_BLOCK),
                false);
    }

    @MainThread
    @Override
    public boolean requestSpaceModeChange(
            boolean requestFullSpaceMode, Runnable completedCallback) {
        return requestSpaceModeChangeInternal(requestFullSpaceMode, completedCallback);
    }

    @MainThread
    @Override
    public boolean requestSpaceModeChange(boolean requestFullSpaceMode) {
        return requestSpaceModeChangeInternal(requestFullSpaceMode, /* completedCallback= */ null);
    }

    @MainThread
    private boolean requestSpaceModeChangeInternal(
            boolean requestFullSpaceMode, @Nullable Runnable completedCallback) {
        ThreadUtils.assertOnUiThread();
        if (!ThreadUtils.runningOnUiThread()) return false;

        // Decline if the request to change XR space mode is being processed.
        if (mIsFullSpaceModeRequested != null) {
            return false;
        }

        // Do nothing if the activity doesn't have focus or the requested XR
        // space mode is already set.
        if (!mActivity.hasWindowFocus() || requestFullSpaceMode == isXrFullSpaceMode()) {
            return false;
        }

        mIsFullSpaceModeRequested = requestFullSpaceMode;
        mXrModeSwitchCallback = completedCallback;

        if (requestFullSpaceMode) {
            mJxrPlatformAdapter.requestFullSpaceMode();
        } else {
            mJxrPlatformAdapter.requestHomeSpaceMode();
        }

        return true;
    }

    @Override
    public ObservableSupplier<Boolean> getXrSpaceModeObservableSupplier() {
        return mIsFullSpaceModeNowSupplier;
    }

    @Override
    public boolean isXrFullSpaceMode() {
        return Boolean.TRUE.equals(mIsFullSpaceModeNowSupplier.get());
    }

    @MainThread
    @Override
    public void setMainPanelVisibility(boolean visible) {
        mJxrPlatformAdapter.getMainPanelEntity().setHidden(!visible);
    }

    @SuppressWarnings("NullAway")
    @Override
    public void destroy() {
        if (mJxrPlatformAdapter != null) {
            mJxrPlatformAdapter
                    .getActivitySpace()
                    .removeOnBoundsChangedListener(mBoundsChangedListener);
            mJxrPlatformAdapter.dispose();
            mJxrPlatformAdapter = null;
        }
        mActivity = null;
    }

    private void boundsChangeCallback(Dimensions dimensions) {
        mIsFullSpaceModeNowSupplier.set(dimensions.width == Float.POSITIVE_INFINITY);

        if (mIsFullSpaceModeRequested != null && mIsFullSpaceModeRequested == isXrFullSpaceMode()) {
            // Mark the current request as completed.
            mIsFullSpaceModeRequested = null;
            if (mXrModeSwitchCallback != null) {
                mXrModeSwitchCallback.run();
                mXrModeSwitchCallback = null;
            }
        }
    }
}
