// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Build;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.xr.runtime.internal.ActivitySpace;
import androidx.xr.runtime.internal.Dimensions;
import androidx.xr.scenecore.impl.JxrPlatformAdapterAxr;

import org.chromium.base.CallbackController;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.ChromiumExecutorServiceFactory;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.util.XrUtils;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;

/**
 * The class wraps usage of {@link androidx.xr.scenecore.impl.JxrPlatformAdapterAxr} and implements
 * {@link XrSceneCoreSessionManager}.
 */
@SuppressLint("RestrictedApi")
@RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
@NullMarked
public class XrSceneCoreSessionManagerImpl implements XrSceneCoreSessionManager {
    // TODO(crbug.com/422151641): Figure out how to get rid of timing delay and use event
    //  or callback instead, when switching XR space modes.
    // The delay is based on observations to let UI redraw before XR space mode switching.
    private static final int XR_MODE_UI_UPDATE_DELAY_MS = 250;
    private JxrPlatformAdapterAxr mJxrPlatformAdapter;
    private Activity mActivity;
    private CallbackController mCallbackController = new CallbackController();

    // If not null, a request to change XR space mode is in progress.
    private @Nullable Boolean mIsFsmRequested;
    private @Nullable Runnable mXrModeSwitchCallback;
    private final ObservableSupplierImpl<Boolean> mIsFullSpaceModeNowSupplier =
            new ObservableSupplierImpl<>(false); // Home space mode on startup.
    private final ActivitySpace.OnBoundsChangedListener mBoundsChangedListener =
            this::boundsChangeCallback;

    public XrSceneCoreSessionManagerImpl(Activity activity) {
        // TODO(crbug.com/422134376): To detect "Android XR" query OS instead of device's
        // properties.
        assert XrUtils.isXrDevice();
        mActivity = activity;
        mJxrPlatformAdapter = createJxrPlatformAdapter(mActivity);
        mJxrPlatformAdapter.getActivitySpace().addOnBoundsChangedListener(mBoundsChangedListener);
    }

    private JxrPlatformAdapterAxr createJxrPlatformAdapter(Activity activity) {
        return JxrPlatformAdapterAxr.create(
                activity,
                ChromiumExecutorServiceFactory.create(TaskTraits.BEST_EFFORT_MAY_BLOCK),
                false);
    }

    @MainThread
    @Override
    public boolean startSpaceModeChange(boolean fsmModeRequested, Runnable completedCallback) {
        return trySpaceModeChange(fsmModeRequested, completedCallback);
    }

    @MainThread
    @Override
    public boolean requestSpaceModeChange(boolean fsmModeRequested) {
        return trySpaceModeChange(fsmModeRequested, /* completedCallback= */ null);
    }

    @MainThread
    private boolean trySpaceModeChange(
            boolean fsmModeRequested, @Nullable Runnable completedCallback) {
        ThreadUtils.assertOnUiThread();
        if (!ThreadUtils.runningOnUiThread()) return false;

        // Decline if the request to change XR space mode is being processed.
        if (mIsFsmRequested != null) {
            return false;
        }

        // Do nothing if the activity doesn't have focus or the requested XR
        // space mode is already set.
        if (!mActivity.hasWindowFocus()
                || assumeNonNull(mIsFullSpaceModeNowSupplier.get()).equals(fsmModeRequested)) {
            return false;
        }

        // Hide the main panel only if callback is requested.
        // The main panel will be set visible by {@link
        // XrSceneCoreSessionManagerImpl#finishSpaceModeChange}.
        if (completedCallback != null) {
            mIsFsmRequested = fsmModeRequested;
            mXrModeSwitchCallback = completedCallback;
            mJxrPlatformAdapter.getMainPanelEntity().setHidden(true);
        }

        if (fsmModeRequested) {
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

    @MainThread
    @Override
    public void finishSpaceModeChange() {
        ThreadUtils.assertOnUiThread();
        if (!ThreadUtils.runningOnUiThread()) return;

        if (mIsFsmRequested != null) {
            mJxrPlatformAdapter.getMainPanelEntity().setHidden(false);
            mIsFsmRequested = null;
        }
    }

    @SuppressWarnings("NullAway")
    @Override
    public void destroy() {
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }

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
        if (mIsFsmRequested == null) {
            // This is an initial callback on `addOnBoundsChangedListener` or
            // callback on `requestSpaceModeChange`.
            return;
        }

        if (!assumeNonNull(mIsFullSpaceModeNowSupplier.get()).equals(mIsFsmRequested)) {
            // Leave if this is not XR space mode we expect.
            return;
        }

        ThreadUtils.postOnUiThreadDelayed(
                mCallbackController.makeCancelable(
                        () -> {
                            if (mXrModeSwitchCallback != null) {
                                mXrModeSwitchCallback.run();
                                mXrModeSwitchCallback = null;
                            }
                        }),
                Boolean.TRUE.equals(mIsFullSpaceModeNowSupplier.get())
                        ? XR_MODE_UI_UPDATE_DELAY_MS
                        : 0);
    }
}
