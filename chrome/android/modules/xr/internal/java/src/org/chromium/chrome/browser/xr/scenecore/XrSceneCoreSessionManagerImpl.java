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
import androidx.xr.runtime.Session;
import androidx.xr.runtime.SessionCreateResult;
import androidx.xr.runtime.SessionCreateSuccess;
import androidx.xr.runtime.math.FloatSize3d;
import androidx.xr.scenecore.ActivitySpace;
import androidx.xr.scenecore.BaseEntity;
import androidx.xr.scenecore.Scene;
import androidx.xr.scenecore.SessionExt;

import org.chromium.base.BundleUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.xr.scenecore.XrEntityHolder;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;
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
        "impress_api_jni", "arcore_sdk_c", "arcore_sdk_jni", "androidx.xr.runtime.openxr"
    };
    private static final Object sLock = new Object();
    private static boolean sLibrariesLoaded;
    private Session mXrSession;
    private Activity mActivity;
    private ActivitySpace mActivitySpace;

    // If not null, a request to change XR space mode is in progress.
    private @Nullable Boolean mIsFullSpaceModeRequested;
    private @Nullable Runnable mXrModeSwitchCallback;
    private final SettableNonNullObservableSupplier<Boolean> mIsFullSpaceModeNowSupplier;
    private final Consumer<FloatSize3d> mBoundsChangedListener = this::boundsChangeCallback;

    public XrSceneCoreSessionManagerImpl(Activity activity) {
        this(activity, createSession(activity));
    }

    @VisibleForTesting
    public XrSceneCoreSessionManagerImpl(Activity activity, Session session) {
        mActivity = activity;
        mXrSession = session;
        mActivitySpace = getScene().getActivitySpace();
        mActivitySpace.addOnBoundsChangedListener(mBoundsChangedListener);

        boolean isXrFullSpaceMode =
                mActivitySpace.getBounds().getWidth() == Float.POSITIVE_INFINITY;
        mIsFullSpaceModeNowSupplier = ObservableSuppliers.createNonNull(isXrFullSpaceMode);
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
        return XrEntityHolderFactory.createSurfaceEntityHolder(mXrSession, shape);
    }

    @Override
    public XrPanelEntityHolder createPanelEntity(View view, String name) {
        return XrEntityHolderFactory.createPanelEntityHolder(mXrSession, view, name);
    }

    @Override
    public XrPanelEntityHolder getMainPanelEntity() {
        return XrPanelEntityHolderImpl.create(mXrSession, getScene().getMainPanelEntity());
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

    @SuppressWarnings("NullAway")
    @Override
    public void destroy() {
        if (mActivitySpace != null) {
            mActivitySpace.removeOnBoundsChangedListener(mBoundsChangedListener);
            mActivitySpace = null;
        }
        mXrSession = null;
        mActivity = null;
    }

    private Scene getScene() {
        return SessionExt.getScene(mXrSession);
    }

    private void boundsChangeCallback(FloatSize3d dimensions) {
        mIsFullSpaceModeNowSupplier.set(dimensions.getWidth() == Float.POSITIVE_INFINITY);

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
