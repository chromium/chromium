// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Bundle;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSession;
import androidx.test.core.app.ApplicationProvider;

import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;

import java.util.ArrayList;
import java.util.concurrent.TimeoutException;

/**
 * Utility class that contains convenience calls related with intent creation for custom tabs
 * testing.
 */
public class CustomTabsIntentTestUtils {
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
        public void onSendFinished(
                PendingIntent pendingIntent,
                Intent intent,
                int resultCode,
                String resultData,
                Bundle resultExtras) {
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
    public static Intent createMinimalCustomTabIntent(Context context, String url) {
        return createCustomTabIntent(context, url, /* launchAsNewTask= */ true, builder -> {});
    }

    /** Creates an Intent that launches a CustomTabActivity, allows some customization. */
    public static Intent createCustomTabIntent(
            Context context,
            String url,
            boolean launchAsNewTask,
            Callback<CustomTabsIntent.Builder> customizer) {
        CustomTabsIntent.Builder builder =
                new CustomTabsIntent.Builder(
                        CustomTabsSession.createMockSessionForTesting(
                                new ComponentName(context, ChromeLauncherActivity.class)));
        customizer.onResult(builder);
        CustomTabsIntent customTabsIntent = builder.build();
        Intent intent = customTabsIntent.intent;
        intent.setAction(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));
        if (launchAsNewTask) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
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
        Intent intent = createMinimalCustomTabIntent(context, url);
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        return intent;
    }

    /**
     * Creates the simplest intent that is sufficient to let {@link ChromeLauncherActivity} launch
     * the {@link CustomTabActivity}. Allows specification of a theme.
     *
     * @param context The instrumentation context to use.
     * @param url The URL to load in the incognito CCT.
     * @param inNightMode Whether the CCT should be launched in night mode.
     * @return Returns the intent to launch the incognito CCT.
     */
    public static Intent createMinimalCustomTabIntentWithTheme(
            Context context, String url, boolean inNightMode) {
        return createCustomTabIntent(
                context,
                url,
                /* launchAsNewTask= */ true,
                builder -> {
                    builder.setColorScheme(
                            inNightMode
                                    ? CustomTabsIntent.COLOR_SCHEME_DARK
                                    : CustomTabsIntent.COLOR_SCHEME_LIGHT);
                });
    }

    /**
     * Add a bundle specifying a a number of custom menu entries.
     *
     * @param customTabIntent The intent to modify.
     * @param numEntries The number of menu entries to add.
     * @param menuTitle The title of the menu.
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
     * @return The pending intent associated with the menu entry.
     */
    public static PendingIntent addMenuEntriesToIntent(
            Intent customTabIntent, int numEntries, Intent callbackIntent, String menuTitle) {
        PendingIntent pi =
                PendingIntent.getBroadcast(
                        ApplicationProvider.getApplicationContext(),
                        0,
                        callbackIntent,
                        PendingIntent.FLAG_UPDATE_CURRENT
                                | IntentUtils.getPendingIntentMutabilityFlag(false));
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
     * @return The {@link PendingIntent} that will be triggered when the action button is clicked.
     */
    public static PendingIntent addActionButtonToIntent(
            Intent intent, Bitmap icon, String description, int id) {
        PendingIntent pi =
                PendingIntent.getBroadcast(
                        ApplicationProvider.getApplicationContext(),
                        0,
                        new Intent(),
                        IntentUtils.getPendingIntentMutabilityFlag(true));
        intent.putExtra(
                CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE,
                makeToolbarItemBundle(icon, description, pi, id));
        return pi;
    }

    /**
     * Sets the {@link CustomTabsIntent.ShareState} of the custom tab.
     *
     * @param intent The intent to modify.
     * @param shareState The {@link CustomTabsIntent.ShareState} being set.
     */
    public static void setShareState(Intent intent, int shareState) {
        intent.putExtra(CustomTabsIntent.EXTRA_SHARE_STATE, shareState);
    }
}
