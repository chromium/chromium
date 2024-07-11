// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule.LONG_TIMEOUT_MS;

import android.content.ComponentName;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.Process;

import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsClient;
import androidx.browser.customtabs.CustomTabsServiceConnection;
import androidx.browser.customtabs.CustomTabsSession;
import androidx.test.core.app.ApplicationProvider;

import org.hamcrest.Matchers;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;
import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Utility class that contains convenience calls related with custom tabs testing. */
@JNINamespace("customtabs")
public class CustomTabsTestUtils {
    /** Intent extra to specify an id to a custom tab. */
    public static final String EXTRA_CUSTOM_TAB_ID =
            "android.support.customtabs.extra.tests.CUSTOM_TAB_ID";

    /** A plain old data class that holds the return value from {@link #bindWithCallback}. */
    public static class ClientAndSession {
        public final CustomTabsClient client;
        public final CustomTabsSession session;

        /** Creates and populates the class. */
        public ClientAndSession(CustomTabsClient client, CustomTabsSession session) {
            this.client = client;
            this.session = session;
        }
    }

    public static CustomTabsConnection setUpConnection() {
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.resetThrottling(Process.myUid());
        return connection;
    }

    public static void cleanupSessions(final CustomTabsConnection connection) {
        ThreadUtils.runOnUiThreadBlocking(connection::cleanupAllForTesting);
    }

    public static ClientAndSession bindWithCallback(final CustomTabsCallback callback)
            throws TimeoutException {
        final AtomicReference<CustomTabsSession> sessionReference = new AtomicReference<>();
        final AtomicReference<CustomTabsClient> clientReference = new AtomicReference<>();
        final CallbackHelper waitForConnection = new CallbackHelper();
        CustomTabsClient.bindCustomTabsService(
                ApplicationProvider.getApplicationContext(),
                ApplicationProvider.getApplicationContext().getPackageName(),
                new CustomTabsServiceConnection() {
                    @Override
                    public void onServiceDisconnected(ComponentName name) {}

                    @Override
                    public void onCustomTabsServiceConnected(
                            ComponentName name, CustomTabsClient client) {
                        clientReference.set(client);
                        sessionReference.set(client.newSession(callback));
                        waitForConnection.notifyCalled();
                    }
                });
        waitForConnection.waitForCallback(0);
        return new ClientAndSession(clientReference.get(), sessionReference.get());
    }

    /** Calls warmup() and waits for all the tasks to complete. Fails the test otherwise. */
    public static CustomTabsConnection warmUpAndWait() throws TimeoutException {
        CustomTabsConnection connection = setUpConnection();
        final CallbackHelper startupCallbackHelper = new CallbackHelper();
        CustomTabsSession session =
                bindWithCallback(
                                new CustomTabsCallback() {
                                    @Override
                                    public void extraCallback(String callbackName, Bundle args) {
                                        if (callbackName.equals(
                                                CustomTabsConnection.ON_WARMUP_COMPLETED)) {
                                            startupCallbackHelper.notifyCalled();
                                        }
                                    }
                                })
                        .session;
        Assert.assertTrue(connection.warmup(0));
        startupCallbackHelper.waitForCallback(0, 1, 10, TimeUnit.SECONDS);
        return connection;
    }

    public static void openAppMenuAndAssertMenuShown(CustomTabActivity activity) {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    activity.onMenuOrKeyboardAction(R.id.show_menu, false);
                });

        CriteriaHelper.pollUiThread(
                activity.getRootUiCoordinatorForTesting()
                                .getAppMenuCoordinatorForTesting()
                                .getAppMenuHandler()
                        ::isAppMenuShowing,
                "App menu was not shown");
    }

    /**
     * @return The test bitmap which can be used to represent an action item on the Toolbar.
     */
    public static Bitmap createTestBitmap(int widthDp, int heightDp) {
        Resources testRes = ApplicationProvider.getApplicationContext().getResources();
        float density = testRes.getDisplayMetrics().density;
        return Bitmap.createBitmap(
                (int) (widthDp * density), (int) (heightDp * density), Bitmap.Config.ARGB_8888);
    }

    /**
     * @param id Id of the variation to search for.
     * @return true Whether id is a registered variation id.
     */
    public static boolean hasVariationId(int id) {
        return CustomTabsTestUtilsJni.get().hasVariationId(id);
    }

    /** Waits for the speculation of |url| for the |connection| to complete. */
    public static void ensureCompletedSpeculationForUrl(
            final CustomTabsConnection connection, final String url) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Tab was not created",
                            connection.getSpeculationParamsForTesting(),
                            Matchers.notNullValue());
                },
                LONG_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        ChromeTabUtils.waitForTabPageLoaded(connection.getSpeculationParamsForTesting().tab, url);
    }

    /**
     * Asserts that the number of items in {@code list} matches the {@code expectedSize}.
     *
     * @param list The list of items in the menu.
     * @param expectedSize The number of expected menu items.
     */
    public static void assertMenuSize(ModelList list, int expectedSize) {
        assertEquals("Populated menu items were:" + getMenuTitles(list), expectedSize, list.size());
    }

    /**
     * @param list The list of items in the menu.
     * @return A string containing the titles of all items in the {@code list}.
     */
    public static String getMenuTitles(ModelList list) {
        StringBuilder items = new StringBuilder();
        for (int i = 0; i < list.size(); i++) {
            PropertyModel model = list.get(i).model;
            items.append("\n").append(model.get(AppMenuItemProperties.TITLE));
            if (model.get(AppMenuItemProperties.SUBMENU) != null) {
                for (var submenu : model.get(AppMenuItemProperties.SUBMENU)) {
                    items.append("\n - ").append(submenu.model.get(AppMenuItemProperties.TITLE));
                }
            }
        }
        return items.toString();
    }

    @NativeMethods
    interface Natives {
        boolean hasVariationId(int id);
    }
}
