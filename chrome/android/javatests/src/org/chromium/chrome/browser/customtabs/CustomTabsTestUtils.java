// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Bundle;
import android.os.Process;
import android.support.test.InstrumentationRegistry;
import android.view.Menu;
import android.view.MenuItem;

import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsClient;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsServiceConnection;
import androidx.browser.customtabs.CustomTabsSession;

import org.junit.Assert;

import org.chromium.base.IntentUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Utility class that contains convenience calls related with custom tabs testing.
 */
@JNINamespace("customtabs")
public class CustomTabsTestUtils {
    /** Intent extra to specify an id to a custom tab.*/
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

    /**
     * A utility class to ensure that a pending intent assigned to a menu item in CCT was invoked.
     */
    public static class OnFinishedForTest implements PendingIntent.OnFinished {
        private final PendingIntent mPendingIntent;
        private final CallbackHelper mCallbackHelper = new CallbackHelper();
        private Intent mCallbackIntent;

        /**
         * Create an instance of {@link OnFinishedForTest}, testing the given {@link PendingIntent}.
         */
        public OnFinishedForTest(PendingIntent pendingIntent) {
            mPendingIntent = pendingIntent;
        }

        public Intent getCallbackIntent() {
            return mCallbackIntent;
        }

        public void waitForCallback(String failureReason) throws TimeoutException {
            mCallbackHelper.waitForCallback(failureReason, 0);
        }

        @Override
        public void onSendFinished(PendingIntent pendingIntent, Intent intent, int resultCode,
                String resultData, Bundle resultExtras) {
            if (pendingIntent.equals(mPendingIntent)) {
                mCallbackIntent = intent;
                mCallbackHelper.notifyCalled();
            }
        }
    }

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

    /**
     * Creates the simplest intent that that is sufficient to let {@link ChromeLauncherActivity}
     * launch the incognito {@link CustomTabActivity}.
     *
     * @param context The instrumentation context to use.
     * @param url The URL to load in the incognito CCT.
     * @return Returns the intent to launch the incognito CCT.
     */
    public static Intent createMinimalIncognitoCustomTabIntent(Context context, String url) {
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, url);
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        return intent;
    }

    public static CustomTabsConnection setUpConnection() {
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.resetThrottling(Process.myUid());
        return connection;
    }

    public static void cleanupSessions(final CustomTabsConnection connection) {
        TestThreadUtils.runOnUiThreadBlocking(connection::cleanupAllForTesting);
    }

    public static ClientAndSession bindWithCallback(final CustomTabsCallback callback)
            throws TimeoutException {
        final AtomicReference<CustomTabsSession> sessionReference = new AtomicReference<>();
        final AtomicReference<CustomTabsClient> clientReference = new AtomicReference<>();
        final CallbackHelper waitForConnection = new CallbackHelper();
        CustomTabsClient.bindCustomTabsService(InstrumentationRegistry.getContext(),
                InstrumentationRegistry.getTargetContext().getPackageName(),
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
        CustomTabsSession session = bindWithCallback(new CustomTabsCallback() {
            @Override
            public void extraCallback(String callbackName, Bundle args) {
                if (callbackName.equals(CustomTabsConnection.ON_WARMUP_COMPLETED)) {
                    startupCallbackHelper.notifyCalled();
                }
            }
        }).session;
        Assert.assertTrue(connection.warmup(0));
        startupCallbackHelper.waitForCallback(0);
        return connection;
    }

    public static void openAppMenuAndAssertMenuShown(CustomTabActivity activity) {
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> { activity.onMenuOrKeyboardAction(R.id.show_menu, false); });

        CriteriaHelper.pollUiThread(activity.getRootUiCoordinatorForTesting()
                                            .getAppMenuCoordinatorForTesting()
                                            .getAppMenuHandler()::isAppMenuShowing,
                "App menu was not shown");
    }

    public static int getVisibleMenuSize(Menu menu) {
        int visibleMenuSize = 0;
        for (int i = 0; i < menu.size(); i++) {
            MenuItem item = menu.getItem(i);
            if (item.isVisible()) visibleMenuSize++;
        }
        return visibleMenuSize;
    }

    /**
     * Add a bundle specifying a a number of custom menu entries.
     *
     * @param customTabIntent The intent to modify.
     * @param numEntries The number of menu entries to add.
     * @param menuTitle The title of the menu.
     *
     * @return The pending intent associated with the menu entries.
     */
    public static PendingIntent addMenuEntriesToIntent(
            Intent customTabIntent, int numEntries, String menuTitle) {
        return addMenuEntriesToIntent(customTabIntent, numEntries, new Intent(), menuTitle);
    }

    /**
     * Add a bundle specifying a custom menu entry.
     *
     * @param customTabIntent The intent to modify.
     * @param numEntries The number of menu entries to add.
     * @param callbackIntent The intent to use as the base for the pending intent.
     * @param menuTitle The title of the menu.
     *
     * @return The pending intent associated with the menu entry.
     */
    public static PendingIntent addMenuEntriesToIntent(
            Intent customTabIntent, int numEntries, Intent callbackIntent, String menuTitle) {
        PendingIntent pi = PendingIntent.getBroadcast(InstrumentationRegistry.getTargetContext(), 0,
                callbackIntent,
                PendingIntent.FLAG_UPDATE_CURRENT
                        | IntentUtils.getPendingIntentMutabilityFlag(true));
        ArrayList<Bundle> menuItems = new ArrayList<>();
        for (int i = 0; i < numEntries; i++) {
            Bundle bundle = new Bundle();
            bundle.putString(CustomTabsIntent.KEY_MENU_ITEM_TITLE, menuTitle);
            bundle.putParcelable(CustomTabsIntent.KEY_PENDING_INTENT, pi);
            menuItems.add(bundle);
        }
        customTabIntent.putParcelableArrayListExtra(CustomTabsIntent.EXTRA_MENU_ITEMS, menuItems);
        return pi;
    }

    /**
     * Creates a CCT Toolbar menu item bundle.
     *
     * @param icon The Bitmap icon to be add in the toolbar.
     * @param description The description about the icon which will be added.
     * @param pi The pending intent that would be triggered when the icon is clicked on.
     * @param id A unique id for this new icon.
     *
     * @return Returns the bundle encapsulating the toolbar item.
     */
    public static Bundle makeToolbarItemBundle(
            Bitmap icon, String description, PendingIntent pi, int id) {
        Bundle bundle = new Bundle();
        bundle.putInt(CustomTabsIntent.KEY_ID, id);
        bundle.putParcelable(CustomTabsIntent.KEY_ICON, icon);
        bundle.putString(CustomTabsIntent.KEY_DESCRIPTION, description);
        bundle.putParcelable(CustomTabsIntent.KEY_PENDING_INTENT, pi);
        bundle.putBoolean(CustomButtonParamsImpl.SHOW_ON_TOOLBAR, true);
        return bundle;
    }

    /**
     * Adds an action button to the custom tab toolbar.
     *
     * @param intent The intent where the action button would be added.
     * @param icon The icon representing the action button.
     * @param description The description associated with the action button.
     * @param id The unique id that would be used for this new Action button.
     *
     * @return The {@link PendingIntent} that will be triggered when the action button is clicked.
     */
    public static PendingIntent addActionButtonToIntent(
            Intent intent, Bitmap icon, String description, int id) {
        PendingIntent pi = PendingIntent.getBroadcast(
                InstrumentationRegistry.getTargetContext(), 0, new Intent(), 0);
        intent.putExtra(CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE,
                makeToolbarItemBundle(icon, description, pi, id));
        return pi;
    }

    /**
     * @return The test bitmap which can be used to represent an action item on the Toolbar.
     */
    public static Bitmap createTestBitmap(int widthDp, int heightDp) {
        Resources testRes = InstrumentationRegistry.getTargetContext().getResources();
        float density = testRes.getDisplayMetrics().density;
        return Bitmap.createBitmap(
                (int) (widthDp * density), (int) (heightDp * density), Bitmap.Config.ARGB_8888);
    }

    /**
     * Sets the {@link CustomTabsIntent.ShareState} of the custom tab.
     * @param intent The intent to modify.
     * @param shareState The {@link CustomTabsIntent.ShareState} being set.
     */
    public static void setShareState(Intent intent, int shareState) {
        intent.putExtra(CustomTabsIntent.EXTRA_SHARE_STATE, shareState);
    }

    /**
     * @param id Id of the variation to search for.
     *
     * @return true Whether id is a registered variation id.
     */
    public static boolean hasVariationId(int id) {
        return CustomTabsTestUtilsJni.get().hasVariationId(id);
    }

    @NativeMethods
    interface Natives {
        boolean hasVariationId(int id);
    }
}
