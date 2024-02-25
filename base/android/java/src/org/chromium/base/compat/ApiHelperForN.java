// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.compat;

import android.app.Activity;
import android.content.ClipData;
import android.content.ComponentName;
import android.content.pm.PackageManager;
import android.media.MediaCodec.CryptoInfo;
import android.net.ConnectivityManager;
import android.os.Build;
import android.os.Process;
import android.security.NetworkSecurityPolicy;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;
import android.view.View.DragShadowBuilder;

import androidx.annotation.RequiresApi;

/**
 * Utility class to use new APIs that were added in N (API level 24). These need to exist in a
 * separate class so that Android framework can successfully verify classes without
 * encountering the new APIs.
 */
@RequiresApi(Build.VERSION_CODES.N)
public final class ApiHelperForN {
    private ApiHelperForN() {}

    /** See {@link View#startDragAndDrop(ClipData, DragShadowBuilder, Object, int)}. */
    public static boolean startDragAndDrop(
            View view,
            ClipData data,
            DragShadowBuilder shadowBuilder,
            Object myLocalState,
            int flags) {
        return view.startDragAndDrop(data, shadowBuilder, myLocalState, flags);
    }

    /** See {@link CryptoInfo#setPattern(Pattern)}. */
    public static void setCryptoInfoPattern(CryptoInfo cryptoInfo, int encrypt, int skip) {
        cryptoInfo.setPattern(new CryptoInfo.Pattern(encrypt, skip));
    }

    /** See {@link Activity#setVrModeEnabled(boolean, ComponentName)}. */
    public static void setVrModeEnabled(
            Activity activity, boolean enabled, ComponentName requestedComponent)
            throws PackageManager.NameNotFoundException {
        activity.setVrModeEnabled(enabled, requestedComponent);
    }

    /** See {@link NetworkSecurityPolicy#isCleartextTrafficPermitted(String)}. */
    public static boolean isCleartextTrafficPermitted(String host) {
        return NetworkSecurityPolicy.getInstance().isCleartextTrafficPermitted(host);
    }

    /** See {@link View#onResolvePointerIcon(MotionEvent, int)}. */
    public static PointerIcon onResolvePointerIcon(View view, MotionEvent event, int pointerIndex) {
        return view.onResolvePointerIcon(event, pointerIndex);
    }

    /** See {@link Process#getStartUptimeMillis()}. */
    public static long getStartUptimeMillis() {
        return Process.getStartUptimeMillis();
    }

    /** See {@link ConnectivityManager#getRestrictBackgroundStatus(ConnectivityManager)}. */
    public static int getRestrictBackgroundStatus(ConnectivityManager cm) {
        return cm.getRestrictBackgroundStatus();
    }
}
