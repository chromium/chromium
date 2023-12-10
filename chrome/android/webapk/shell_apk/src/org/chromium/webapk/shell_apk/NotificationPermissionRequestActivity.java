// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.Manifest;
import android.app.Activity;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.util.Log;

/**
 * A simple transparent activity for requesting the notification permission. On either approve or
 * disapprove, this will send the result via the {@link Messenger} provided with the intent, and
 * then finish.
 */
public class NotificationPermissionRequestActivity extends Activity {
    private static final String TAG = "PermissionRequestActivity";

    // An intent extra for a notification channel name string.
    private static final String EXTRA_NOTIFICATION_CHANNEL_NAME = "notificationChannelName";

    // An intent extra for a notification channel id string.
    private static final String EXTRA_NOTIFICATION_CHANNEL_ID = "notificationChannelId";

    // An intent extra for a {@link Messenger}.
    private static final String EXTRA_MESSENGER = "messenger";

    // A bundle key for a {@link PermissionStatus}.
    private static final String KEY_PERMISSION_STATUS = "permissionStatus";

    private String mChannelName;
    private String mChannelId;
    private Messenger mMessenger;

    /**
     * Creates a {@link PendingIntent} for launching this activity to request the notification
     * permission. It is mutable so that a messenger extra can be added for returning the permission
     * request result.
     */
    public static PendingIntent createPermissionRequestPendingIntent(
            Context context, String channelName, String channelId) {
        Intent intent =
                new Intent(
                        context.getApplicationContext(),
                        NotificationPermissionRequestActivity.class);
        intent.putExtra(EXTRA_NOTIFICATION_CHANNEL_NAME, channelName);
        intent.putExtra(EXTRA_NOTIFICATION_CHANNEL_ID, channelId);
        // Starting with Build.VERSION_CODES.S it is required to explicitly specify the mutability
        // of PendingIntents.
        int flags = Build.VERSION.SDK_INT >= Build.VERSION_CODES.S ? PendingIntent.FLAG_MUTABLE : 0;
        return PendingIntent.getActivity(context.getApplicationContext(), 0, intent, flags);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            Log.w(TAG, "Cannot request notification permission before Android T.");
            finish();
            return;
        }

        mChannelName = getIntent().getStringExtra(EXTRA_NOTIFICATION_CHANNEL_NAME);
        mChannelId = getIntent().getStringExtra(EXTRA_NOTIFICATION_CHANNEL_ID);
        mMessenger = getIntent().getParcelableExtra(EXTRA_MESSENGER);
        if (mChannelName == null || mChannelId == null || mMessenger == null) {
            Log.w(TAG, "Finishing because not all required extras were provided.");
            finish();
            return;
        }

        // When running on T or greater, with the app targeting less than T, creating a channel for
        // the first time will trigger the permission dialog.
        if (getApplicationContext().getApplicationInfo().targetSdkVersion
                < Build.VERSION_CODES.TIRAMISU) {
            NotificationChannel channel =
                    new NotificationChannel(
                            mChannelId, mChannelName, NotificationManager.IMPORTANCE_DEFAULT);
            getNotificationManager().createNotificationChannel(channel);
        }

        requestPermissions(new String[] {Manifest.permission.POST_NOTIFICATIONS}, 0);
    }

    @Override
    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        boolean enabled = false;
        for (int i = 0; i < permissions.length; i++) {
            if (!permissions[i].equals(Manifest.permission.POST_NOTIFICATIONS)) continue;

            PrefUtils.setHasRequestedNotificationPermission(this);
            enabled = grantResults[i] == PackageManager.PERMISSION_GRANTED;
            break;
        }

        // This method will only receive the notification permission and its grant result when
        // running on and targeting >= T. Check whether notifications are actually enabled, perhaps
        // because the system displayed a permission dialog after the first notification channel was
        // created and the user approved it.
        if (!enabled) {
            enabled = getNotificationManager().areNotificationsEnabled();
        }

        sendPermissionMessage(mMessenger, enabled);
        finish();
    }

    /** Sends a message to the messenger containing the permission status. */
    private static void sendPermissionMessage(Messenger messenger, boolean enabled) {
        Bundle data = new Bundle();
        @PermissionStatus int status = enabled ? PermissionStatus.ALLOW : PermissionStatus.BLOCK;
        data.putInt(KEY_PERMISSION_STATUS, status);
        Message message = Message.obtain();
        message.setData(data);

        try {
            messenger.send(message);
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    private NotificationManager getNotificationManager() {
        return (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
    }
}
