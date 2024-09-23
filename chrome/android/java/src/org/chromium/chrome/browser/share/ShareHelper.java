// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.os.Parcelable;
import android.service.chooser.ChooserAction;
import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.share.ShareParams.TargetChosenCallback;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** A helper class that provides additional Chrome-specific share functionality. */
public class ShareHelper extends org.chromium.components.browser_ui.share.ShareHelper {
    private static final String TAG = "AndroidShare";
    // TODO(crbug.com/40063301): Remove when Android OS provides this string.
    private static final String INTENT_EXTRA_CHOOSER_CUSTOM_ACTIONS =
            "android.intent.extra.CHOOSER_CUSTOM_ACTIONS";
    private static final String INTENT_EXTRA_CHOOSER_MODIFY_SHARE_ACTION =
            "android.intent.extra.CHOOSER_MODIFY_SHARE_ACTION";
    // The max number of custom actions supported for custom actions.
    private static final int MAX_CUSTOM_ACTION_SUPPORTED = 5;
    private static final int CUSTOM_ACTION_REQUEST_CODE_BASE = 112;
    @VisibleForTesting static final String EXTRA_SHARE_CUSTOM_ACTION = "EXTRA_SHARE_CUSTOM_ACTION";

    private ShareHelper() {}

    /**
     * Shares the params using the system share sheet.
     *
     * @param params The share parameters.
     * @param profile The profile last shared component will be saved to, if |saveLastUsed| is set.
     * @param saveLastUsed True if the chosen share component should be saved for future reuse.
     */
    // TODO(crbug.com/40106499): Should be package-protected once modularization is complete.
    public static void shareWithSystemShareSheetUi(
            ShareParams params, @Nullable Profile profile, boolean saveLastUsed) {
        shareWithSystemShareSheetUi(params, profile, saveLastUsed, null);
    }

    /**
     * Shares the params using the system share sheet with custom actinos.
     * @param params The share parameters.
     * @param profile The profile last shared component will be saved to, if |saveLastUsed| is set.
     * @param saveLastUsed True if the chosen share component should be saved for future reuse.
     * @param customActionProvider List of custom actions for Android share sheet.
     */
    public static void shareWithSystemShareSheetUi(
            ShareParams params,
            @Nullable Profile profile,
            boolean saveLastUsed,
            @Nullable ChromeCustomShareAction.Provider customActionProvider) {
        assert (customActionProvider == null || ChooserActionHelper.isSupported())
                : "Custom action is not supported.";

        recordShareSource(ShareSourceAndroid.ANDROID_SHARE_SHEET);
        if (saveLastUsed) {
            params.setCallback(new SaveComponentCallback(profile, params.getCallback()));
        }
        Intent intent = getShareIntent(params);

        sendChooserIntent(params.getWindow(), intent, params.getCallback(), customActionProvider);
    }

    /**
     * Share directly with the provided share target.
     *
     * @param params The container holding the share parameters.
     * @param component The component to share to, bypassing any UI.
     * @param profile The profile last shared component will be saved to, if |saveLastUsed| is set.
     * @param saveLastUsed True if the chosen share component should be saved for future reuse.
     */
    // TODO(crbug.com/40106499): Should be package-protected once modularization is complete.
    public static void shareDirectly(
            @NonNull ShareParams params,
            @NonNull ComponentName component,
            @Nullable Profile profile,
            boolean saveLastUsed) {
        // Save the component directly without using a SaveComponentCallback.
        if (saveLastUsed) {
            setLastShareComponentName(profile, component);
        }
        Intent intent = getShareIntent(params);
        intent.addFlags(Intent.FLAG_ACTIVITY_FORWARD_RESULT | Intent.FLAG_ACTIVITY_PREVIOUS_IS_TOP);
        intent.setComponent(component);
        try {
            fireIntent(params.getWindow(), intent, null);
        } catch (ActivityNotFoundException exception) {
            // In rare cases when the component set for the send intent is not found, swallow the
            // errors.
            Log.e(TAG, exception.getMessage());
            ChromePureJavaExceptionReporter.reportJavaException(exception);
        }
    }

    /**
     * Convenience method to create an Intent to retrieve all the apps that support sharing text.
     */
    public static List<ResolveInfo> getCompatibleAppsForSharingText() {
        return PackageManagerUtils.queryIntentActivities(
                getShareTextAppCompatibilityIntent(),
                PackageManager.MATCH_DEFAULT_ONLY | PackageManager.GET_RESOLVED_FILTER);
    }

    /**
     * Convenience method to create an Intent to retrieve all the apps that support sharing {@code
     * fileContentType}.
     */
    public static List<ResolveInfo> getCompatibleAppsForSharingFiles(String fileContentType) {
        return PackageManagerUtils.queryIntentActivities(
                getShareFileAppCompatibilityIntent(fileContentType),
                PackageManager.MATCH_DEFAULT_ONLY | PackageManager.GET_RESOLVED_FILTER);
    }

    /** Gets the {@link ComponentName} of the app that was used to last share. */
    public static @Nullable ComponentName getLastShareComponentName() {
        SharedPreferencesManager preferencesManager = ChromeSharedPreferences.getInstance();
        String name =
                preferencesManager.readString(
                        ChromePreferenceKeys.SHARING_LAST_SHARED_COMPONENT_NAME, null);
        if (name == null) {
            return null;
        }
        return ComponentName.unflattenFromString(name);
    }

    /** Convenience method to retrieve the most recent app that support sharing text. */
    public static Pair<Drawable, CharSequence> getShareableIconAndNameForText() {
        return getShareableIconAndName(getShareTextAppCompatibilityIntent());
    }

    /**
     * Convenience method to retrieve the most recent app that support sharing {@code
     * fileContentType}.
     */
    public static Pair<Drawable, CharSequence> getShareableIconAndNameForFileContentType(
            String fileContentType) {
        return getShareableIconAndName(getShareFileAppCompatibilityIntent(fileContentType));
    }

    /**
     * Get the icon and name of the most recently shared app by certain app.
     * @param shareIntent Intent used to get list of apps support sharing.
     * @return The Image and the String of the recently shared Icon.
     */
    private static Pair<Drawable, CharSequence> getShareableIconAndName(Intent shareIntent) {
        Drawable directShareIcon = null;
        CharSequence directShareTitle = null;

        final ComponentName component = getLastShareComponentName();
        boolean isComponentValid = false;
        if (component != null) {
            shareIntent.setPackage(component.getPackageName());
            List<ResolveInfo> resolveInfoList =
                    PackageManagerUtils.queryIntentActivities(shareIntent, 0);
            for (ResolveInfo info : resolveInfoList) {
                ActivityInfo ai = info.activityInfo;
                if (component.equals(new ComponentName(ai.applicationInfo.packageName, ai.name))) {
                    isComponentValid = true;
                    break;
                }
            }
        }
        if (isComponentValid) {
            final PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
            try {
                // TODO(dtrainor): Make asynchronous and have a callback to update the menu.
                // https://crbug.com/729737
                directShareIcon = pm.getActivityIcon(component);
                directShareTitle = pm.getActivityInfo(component, 0).loadLabel(pm);
            } catch (NameNotFoundException exception) {
                // Use the default null values.
            }
        }

        return new Pair<>(directShareIcon, directShareTitle);
    }

    /**
     * Share directly with the last used share target, and record its share source.
     * @param params The container holding the share parameters.
     */
    static void shareWithLastUsedComponent(@NonNull ShareParams params) {
        ComponentName component = getLastShareComponentName();
        if (component == null) return;
        assert params.getCallback() == null;
        recordShareSource(ShareSourceAndroid.DIRECT_SHARE);
        shareDirectly(params, component, null, false);
    }

    /**
     * Stores the component selected for sharing last time share was called by certain app.
     *
     * This method is public since it is used in tests to avoid creating share dialog.
     * @param component The {@link ComponentName} of the app selected for sharing.
     */
    @VisibleForTesting
    public static void setLastShareComponentName(Profile profile, ComponentName component) {
        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.SHARING_LAST_SHARED_COMPONENT_NAME,
                        component.flattenToString());
        if (profile != null) {
            ShareHistoryBridge.addShareEntry(profile, component.flattenToString());
        }
    }

    private static void sendChooserIntent(
            WindowAndroid window,
            Intent sharingIntent,
            @Nullable TargetChosenCallback callback,
            ChromeCustomShareAction.Provider customActions) {
        new CustomActionChosenReceiver(callback, customActions)
                .sendChooserIntent(window, sharingIntent);
    }

    private static Intent getShareTextAppCompatibilityIntent() {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        intent.putExtra(Intent.EXTRA_SUBJECT, "");
        intent.putExtra(Intent.EXTRA_TEXT, "");
        intent.setType("text/plain");
        return intent;
    }

    /**
     * Convenience method to create an Intent to retrieve all the apps that support sharing {@code
     * fileContentType}.
     */
    private static Intent getShareFileAppCompatibilityIntent(String fileContentType) {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        intent.setType(fileContentType);
        return intent;
    }

    /** Helper class for injecting extras into the sharing intents. */
    private static class CustomActionChosenReceiver extends TargetChosenReceiver {
        private final ChromeCustomShareAction.Provider mCustomActionProvider;
        private final Map<String, Runnable> mActionsMap = new HashMap<>();

        protected CustomActionChosenReceiver(
                @Nullable TargetChosenCallback callback,
                @Nullable ChromeCustomShareAction.Provider customActionProvider) {
            super(callback);
            mCustomActionProvider = customActionProvider;
        }

        // Override so this file can have access to call this protected method.
        @Override
        protected void sendChooserIntent(WindowAndroid windowAndroid, Intent sharingIntent) {
            super.sendChooserIntent(windowAndroid, sharingIntent);
        }

        @Override
        protected Intent getChooserIntent(WindowAndroid window, Intent sharingIntent) {
            Intent chooserIntent = super.getChooserIntent(window, sharingIntent);
            if (mCustomActionProvider == null || !ChooserActionHelper.isSupported()) {
                return chooserIntent;
            }

            List<ChromeCustomShareAction> chromeCustomShareActions =
                    mCustomActionProvider.getCustomActions();
            assert chromeCustomShareActions.size() <= MAX_CUSTOM_ACTION_SUPPORTED
                    : "Max number of actions supported:" + MAX_CUSTOM_ACTION_SUPPORTED;

            List<Parcelable> chooserActions = new ArrayList<>();
            Activity activity = window.getActivity().get();

            // Use different request code to avoid pending intent don't collision.
            int requestCode =
                    activity.getTaskId() * MAX_CUSTOM_ACTION_SUPPORTED
                            + CUSTOM_ACTION_REQUEST_CODE_BASE;
            for (var action : chromeCustomShareActions) {
                Parcelable chooserAction = createChooserAction(action, activity, requestCode++);
                chooserActions.add(chooserAction);
            }

            Parcelable[] customActions = chooserActions.toArray(new Parcelable[0]);
            chooserIntent.putExtra(Intent.EXTRA_CHOOSER_CUSTOM_ACTIONS, customActions);

            return chooserIntent;
        }

        @Override
        protected void onReceiveInternal(Context context, Intent intent) {
            String action = IntentUtils.safeGetStringExtra(intent, EXTRA_SHARE_CUSTOM_ACTION);
            if (!TextUtils.isEmpty(action)) {
                assert mActionsMap.get(action) != null : "Action <" + action + "> does not exists.";
                mActionsMap.get(action).run();
            }
        }

        private Parcelable createChooserAction(
                ChromeCustomShareAction action, Activity activity, int requestCode) {
            Intent sendBackIntent = createSendBackIntentWithFilteredAction();
            sendBackIntent.putExtra(EXTRA_SHARE_CUSTOM_ACTION, action.key);
            // Make custom action immutable, since it doesn't need change any chooser component.
            PendingIntent pendingIntent =
                    PendingIntent.getBroadcast(
                            activity,
                            requestCode,
                            sendBackIntent,
                            PendingIntent.FLAG_CANCEL_CURRENT
                                    | PendingIntent.FLAG_ONE_SHOT
                                    | PendingIntent.FLAG_IMMUTABLE);

            Parcelable chooserAction =
                    ChooserActionHelper.newChooserAction(action.icon, action.label, pendingIntent);
            mActionsMap.put(action.key, action.runnable);

            return chooserAction;
        }
    }

    /**
     * A {@link TargetChosenCallback} that wraps another callback, forwarding calls to it, and
     * saving the chosen component.
     */
    private static class SaveComponentCallback implements TargetChosenCallback {
        private TargetChosenCallback mOriginalCallback;
        private Profile mProfile;

        public SaveComponentCallback(
                @Nullable Profile profile, @Nullable TargetChosenCallback originalCallback) {
            mOriginalCallback = originalCallback;
            mProfile = profile;
        }

        @Override
        public void onTargetChosen(ComponentName chosenComponent) {
            if (chosenComponent != null) {
                setLastShareComponentName(mProfile, chosenComponent);
            }
            if (mOriginalCallback != null) mOriginalCallback.onTargetChosen(chosenComponent);
        }

        @Override
        public void onCancel() {
            if (mOriginalCallback != null) mOriginalCallback.onCancel();
        }
    }

    /** Helper class used to build Android custom action. */
    @VisibleForTesting
    public static class ChooserActionHelper {
        static boolean isSupported() {
            return Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE;
        }

        static Parcelable newChooserAction(Icon icon, String name, PendingIntent action) {
            if (!isSupported()) return null;
            return new ChooserAction.Builder(icon, name, action).build();
        }
    }
}
