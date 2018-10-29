// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Process;
import android.support.customtabs.CustomTabsCallback;
import android.support.customtabs.CustomTabsClient;
import android.support.customtabs.CustomTabsIntent;
import android.support.customtabs.CustomTabsServiceConnection;
import android.support.customtabs.CustomTabsSession;
import android.support.test.InstrumentationRegistry;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Utility class that contains convenience calls related with custom tabs testing.
 */
public class CustomTabsTestUtils {

    /**
     * Creates the simplest intent that is sufficient to let {@link ChromeLauncherActivity} launch
     * the {@link CustomTabActivity}.
     */
    public static Intent createMinimalCustomTabIntent(
            Context context, String url) {
        CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder(
                CustomTabsSession.createMockSessionForTesting(
                        new ComponentName(context, ChromeLauncherActivity.class)));
        CustomTabsIntent customTabsIntent = builder.build();
        Intent intent = customTabsIntent.intent;
        intent.setAction(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    public static CustomTabsConnection setUpConnection() {
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.resetThrottling(Process.myUid());
        return connection;
    }

    public static void cleanupSessions(final CustomTabsConnection connection) {
        ThreadUtils.runOnUiThreadBlocking(connection::cleanupAll);
    }

    public static CustomTabsSession bindWithCallback(final CustomTabsCallback callback)
            throws InterruptedException, TimeoutException {
        final AtomicReference<CustomTabsSession> sessionReference = new AtomicReference<>(null);
        final CallbackHelper waitForConnection = new CallbackHelper();
        CustomTabsClient.bindCustomTabsService(InstrumentationRegistry.getContext(),
                InstrumentationRegistry.getTargetContext().getPackageName(),
                new CustomTabsServiceConnection() {
                    @Override
                    public void onServiceDisconnected(ComponentName name) {}

                    @Override
                    public void onCustomTabsServiceConnected(
                            ComponentName name, CustomTabsClient client) {
                        sessionReference.set(client.newSession(callback));
                        waitForConnection.notifyCalled();
                    }
                });
        waitForConnection.waitForCallback(0);
        return sessionReference.get();
    }

    /** Calls warmup() and waits for all the tasks to complete. Fails the test otherwise. */
    public static CustomTabsConnection warmUpAndWait()
            throws InterruptedException, TimeoutException {
        CustomTabsConnection connection = setUpConnection();
        final CallbackHelper startupCallbackHelper = new CallbackHelper();
        CustomTabsSession session = bindWithCallback(new CustomTabsCallback() {
            @Override
            public void extraCallback(String callbackName, Bundle args) {
                if (callbackName.equals(CustomTabsConnection.ON_WARMUP_COMPLETED)) {
                    startupCallbackHelper.notifyCalled();
                }
            }
        });
        Assert.assertTrue(connection.warmup(0));
        startupCallbackHelper.waitForCallback(0);
        return connection;
    }

    public static void openAppMenuAndAssertMenuShown(CustomTabActivity activity) {
        ThreadUtils.runOnUiThread(
                () -> { activity.onMenuOrKeyboardAction(R.id.show_menu, false); });

        CriteriaHelper.pollUiThread(
                activity.getAppMenuHandler()::isAppMenuShowing, "App menu was not shown");
    }
}
