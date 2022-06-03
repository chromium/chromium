// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.send_tab_to_self.SendTabToSelfShareActivity;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfCoordinator;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.NavigationEntry;

import java.util.HashMap;
import java.util.Map;

/**
 * Glues the sub-class of ChromeAccessorActivity to the relevant share action.
 *
 * When adding a new share action to Chrome,
 * 1. Add a new subclass of {@link ChromeAccessorActivity}.
 * 2. Register that activity and the proper intent-filter in AndroidManifest.xml.
 * 3. Register a {@link BroadcastReceiver} here to catch the action broadcasted from (1).
 * 4. Implement the sharing logic in a function inside this class and call it from (3).
 **/
public class ShareRegistrationCoordinator {
    private static final String TAG = "ShareRegCoord";

    /** Handles receiving share-specific internal broadcasts. */
    public static class ShareBroadcastReceiver extends BroadcastReceiver {
        /** A token used to verify that the broadcast is a Chrome-internal one. */
        private static final String EXTRA_TOKEN = "receiver_token";

        /** The type to pass along to the receiver, used to route the request. */
        @VisibleForTesting
        static final String EXTRA_TYPE = "share_type";

        /** Top-level intent action that allows the share actions to be grouped in one intent. */
        private static final String RECEIVER_ACTION = "ShareBroadcastReceiverBroadcastAction";

        private static final Map<Integer, ShareBroadcastReceiver> sReceiverMap = new HashMap<>();

        /**
         * Send a share broadcast with the given action.
         * @param taskId The Activity task id for the broadcast destination,.
         * @param action The share action to be broadcast.
         */
        public static void sendShareBroadcastWithAction(int taskId, String action) {
            sendShareBroadcastWithAction(taskId, action, ContextUtils.getApplicationContext());
        }

        @VisibleForTesting
        static void sendShareBroadcastWithAction(int taskId, String action, Context context) {
            ShareBroadcastReceiver receiver = sReceiverMap.get(taskId);
            if (receiver == null) {
                Log.e(TAG,
                        "Attempt to send share broadcast before reciever was registered: \""
                                + action + "\"");
                return;
            }

            Intent intent = new Intent(RECEIVER_ACTION);
            // Attach the parent ShareRegistrationCoordinator's hashcode to verify the intent.
            intent.putExtra(EXTRA_TOKEN, receiver.getHashCodeToken());
            intent.putExtra(EXTRA_TYPE, action);
            intent.putExtra(ShareHelper.EXTRA_TASK_ID, taskId);

            context.sendBroadcast(intent);
        }

        private final Map<String, Runnable> mShareMap = new HashMap<>();
        private final int mTaskId;
        private final int mHashCodeToken;
        private Context mContext;
        private boolean mIsDestroyed;

        /**
         * @param activity The activity to associate with this receiver.
         */
        public ShareBroadcastReceiver(Activity activity) {
            this(activity.getTaskId(), ContextUtils.getApplicationContext());
        }

        @VisibleForTesting
        ShareBroadcastReceiver(int taskId, Context context) {
            mTaskId = taskId;
            mContext = context;
            // We do this so the token is durable over the lifetime of the app.
            mHashCodeToken = hashCode();

            sReceiverMap.put(mTaskId, this);
            mContext.registerReceiver(this, new IntentFilter(RECEIVER_ACTION));
        }

        /** Destroy the receiver. */
        public void destroy() {
            mIsDestroyed = true;
            sReceiverMap.remove(mTaskId);
            mContext.unregisterReceiver(this);

            mContext = null;
        }

        /**
         * Register this share type.
         * @param type The share type to register.
         * @param runnable The runnable to run when the share type is broadcasted.
         */
        public void registerShareType(String type, Runnable runnable) {
            if (mIsDestroyed) {
                Log.e(TAG, "Attempted to register type after destruction: \"" + type + "\".");
                return;
            }

            if (mShareMap.containsKey(type)) {
                throw new IllegalStateException(
                        "Only one instance of a share type should be registered at a time.");
            }

            mShareMap.put(type, runnable);
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            String type = IntentUtils.safeGetStringExtra(intent, EXTRA_TYPE);
            if (mIsDestroyed) {
                Log.e(TAG, "Broadcast received after destruction: \"" + type + "\".");
                return;
            }

            boolean hasToken = intent.hasExtra(EXTRA_TOKEN)
                    && intent.getIntExtra(EXTRA_TOKEN, 0) == mHashCodeToken;
            boolean hasTaskId = intent.hasExtra(ShareHelper.EXTRA_TASK_ID)
                    && intent.getIntExtra(ShareHelper.EXTRA_TASK_ID, 0) == mTaskId;
            if (!hasToken || !hasTaskId) return;

            if (!mShareMap.containsKey(type)) {
                Log.e(TAG, "Unidentified type receieved: \"" + type + "\".");
                return;
            }

            mShareMap.get(type).run();
        }

        private int getHashCodeToken() {
            return mHashCodeToken;
        }
    }

    private final ShareBroadcastReceiver mShareBroadcastReceiver;

    /** ShareRegistrationCoordinator constructor. */
    public ShareRegistrationCoordinator(Activity activity, Supplier<Tab> currentTabSupplier,
            BottomSheetController bottomSheetController) {
        mShareBroadcastReceiver = new ShareBroadcastReceiver(activity);

        mShareBroadcastReceiver.registerShareType(
                SendTabToSelfShareActivity.BROADCAST_ACTION, () -> {
                    NavigationEntry entry = currentTabSupplier.hasValue()
                            ? currentTabSupplier.get()
                                      .getWebContents()
                                      .getNavigationController()
                                      .getVisibleEntry()
                            : null;
                    doSendTabToSelfShare(activity, entry, bottomSheetController);
                });
    }

    /**
     * Register the type runnable pair.
     * @param type The share type to register.
     * @param runnable The runnable to invoke for the given share type.
     */
    public void registerShareType(String type, Runnable runnable) {
        mShareBroadcastReceiver.registerShareType(type, runnable);
    }

    /** Destroys this component */
    public void destroy() {
        mShareBroadcastReceiver.destroy();
    }

    /**
     * Starts a send tab to self share action.
     *
     * @param context The current application context.
     * @param entry The current {@link NavigationEntry}, null if the current tab isn't available or
     *              doesn't have a visible entry.
     * @param bottomSheetController Controls what's shown in the bottom sheet.
     */
    @VisibleForTesting
    void doSendTabToSelfShare(@NonNull Context context, @Nullable NavigationEntry entry,
            @NonNull BottomSheetController bottomSheetController) {
        if (entry == null) return;
        boolean isSyncEnabled = SyncService.get() != null && SyncService.get().isSyncRequested();
        bottomSheetController.requestShowContent(
                SendTabToSelfCoordinator.createBottomSheetContent(context, entry.getUrl().getSpec(),
                        entry.getTitle(), entry.getTimestamp(), bottomSheetController,
                        new SettingsLauncherImpl(), isSyncEnabled),
                true);
    }
}
