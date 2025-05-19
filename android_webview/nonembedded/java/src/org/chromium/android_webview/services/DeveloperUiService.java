// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.services;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Binder;
import android.os.Build;
import android.os.IBinder;
import android.os.Process;
import android.os.RemoteException;

import androidx.annotation.NonNull;

import org.chromium.android_webview.common.DeveloperModeUtils;
import org.chromium.android_webview.common.Flag;
import org.chromium.android_webview.common.FlagOverrideHelper;
import org.chromium.android_webview.common.ProductionSupportedFlagList;
import org.chromium.android_webview.common.services.IDeveloperUiService;
import org.chromium.android_webview.common.services.ServiceHelper;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;

import java.util.HashMap;
import java.util.Map;

import javax.annotation.concurrent.GuardedBy;

/**
 * A Service to support Developer UI features. This service enables communication between the
 * WebView implementation embedded in apps on the system and the Developer UI.
 */
public final class DeveloperUiService extends Service {
    private static final String TAG = "WebViewDevTools";

    private static final String CHANNEL_ID = "DevUiChannel";
    private static final int FLAG_OVERRIDE_NOTIFICATION_ID = 1;

    // Keep in sync with MainActivity.java
    private static final String FRAGMENT_ID_INTENT_EXTRA = "fragment-id";
    private static final String RESET_FLAGS_INTENT_EXTRA = "reset-flags";

    private static final int FRAGMENT_ID_FLAGS = 2;

    public static final String NOTIFICATION_TITLE = "Experimental WebView features active";
    public static final String NOTIFICATION_CONTENT = "Tap to see experimental WebView features.";
    public static final String NOTIFICATION_TICKER = "Experimental WebView features active";

    private static final Object sLock = new Object();

    @GuardedBy("sLock")
    private static Map<String, Boolean> sOverriddenFlags = new HashMap<>();

    // This is locked to guard reads/writes to the corresponding SharedPreferences object. Make sure
    // all edits to that object are synchronized on sLock.
    @GuardedBy("sLock")
    private static final String SHARED_PREFS_FILE = "webview_devui_flags";

    private static final Map<String, String> INITIAL_SWITCHES =
            CommandLine.getInstance().getSwitches();

    @GuardedBy("sLock")
    private static @NonNull Flag[] sFlagList = ProductionSupportedFlagList.sFlagList;

    private final IDeveloperUiService.Stub mBinder =
            new IDeveloperUiService.Stub() {
                @Override
                public void setFlagOverrides(Map overriddenFlags) {
                    if (Binder.getCallingUid() != Process.myUid()) {
                        throw new SecurityException(
                                "setFlagOverrides() may only be called by the Developer UI app");
                    }
                    synchronized (sLock) {
                        applyFlagsToCommandLine(sOverriddenFlags, overriddenFlags);
                        sOverriddenFlags = overriddenFlags;
                        writeFlagsToStorageAsync(sOverriddenFlags);
                        if (sOverriddenFlags.isEmpty()) {
                            disableDeveloperMode();
                        } else {
                            enableDeveloperMode();
                        }
                    }
                }
            };

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        final int mode = super.onStartCommand(intent, flags, startId);
        // Service is always expected to run in foreground, so mark as such when it is started.
        // Subsequent calls will simply replace the foreground service notification.
        markAsForegroundService();
        return mode;
    }

    /**
     * Static method to fetch the flag overrides. If this returns an empty map, this will
     * asynchronously restart the Service to disable developer mode.
     */
    public static Map<String, Boolean> getFlagOverrides() {
        Map<String, Boolean> flagOverrides;
        synchronized (sLock) {
            // Create a copy so the caller can do what it wants with the Map without worrying about
            // thread safety.
            flagOverrides = new HashMap<>(sOverriddenFlags);
        }
        if (flagOverrides.isEmpty()) {
            // If the map is empty, it's probably because the Service has died. Read flags from
            // disk to recover.
            flagOverrides = readFlagsFromStorageSync();
            // Send flags back to the service so it can properly restore developer mode.
            startServiceAndSendFlags(flagOverrides);
        }
        return flagOverrides;
    }

    private static void startServiceAndSendFlags(final Map<String, Boolean> flags) {
        final Context context = ContextUtils.getApplicationContext();
        ServiceConnection connection =
                new ServiceConnection() {
                    @Override
                    public void onServiceConnected(ComponentName name, IBinder service) {
                        try {
                            IDeveloperUiService.Stub.asInterface(service).setFlagOverrides(flags);
                        } catch (RemoteException e) {
                            Log.e(TAG, "Failed to send flag overrides to service", e);
                        } finally {
                            // Unbind when we've sent the flags overrides, since we expect we only
                            // need to do this once.
                            context.unbindService(this);
                        }
                    }

                    @Override
                    public void onServiceDisconnected(ComponentName name) {}
                };
        Intent intent = new Intent(context, DeveloperUiService.class);
        if (!ServiceHelper.bindService(context, intent, connection, Context.BIND_AUTO_CREATE)) {
            Log.e(TAG, "Failed to bind to Developer UI service");
        }
    }

    @GuardedBy("sLock")
    private static boolean isFlagAllowed(String name) {
        for (Flag flag : sFlagList) {
            if (flag.getName().equals(name)) return true;
        }
        return false;
    }

    private static Map<String, Boolean> readFlagsFromStorageSync() {
        synchronized (sLock) {
            Map<String, Boolean> flags = new HashMap<>();
            Map<String, ?> allPreferences =
                    ContextUtils.getApplicationContext()
                            .getSharedPreferences(SHARED_PREFS_FILE, Context.MODE_PRIVATE)
                            .getAll();

            for (Map.Entry<String, ?> entry : allPreferences.entrySet()) {
                String flagName = entry.getKey();
                // Since flags may be persisted by a previous version, we need to filter by the
                // current version's sFlagList (in case flags get removed from
                // ProductionSupportedFlagList).
                if (!isFlagAllowed(flagName)) {
                    Log.w(TAG, "Toggling '" + flagName + "' is no longer supported");
                    continue;
                }
                if (!(entry.getValue() instanceof Boolean)) {
                    Log.e(TAG, "Expected value '" + entry.getValue() + "' to be type Boolean");
                    continue;
                }
                boolean enabled = (Boolean) entry.getValue();
                flags.put(flagName, enabled);
            }
            return flags;
        }
    }

    private static void writeFlagsToStorageAsync(Map<String, Boolean> flags) {
        synchronized (sLock) {
            SharedPreferences.Editor editor =
                    ContextUtils.getApplicationContext()
                            .getSharedPreferences(SHARED_PREFS_FILE, Context.MODE_PRIVATE)
                            .edit();
            editor.clear();
            for (Map.Entry<String, Boolean> entry : flags.entrySet()) {
                String flagName = entry.getKey();
                boolean enabled = entry.getValue();
                editor.putBoolean(flagName, enabled);
            }
            // Ignore errors, since there's no way to recover. Commit changes async to avoid
            // blocking the service.
            editor.apply();
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    private Intent createFlagsFragmentIntent(boolean resetFlags) {
        Intent intent = new Intent("com.android.webview.SHOW_DEV_UI");
        intent.setClassName(getPackageName(), "org.chromium.android_webview.devui.MainActivity");
        intent.putExtra(FRAGMENT_ID_INTENT_EXTRA, FRAGMENT_ID_FLAGS);
        if (resetFlags) {
            intent.putExtra(RESET_FLAGS_INTENT_EXTRA, resetFlags);
        }

        return intent;
    }

    private void registerDefaultNotificationChannel() {
        CharSequence name = "WebView DevTools alerts";
        // The channel importance should be consistent with the Notification priority on pre-O.
        NotificationChannel channel =
                new NotificationChannel(CHANNEL_ID, name, NotificationManager.IMPORTANCE_LOW);
        channel.enableVibration(false);
        channel.enableLights(false);
        NotificationManager notificationManager = getSystemService(NotificationManager.class);
        notificationManager.createNotificationChannel(channel);
    }

    private void markAsForegroundService() {
        registerDefaultNotificationChannel();

        Intent openFlagsIntent = createFlagsFragmentIntent(false);
        PendingIntent pendingOpenFlagsIntent =
                PendingIntent.getActivity(
                        this,
                        0,
                        openFlagsIntent,
                        IntentUtils.getPendingIntentMutabilityFlag(false));

        // While this service does ultimately manage writing the flag overrides, we would run
        // into issues around synchronizing with the flags fragment if it's open because it holds
        // onto the state of the flags so we send an intent to reset through there.
        Intent resetIntent = createFlagsFragmentIntent(true);
        PendingIntent pendingResetExperimentsIntent =
                PendingIntent.getActivity(
                        this, 1, resetIntent, IntentUtils.getPendingIntentMutabilityFlag(false));

        Notification.Action resetExperimentsAction =
                new Notification.Action.Builder(
                                org.chromium.android_webview.devui.R.drawable.ic_flag,
                                "Disable experimental features",
                                pendingResetExperimentsIntent)
                        .build();

        Notification notification =
                new Notification.Builder(this, CHANNEL_ID)
                        .setContentTitle(NOTIFICATION_TITLE)
                        .setContentText(NOTIFICATION_CONTENT)
                        .setSmallIcon(org.chromium.android_webview.devui.R.drawable.ic_flag)
                        .setContentIntent(pendingOpenFlagsIntent)
                        .setOngoing(true)
                        .setVisibility(Notification.VISIBILITY_PUBLIC)
                        .addAction(resetExperimentsAction)
                        .setTicker(NOTIFICATION_TICKER)
                        .build();
        try {
            startForeground(FLAG_OVERRIDE_NOTIFICATION_ID, notification);
        } catch (IllegalStateException e) {
            logSuspectedForegroundServiceStartNotAllowedException();
        }
    }

    private void logSuspectedForegroundServiceStartNotAllowedException() {
        // Expecting a ForegroundServiceStartNotAllowedException, but that's an S API.
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                : "Unable enable developer mode, this is only expected on Android S";
        String msg =
                "Unable to create foreground service (client is likely in "
                        + "background). Continuing as a background service.";
        Log.w(TAG, msg);
    }

    /**
     * Enables developer mode. This includes requesting foreground status, toggling
     * {@code DEVELOPER_MODE_STATE_COMPONENT}'s enabled status, posting the notification, etc.
     *
     * @throws IllegalStateException if we're on Android S+ and we're currently running with
     * background status. In this case, {@code mDeveloperModeEnabled} will be {@code false} and
     * {@code DEVELOPER_MODE_STATE_COMPONENT} will be unmodified so that we can call try again when
     * the next client connects.
     */
    private void enableDeveloperMode() {
        synchronized (sLock) {

            // Mark developer mode as enabled for other apps.
            ComponentName developerModeState =
                    new ComponentName(this, DeveloperModeUtils.DEVELOPER_MODE_STATE_COMPONENT);
            getPackageManager()
                    .setComponentEnabledSetting(
                            developerModeState,
                            PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
                            PackageManager.DONT_KILL_APP);

            // Keep this service alive as long as we're in developer mode.
            Intent intent = new Intent(this, DeveloperUiService.class);
            try {
                startForegroundService(intent);
            } catch (IllegalStateException e) {
                // Android O doesn't allow bound Services to request foreground status unless the
                // app is running in the foreground already or we already started the service with
                // Context#startForegroundService.
                logSuspectedForegroundServiceStartNotAllowedException();
            }
        }
    }

    private void disableDeveloperMode() {
        synchronized (sLock) {
            ComponentName developerModeState =
                    new ComponentName(this, DeveloperModeUtils.DEVELOPER_MODE_STATE_COMPONENT);
            getPackageManager()
                    .setComponentEnabledSetting(
                            developerModeState,
                            PackageManager.COMPONENT_ENABLED_STATE_DEFAULT,
                            PackageManager.DONT_KILL_APP);

            // Finally, stop the service explicitly. Do this last to make sure we do the other
            // necessary cleanup.
            stopForeground(STOP_FOREGROUND_REMOVE);
            stopSelf();
        }
    }

    /**
     * Undoes {@code oldFlags} and applies {@code newFlags}. When undoing {@code oldFlags}, we do
     * a best-effort attempt to restore the initial CommandLine state set by the flags file. More
     * precisely, we default to whatever state is captured by INITIAL_SWITCHES.
     *
     * <p><b>Note:</b> {@code newFlags} are not applied atomically.
     */
    @GuardedBy("sLock")
    private void applyFlagsToCommandLine(
            Map<String, Boolean> oldFlags, Map<String, Boolean> newFlags) {
        // Best-effort attempt to undo oldFlags back to the initial CommandLine.
        for (String flagName : oldFlags.keySet()) {
            if (INITIAL_SWITCHES.containsKey(flagName)) {
                // If the initial CommandLine had this switch, restore its value.
                CommandLine.getInstance()
                        .appendSwitchWithValue(flagName, INITIAL_SWITCHES.get(flagName));
            } else if (CommandLine.getInstance().hasSwitch(flagName)) {
                // Otherwise, make sure it's removed from the CommandLine. As an optimization, this
                // is only necessary if the current CommandLine has the switch.
                CommandLine.getInstance().removeSwitch(flagName);
            }
        }

        // Apply newFlags
        FlagOverrideHelper helper = new FlagOverrideHelper(sFlagList);
        helper.applyFlagOverrides(newFlags);
    }

    public static void clearSharedPrefsForTesting(Context context) {
        synchronized (sLock) {
            context.getSharedPreferences(DeveloperUiService.SHARED_PREFS_FILE, Context.MODE_PRIVATE)
                    .edit()
                    .clear()
                    .apply();
        }
    }

    public static void setFlagListForTesting(@NonNull Flag[] flagList) {
        synchronized (sLock) {
            sFlagList = flagList;
        }
    }
}
