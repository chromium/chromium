// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.compat;

import android.animation.ValueAnimator;
import android.app.Activity;
import android.app.Notification;
import android.content.BroadcastReceiver;
import android.content.ClipData;
import android.content.ClipData.Item;
import android.content.ClipDescription;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.net.ConnectivityManager;
import android.net.ConnectivityManager.NetworkCallback;
import android.net.NetworkRequest;
import android.os.Build;
import android.os.Handler;
import android.view.Display;
import android.view.View;
import android.view.Window;
import android.view.autofill.AutofillManager;

import androidx.annotation.RequiresApi;

import org.chromium.base.StrictModeContext;

/**
 * Utility class to use new APIs that were added in O (API level 26). These need to exist in a
 * separate class so that Android framework can successfully verify classes without
 * encountering the new APIs.
 */
@RequiresApi(Build.VERSION_CODES.O)
public final class ApiHelperForO {
    private ApiHelperForO() {}

    /** See {@link Display#isWideColorGamut() }. */
    public static boolean isWideColorGamut(Display display) {
        return display.isWideColorGamut();
    }

    /** See {@link Window#setColorMode(int) }. */
    public static void setColorMode(Window window, int colorMode) {
        window.setColorMode(colorMode);
    }

    /** See {@link Configuration#isScreenWideColorGamut() }. */
    public static boolean isScreenWideColorGamut(Configuration configuration) {
        return configuration.isScreenWideColorGamut();
    }

    /** See {@link PackageManager#isInstantApp() }. */
    public static boolean isInstantApp(PackageManager packageManager) {
        return packageManager.isInstantApp();
    }

    /** See {@link View#setDefaultFocusHighlightEnabled(boolean) }. */
    public static void setDefaultFocusHighlightEnabled(View view, boolean enabled) {
        view.setDefaultFocusHighlightEnabled(enabled);
    }

    /** See {@link ClipDescription#getTimestamp()}. */
    public static long getTimestamp(ClipDescription clipDescription) {
        return clipDescription.getTimestamp();
    }

    /**
     * See {@link Context.createContextForSplit(String) }. Be careful about adding new uses of
     * this, most split Contexts should be created through {@link
     * BundleUtils.createIsolatedSplitContext(Context, String) since it has workarounds for
     * framework bugs.
     */
    public static Context createContextForSplit(Context context, String name)
            throws PackageManager.NameNotFoundException {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return context.createContextForSplit(name);
        }
    }

    /** See {@link AutofillManager#cancel()}. */
    public static void cancelAutofillSession(Activity activity) {
        // The AutofillManager has to be retrieved from an activity context.
        // https://cs.android.com/android/platform/superproject/+/master:frameworks/base/core/java/android/app/Application.java;l=624;drc=5d123b67756dffcfdebdb936ab2de2b29c799321
        AutofillManager afm = activity.getSystemService(AutofillManager.class);
        if (afm != null) {
            afm.cancel();
        }
    }

    /** See {@link AutofillManager#notifyValueChanged(View)}. */
    public static void notifyValueChangedForAutofill(View view) {
        final AutofillManager afm = view.getContext().getSystemService(AutofillManager.class);
        if (afm != null) {
            afm.notifyValueChanged(view);
        }
    }

    /**
     * See {@link ConnectivityManager#registerNetworkCallback(NetworkRequest,
     * ConnectivityManager.NetworkCallback, Handler) }.
     */
    public static void registerNetworkCallback(ConnectivityManager connectivityManager,
            NetworkRequest networkRequest, NetworkCallback networkCallback, Handler handler) {
        connectivityManager.registerNetworkCallback(networkRequest, networkCallback, handler);
    }

    /** See {@link ValueAnimator#areAnimatorsEnabled()}. */
    public static boolean areAnimatorsEnabled() {
        return ValueAnimator.areAnimatorsEnabled();
    }

    /** See {@link Notification.Builder#setChannelId(String)}. */
    public static Notification.Builder setChannelId(
            Notification.Builder builder, String channelId) {
        return builder.setChannelId(channelId);
    }

    /** See {@link Notification.Builder#setTimeoutAfter(long)}. */
    public static Notification.Builder setTimeoutAfter(Notification.Builder builder, long ms) {
        return builder.setTimeoutAfter(ms);
    }

    /**
     * See {@link
     * ConnectivityManager#registerDefaultNetworkCallback(ConnectivityManager.NetworkCallback,
     * Handler) }.
     */
    public static void registerDefaultNetworkCallback(ConnectivityManager connectivityManager,
            NetworkCallback networkCallback, Handler handler) {
        connectivityManager.registerDefaultNetworkCallback(networkCallback, handler);
    }

    /** See {@link Notification#getChannelId()}. */
    public static String getNotificationChannelId(Notification notification) {
        return notification.getChannelId();
    }

    /**
     * See {@link Context#registerReceiver(BroadcastReceiver, IntentFilter, String, Handler, int)}
     */
    public static Intent registerReceiver(Context context, BroadcastReceiver receiver,
            IntentFilter filter, String permission, Handler scheduler, int flags) {
        return context.registerReceiver(receiver, filter, permission, scheduler, flags);
    }

    /**
     * See {@link ClipData#addItem(ContentResolver, Item)}.
     */
    public static void addItem(ClipData clipData, ContentResolver contentResolver, Item item) {
        clipData.addItem(contentResolver, item);
    }
}
