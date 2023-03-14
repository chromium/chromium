// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.android_share_sheet;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.os.Parcelable;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeProvidedSharingOptionsProviderBase;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ShareContentTypeHelper;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.share.ShareParams.TargetChosenCallback;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.WindowAndroid;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Provider that constructs custom actions for Android share sheet.
 */
// TODO(https://crbug/1421447): Support custom actions in multi-instance.
class AndroidCustomActionProvider extends ChromeProvidedSharingOptionsProviderBase {
    private static final String TAG = "AndroidShare";
    private static final String EXTRA_SHARE_CUSTOM_ACTION = "EXTRA_SHARE_CUSTOM_ACTION";
    // Arbitrary request code used for pending intents of custom action.
    private static final Integer CUSTOM_ACTION_REQUEST_CODE = 112;

    private static CustomActionAgent sLastRegisteredAgent;
    private static String sCustomChooserAction;

    /**
     * Constructs a new {@link AndroidCustomActionProvider}.
     *
     * @param activity The current {@link Activity}.
     * @param windowAndroid The current window.
     * @param tabProvider Supplier for the current activity tab.
     * @param bottomSheetController The {@link BottomSheetController} for the current activity.
     * @param shareParams The {@link ShareParams} for the current share.
     * @param printTab A {@link Callback} that will print a given Tab.
     * @param isIncognito Whether incognito mode is enabled.
     * @param chromeOptionShareCallback A ChromeOptionShareCallback that can be used by
     * Chrome-provided sharing options.
     * @param featureEngagementTracker feature engagement tracker.
     * @param url Url to share.
     * @param profile The current profile of the User.
     */
    AndroidCustomActionProvider(Activity activity, WindowAndroid windowAndroid,
            Supplier<Tab> tabProvider, BottomSheetController bottomSheetController,
            ShareParams shareParams, Callback<Tab> printTab, boolean isIncognito,
            ChromeOptionShareCallback chromeOptionShareCallback, Tracker featureEngagementTracker,
            String url, Profile profile) {
        super(activity, windowAndroid, tabProvider, bottomSheetController, shareParams, printTab,
                isIncognito, chromeOptionShareCallback, featureEngagementTracker, url, profile);
    }

    /**
     * Create the list of Parcelable used as custom actions for Android share sheet.
     *
     * @param params The {@link ShareParams} for the current share.
     * @param chromeShareExtras The {@link ChromeShareExtras} for the current share, if exists.
     * @param isMultiWindow Whether the current activity is in multi-window mode.
     * @return List of custom action used for Android share sheet.
     */
    List<Parcelable> createCustomActions(
            ShareParams params, ChromeShareExtras chromeShareExtras, boolean isMultiWindow) {
        // TODO(https://crbug.com/1421783): Maybe fallback to Chrome's share sheet properly.
        if (!ChooserActionHelper.isSupported()) {
            return new ArrayList<>();
        }

        List<FirstPartyOption> options = getFirstPartyOptions(
                ShareContentTypeHelper.getContentTypes(params, chromeShareExtras),
                chromeShareExtras.getDetailedContentType(), isMultiWindow);

        CustomActionAgent agent = new CustomActionAgent(mActivity, options, params.getCallback());
        registerBroadcastReceiver(agent);
        return agent.getCustomActions();
    }

    private static String getBroadcastReceiverAction() {
        // Create custom action based on the context and channel. This is needed so actions
        // triggered by other Chrome channels will not receive signals.
        if (sCustomChooserAction == null) {
            sCustomChooserAction = ContextUtils.getApplicationContext().getPackageName() + "/"
                    + AndroidCustomActionProvider.class.getName() + "_ACTION";
        }
        return sCustomChooserAction;
    }

    private static void registerBroadcastReceiver(CustomActionAgent receiver) {
        ThreadUtils.assertOnUiThread();
        unregisterBroadcastReceiver();
        sLastRegisteredAgent = receiver;
        ContextUtils.registerNonExportedBroadcastReceiver(ContextUtils.getApplicationContext(),
                receiver, new IntentFilter(getBroadcastReceiverAction()));
    }

    @VisibleForTesting
    static void unregisterBroadcastReceiver() {
        ThreadUtils.assertOnUiThread();
        if (sLastRegisteredAgent != null) {
            sCustomChooserAction = null;
            ContextUtils.getApplicationContext().unregisterReceiver(sLastRegisteredAgent);
            sLastRegisteredAgent = null;
        }
    }

    //  extends ChromeProvidedSharingOptionsProviderBase:

    @Nullable
    @Override
    protected FirstPartyOption createScreenshotFirstPartyOption() {
        return null;
    }

    // TODO(https://crbug/1410201): Support long screenshot.
    @Nullable
    @Override
    protected FirstPartyOption createLongScreenshotsFirstPartyOption() {
        return null;
    }

    /** Helper class used to create and handle custom actions. */
    private static class CustomActionAgent extends BroadcastReceiver {
        private final Map<String, Runnable> mActionsMap = new HashMap<>();
        private final List<Parcelable> mCustomActions = new ArrayList<>();
        private final @Nullable TargetChosenCallback mCallback;

        /**
         * Create a CustomActionAgent which creates a list of custom actions, and knowing how to
         * handle them. To use, this class needs to be registered as BroadCastReceiver for the
         * application context.
         *
         * The wrapped {@link TargetChosenCallback} will be called when a custom action is selected.
         * This class does not handle {@link TargetChosenCallback#onCancel()}, as there's no such a
         * signal an custom action is "canceled". If the share sheet is canceled, such path is
         * handled by {@link
         * org.chromium.components.browser_ui.share.ShareHelper.TargetChosenReceiver}.
         *
         * @param context The application context
         * @param options List of options that used to build the custom actions.
         * @param callback Wrapped callback will be called when a custom action is selected.
         */
        CustomActionAgent(Context context, List<FirstPartyOption> options,
                @Nullable TargetChosenCallback callback) {
            mCallback = callback;

            for (FirstPartyOption option : options) {
                Parcelable chooserAction = makeCustomAction(context, option.featureNameForMetrics,
                        context.getString(option.iconLabel), option.icon);
                assert (chooserAction != null)
                    : "Chooser action is null even when |ChooserActionHelper.isSupported()|.";
                mActionsMap.put(option.featureNameForMetrics, option.onClickCallback.bind(null));
                mCustomActions.add(chooserAction);
            }
        }

        /**
         * @return The list of custom actions supported by this agent.
         */
        public List<Parcelable> getCustomActions() {
            return mCustomActions;
        }

        @Nullable
        Parcelable makeCustomAction(Context context, String action, String name, int drawableRes) {
            // Broadcasting intent has to be an explicit intent. (i.e. with package name)
            Intent broadcastingIntent = new Intent(getBroadcastReceiverAction());
            broadcastingIntent.setPackage(context.getPackageName());
            broadcastingIntent.putExtra(EXTRA_SHARE_CUSTOM_ACTION, action);

            // Adding intent extras since non-exported broadcast listener does not exist pre-T.
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
                IntentUtils.addTrustedIntentExtras(broadcastingIntent);
            }

            // Use unique request codes for each pending intent to not overriding each other.
            int requestCode = CUSTOM_ACTION_REQUEST_CODE + drawableRes;
            PendingIntent pendingIntent =
                    PendingIntent.getBroadcast(context, requestCode, broadcastingIntent,
                            PendingIntent.FLAG_CANCEL_CURRENT | PendingIntent.FLAG_IMMUTABLE);
            Icon icon = Icon.createWithResource(context, drawableRes);

            return ChooserActionHelper.newChooserAction(icon, name, pendingIntent);
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            assert sLastRegisteredAgent
                    == this : "CustomActionAgent is not tracked as last registered agent.";
            // Ignore untrusted intent pre-T.
            if (!BuildInfo.isAtLeastT() && !IntentUtils.isTrustedIntentFromSelf(intent)) return;

            unregisterBroadcastReceiver();
            String actionKey = IntentUtils.safeGetStringExtra(intent, EXTRA_SHARE_CUSTOM_ACTION);
            if (mActionsMap.containsKey(actionKey)) {
                mActionsMap.get(actionKey).run();
            } else {
                // TODO(https://crbug/1421447): Make an assert once we supported multi-instance.
                Log.w(TAG, "Selected custom action can not be found.");
            }

            ComponentName chosenComponent =
                    intent.getParcelableExtra(Intent.EXTRA_CHOSEN_COMPONENT);
            if (mCallback != null) {
                mCallback.onTargetChosen(chosenComponent);
            }
        }
    }

    /**
     * Helper class used to build Android custom action.
     */
    // TODO(https://crbug.com/1420388): Replace calls with Android OS chooser actions.
    @VisibleForTesting
    static class ChooserActionHelper {
        /**
         * Try to query if the builder class exists.
         */
        @SuppressWarnings("PrivateApi")
        static boolean isSupported() {
            try {
                Class.forName("android.service.chooser.ChooserAction$Builder");
                return true;
            } catch (ClassNotFoundException e) {
                return false;
            }
        }

        @SuppressWarnings("PrivateApi")
        static Parcelable newChooserAction(Icon icon, String name, PendingIntent action) {
            Parcelable parcelable = null;
            try {
                Class<?> chooserActionBuilderClass =
                        Class.forName("android.service.chooser.ChooserAction$Builder");
                Constructor<?> ctor = chooserActionBuilderClass.getConstructor(
                        Icon.class, CharSequence.class, PendingIntent.class);
                Object builder = ctor.newInstance(icon, name, action);
                parcelable =
                        (Parcelable) chooserActionBuilderClass.getMethod("build").invoke(builder);
            } catch (ClassNotFoundException | NoSuchMethodException | IllegalAccessException
                    | InvocationTargetException | InstantiationException e) {
                Log.w(TAG, "Building ChooserAction failed.", e);
            }
            return parcelable;
        }
    }
}
