// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Build;
import android.view.View;

import androidx.annotation.MainThread;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.xr.arcore.ArDevice;
import androidx.xr.runtime.Config;
import androidx.xr.runtime.DeviceTrackingMode;
import androidx.xr.runtime.Session;
import androidx.xr.runtime.SessionCreateResult;
import androidx.xr.runtime.SessionCreateSuccess;
import androidx.xr.runtime.math.FloatSize3d;
import androidx.xr.runtime.math.Pose;
import androidx.xr.scenecore.ActivitySpace;
import androidx.xr.scenecore.BaseEntity;
import androidx.xr.scenecore.Scene;
import androidx.xr.scenecore.SessionExt;

import org.chromium.base.BundleUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TriState;
import org.chromium.base.TriStateUtils;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.xr.scenecore.XrEntityHolder;
import org.chromium.ui.xr.scenecore.XrFactory;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;
import org.chromium.ui.xr.scenecore.XrPixelDensity;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;

import java.util.function.Consumer;

/**
 * The class wraps usage of {@link androidx.xr.runtime.Session} and implements {@link
 * XrSceneCoreSessionManager}.
 */
@SuppressLint("RestrictedApi")
@RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
@NullMarked
public class XrSceneCoreSessionManagerImpl implements XrSceneCoreSessionManager {
    private static final String TAG = "XrSceneCore";
    private static final String MODULE_NAME = "xr";
    // List of native libraries to load for the XR module.
    private static final String[] NATIVE_LIBS = {
        "impress_api_jni",
        "arcore_sdk_c",
        "arcore_sdk_jni",
        "androidx.xr.runtime.openxr",
        "androidx.xr.arcore.openxr",
    };
    private static final Object sLock = new Object();
    private static boolean sLibrariesLoaded;
    private Session mXrSession;
    private Activity mActivity;
    private ActivitySpace mActivitySpace;

    // If not TriState.NOT_SET, a request to change XR space mode is in progress.
    private @TriState int mIsFullSpaceModeRequested;
    private @Nullable Runnable mXrModeSwitchCallback;
    private boolean mIsHeadTrackingEnabled;
    private final XrHeadPoseTracker mHeadPoseTracker;
    private final SettableNullableObservableSupplier<XrPose> mHeadPoseSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNonNullObservableSupplier<Boolean> mIsFullSpaceModeNowSupplier;
    private final Consumer<FloatSize3d> mBoundsChangedListener = this::boundsChangeCallback;
    private final XrPixelDensity mPixelDensity;

    public XrSceneCoreSessionManagerImpl(Activity activity) {
        this(activity, createSession(activity));
    }

    @VisibleForTesting
    public XrSceneCoreSessionManagerImpl(Activity activity, Session session) {
        mActivity = activity;
        mXrSession = session;
        mHeadPoseTracker = new XrHeadPoseTracker(this, mHeadPoseSupplier::set);
        mActivitySpace = getScene().getActivitySpace();
        mActivitySpace.addOnBoundsChangedListener(mBoundsChangedListener);

        boolean isXrFullSpaceMode =
                mActivitySpace.getBounds().getWidth() == Float.POSITIVE_INFINITY;
        mIsFullSpaceModeNowSupplier = ObservableSuppliers.createNonNull(isXrFullSpaceMode);
        mPixelDensity =
                XrPixelDensityImpl.create(activity.getResources().getDisplayMetrics().density);
    }

    private static Session createSession(Activity activity) {
        assert DeviceInfo.isXr();
        ensureNativeLibrariesLoaded();
        SessionCreateResult result = Session.create(activity);
        assert result instanceof SessionCreateSuccess : "Session creation failed.";
        return ((SessionCreateSuccess) result).getSession();
    }

    @SuppressLint("UnsafeDynamicallyLoadedCode")
    public static void ensureNativeLibrariesLoaded() {
        synchronized (sLock) {
            if (sLibrariesLoaded) {
                return;
            }
            try {
                for (String lib : NATIVE_LIBS) {
                    System.load(BundleUtils.getNativeLibraryPath(lib, MODULE_NAME));
                }
                sLibrariesLoaded = true;
            } catch (UnsatisfiedLinkError e) {
                Log.e(TAG, "Error loading native libraries", e);
                throw e;
            } catch (Exception e) {
                Log.e(TAG, "Error obtaining native library path", e);
                throw new RuntimeException(e);
            }
        }
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
        if (mIsFullSpaceModeRequested != TriState.NOT_SET) {
            return false;
        }

        // Do nothing if the activity doesn't have focus or the requested XR
        // space mode is already set.
        if (!mActivity.hasWindowFocus() || requestFullSpaceMode == isXrFullSpaceMode()) {
            return false;
        }

        mIsFullSpaceModeRequested = TriStateUtils.from(requestFullSpaceMode);
        mXrModeSwitchCallback = completedCallback;

        Scene scene = getScene();
        if (requestFullSpaceMode) {
            scene.requestFullSpaceMode();
        } else {
            scene.requestHomeSpaceMode();
        }

        return true;
    }

    @Override
    public NonNullObservableSupplier<Boolean> getXrSpaceModeObservableSupplier() {
        return mIsFullSpaceModeNowSupplier;
    }

    @Override
    public boolean isXrFullSpaceMode() {
        return mIsFullSpaceModeNowSupplier.get();
    }

    @MainThread
    @Override
    public void setMainPanelVisibility(boolean visible) {
        getScene().getMainPanelEntity().setEnabled(visible);
    }

    @Override
    public XrSurfaceEntityHolder createSurfaceEntity(@XrSurfaceEntityShape int shape) {
        return XrFactory.Holder.get().createSurfaceEntity(mXrSession, shape);
    }

    @Override
    public XrPanelEntityHolder createPanelEntity(View view, String name) {
        return XrFactory.Holder.get().createPanelEntity(mXrSession, view, name);
    }

    @Override
    public XrPanelEntityHolder getMainPanelEntity() {
        return XrPanelEntityHolderImpl.create(mXrSession, getScene().getMainPanelEntity());
    }

    @Override
    public XrEntityHolder getActivitySpaceEntity() {
        return new XrEntityHolderImpl<ActivitySpace>(mXrSession, mActivitySpace) {};
    }

    @Override
    public @Nullable XrPose getHeadPoseInActivitySpace() {
        ArDevice arDevice = getArDevice();
        if (!isHeadTrackingEnabled() || arDevice == null) return null;

        Scene scene = getScene();
        Pose devicePose = arDevice.getState().getValue().getDevicePose();
        Pose transformedPose =
                scene.getPerceptionSpace().transformPoseTo(devicePose, scene.getActivitySpace());
        return XrPoseImpl.toXrPose(transformedPose);
    }

    @Override
    public void setHeadTrackingEnabled(boolean enable) {
        if (mXrSession == null) return;
        mIsHeadTrackingEnabled = enable;
        if (!enable) {
            mHeadPoseTracker.stop();
        }
        Config currentConfig = mXrSession.getConfig();
        DeviceTrackingMode mode =
                enable ? DeviceTrackingMode.LAST_KNOWN : DeviceTrackingMode.DISABLED;
        if (currentConfig.getDeviceTracking() != mode) {
            mXrSession.configure(
                    currentConfig.copy(
                            currentConfig.getPlaneTracking(),
                            currentConfig.getHandTracking(),
                            mode,
                            currentConfig.getDepthEstimation(),
                            currentConfig.getAnchorPersistence()));
        }
    }

    @Override
    public boolean isHeadTrackingEnabled() {
        return mIsHeadTrackingEnabled;
    }

    @Override
    public boolean startHeadPoseTracking() {
        return mHeadPoseTracker.start();
    }

    @Override
    public void stopHeadPoseTracking() {
        mHeadPoseTracker.stop();
    }

    @Override
    public NullableObservableSupplier<XrPose> getHeadPoseObservableSupplier() {
        return mHeadPoseSupplier;
    }

    @Override
    public void setKeyEntity(@Nullable XrEntityHolder entityHolder) {
        Scene scene = getScene();
        if (entityHolder != null && entityHolder.getEntity() instanceof BaseEntity entity) {
            scene.setKeyEntity(entity);
        } else {
            scene.setKeyEntity(null);
        }
    }

    @Override
    public XrPixelDensity getPixelDensity() {
        return mPixelDensity;
    }

    @SuppressWarnings("NullAway")
    @Override
    public void destroy() {
        stopHeadPoseTracking();
        if (mActivitySpace != null) {
            mActivitySpace.removeOnBoundsChangedListener(mBoundsChangedListener);
            mActivitySpace = null;
        }
        mIsFullSpaceModeRequested = TriState.NOT_SET;
        mXrSession = null;
        mActivity = null;
    }

    private Scene getScene() {
        return SessionExt.getScene(mXrSession);
    }

    private @Nullable ArDevice getArDevice() {
        try {
            return ArDevice.getInstance(mXrSession);
        } catch (IllegalStateException e) {
            Log.w(TAG, "Failed to get ArDevice: " + e);
            return null;
        }
    }

    private void boundsChangeCallback(FloatSize3d dimensions) {
        mIsFullSpaceModeNowSupplier.set(dimensions.getWidth() == Float.POSITIVE_INFINITY);

        if (mIsFullSpaceModeRequested == TriStateUtils.from(isXrFullSpaceMode())) {
            // Mark the current request as completed.
            mIsFullSpaceModeRequested = TriState.NOT_SET;
            if (mXrModeSwitchCallback != null) {
                mXrModeSwitchCallback.run();
                mXrModeSwitchCallback = null;
            }
        }
    }
}
