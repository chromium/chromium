// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.send_tab_to_self.SendTabToSelfShareActivity;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.ui.base.WindowAndroid;

import java.util.HashMap;
import java.util.Map;

/**
 * Glues the sub-class of ChromeAccessorActivity to the relevant share action.
 *
 * When adding a new share action to Chrome,
 * 1. Add a new subclass of {@link ChromeAccessorActivity}.
 * 2. Register that activity and the proper intent-filter in AndroidManifest.xml.
 * 3. Register a {@link ShareRegistrationCoordinator} here to handle the action from (1).
 * 4. Implement the sharing logic in a function inside this class and call it from (3).
 **/
public class ShareRegistrationCoordinator {
    private static final String TAG = "ShareRegCoord";

    private static final Map<Integer, ShareRegistrationCoordinator> sCoordinatorMap =
            new HashMap<>();

    private final Map<String, Runnable> mShareMap = new HashMap<>();
    private final int mTaskId;

    /**
     * Signals that the user chose the specific share action in the given Android activity task.
     *
     * @param taskId The task ID of the activity handling the share action.
     * @param action The share action chosen.
     */
    public static void onShareActionChosen(int taskId, String action) {
        ShareRegistrationCoordinator coordinator = sCoordinatorMap.get(taskId);

        if (coordinator == null) {
            Log.e(TAG,
                    "Attempt to send share broadcast before reciever was registered: \"" + action
                            + "\"");
            return;
        }

        Runnable actionRunnable = coordinator.mShareMap.get(action);
        if (actionRunnable == null) {
            Log.e(TAG, "No registered action for: \"" + action + "\"");
            return;
        }
        actionRunnable.run();
    }

    /** ShareRegistrationCoordinator constructor. */
    public ShareRegistrationCoordinator(Activity activity, WindowAndroid windowAndroid,
            Supplier<Tab> currentTabSupplier, BottomSheetController bottomSheetController) {
        mTaskId = activity.getTaskId();

        registerShareType(
                SendTabToSelfShareActivity.SHARE_ACTION, () -> {
                    NavigationEntry entry = currentTabSupplier.hasValue()
                            ? currentTabSupplier.get()
                                      .getWebContents()
                                      .getNavigationController()
                                      .getVisibleEntry()
                            : null;
                    doSendTabToSelfShare(activity, windowAndroid, entry, bottomSheetController);
                });
        sCoordinatorMap.put(mTaskId, this);
    }

    /**
     * Register the type runnable pair.
     * @param type The share type to register.
     * @param runnable The runnable to invoke for the given share type.
     */
    public void registerShareType(String type, Runnable runnable) {
        if (mShareMap.containsKey(type)) {
            throw new IllegalStateException(
                    "Only one instance of a share type should be registered at a time.");
        }
        mShareMap.put(type, runnable);
    }

    /** Destroys this component */
    public void destroy() {
        sCoordinatorMap.remove(mTaskId);
    }

    /**
     * Starts a send tab to self share action.
     *
     * @param context The current application context.
     * @param windowAndroid The current window.
     * @param entry The current {@link NavigationEntry}, null if the current tab isn't available or
     *              doesn't have a visible entry.
     * @param bottomSheetController Controls what's shown in the bottom sheet.
     */
    @VisibleForTesting
    void doSendTabToSelfShare(@NonNull Context context, @NonNull WindowAndroid windowAndroid,
            @Nullable NavigationEntry entry, @NonNull BottomSheetController bottomSheetController) {
        if (entry == null) return;
        SendTabToSelfCoordinator coordinator =
                new SendTabToSelfCoordinator(context, windowAndroid, entry.getUrl().getSpec(),
                        entry.getTitle(), bottomSheetController, entry.getTimestamp());
        coordinator.show();
    }
}
