// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.ClipData;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.share.ShareParams.TargetChosenCallback;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/**
 * A helper class that provides additional Chrome-specific share functionality.
 */
public class ShareHelper extends org.chromium.components.browser_ui.share.ShareHelper {
    private ShareHelper() {}

    /**
     * Shares the params using the system share sheet.
     * @param params The share parameters.
     * @param profile The profile last shared component will be saved to, if |saveLastUsed| is set.
     * @param saveLastUsed True if the chosen share component should be saved for future reuse.
     */
    // TODO(crbug/1022172): Should be package-protected once modularization is complete.
    public static void shareWithSystemShareSheetUi(
            ShareParams params, @Nullable Profile profile, boolean saveLastUsed) {
        if (saveLastUsed) {
            params.setCallback(new SaveComponentCallback(profile, params.getCallback()));
        }
        ShareHelper.shareWithSystemShareSheetUi(params);
    }

    /**
     * Share directly with the provided share target.
     * @param params The container holding the share parameters.
     * @param component The component to share to, bypassing any UI.
     * @param profile The profile last shared component will be saved to, if |saveLastUsed| is set.
     * @param saveLastUsed True if the chosen share component should be saved for future reuse.
     */
    // TODO(crbug/1022172): Should be package-protected once modularization is complete.
    public static void shareDirectly(@NonNull ShareParams params, @NonNull ComponentName component,
            @Nullable Profile profile, boolean saveLastUsed) {
        // Save the component directly without using a SaveComponentCallback.
        if (saveLastUsed) {
            setLastShareComponentName(profile, component);
        }
        Intent intent = getShareIntent(params);
        intent.addFlags(Intent.FLAG_ACTIVITY_FORWARD_RESULT | Intent.FLAG_ACTIVITY_PREVIOUS_IS_TOP);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARING_HUB_LAUNCH_ADJACENT)) {
            intent.addFlags(Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);
        }
        intent.setComponent(component);
        fireIntent(params.getWindow(), intent, null);
    }

    /**
     * Share an image URI with an activity identified by the provided Component Name.
     * @param window The current window.
     * @param name The component name of the activity to share the image with.
     * @param imageUri The uri generated from the content provider of the image to share with the
     *         external activity.
     * @param contentUrl The web url shared along with the image as a text if set.
     */
    public static void shareImage(final WindowAndroid window, final Profile profile,
            final ComponentName name, Uri imageUri, @Nullable String contentUrl) {
        Intent shareIntent = getShareImageIntent(imageUri, contentUrl);
        if (name == null) {
            sendChooserIntent(window, shareIntent, new SaveComponentCallback(profile, null));
        } else {
            shareIntent.setComponent(name);
            fireIntent(window, shareIntent, null);
        }
    }

    /**
     * Convenience method to create an Intent to retrieve all the apps that support sharing text.
     */
    public static Intent getShareTextAppCompatibilityIntent() {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        intent.putExtra(Intent.EXTRA_SUBJECT, "");
        intent.putExtra(Intent.EXTRA_TEXT, "");
        intent.setType("text/plain");
        return intent;
    }

    /**
     * Convenience method to create an Intent to retrieve all the apps that support sharing image.
     */
    public static Intent getShareImageAppCompatibilityIntent() {
        return getShareImageIntent(null, null);
    }

    /**
     * Convenience method to create an Intent to retrieve all the apps that support sharing {@code
     * fileContentType}.
     */
    public static Intent getShareFileAppCompatibilityIntent(String fileContentType) {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        intent.setType(fileContentType);
        return intent;
    }

    /**
     * Gets the {@link ComponentName} of the app that was used to last share.
     */
    @Nullable
    public static ComponentName getLastShareComponentName() {
        SharedPreferencesManager preferencesManager = SharedPreferencesManager.getInstance();
        String name = preferencesManager.readString(
                ChromePreferenceKeys.SHARING_LAST_SHARED_COMPONENT_NAME, null);
        if (name == null) {
            return null;
        }
        return ComponentName.unflattenFromString(name);
    }

    /**
     * Get the icon and name of the most recently shared app by certain app.
     * @param shareIntent Intent used to get list of apps support sharing.
     * @return The Image and the String of the recently shared Icon.
     */
    public static Pair<Drawable, CharSequence> getShareableIconAndName(Intent shareIntent) {
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
            boolean retrieved = false;
            final PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
            try {
                // TODO(dtrainor): Make asynchronous and have a callback to update the menu.
                // https://crbug.com/729737
                try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                    directShareIcon = pm.getActivityIcon(component);
                    directShareTitle = pm.getActivityInfo(component, 0).loadLabel(pm);
                }
                retrieved = true;
            } catch (NameNotFoundException exception) {
                // Use the default null values.
            }
            RecordHistogram.recordBooleanHistogram(
                    "Android.IsLastSharedAppInfoRetrieved", retrieved);
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
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.SHARING_LAST_SHARED_COMPONENT_NAME,
                component.flattenToString());
        if (profile != null) {
            ShareHistoryBridge.addShareEntry(profile, component.flattenToString());
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
            setLastShareComponentName(mProfile, chosenComponent);
            if (mOriginalCallback != null) mOriginalCallback.onTargetChosen(chosenComponent);
        }

        @Override
        public void onCancel() {
            if (mOriginalCallback != null) mOriginalCallback.onCancel();
        }
    }

    private static Intent getShareImageIntent(@Nullable Uri imageUri, @Nullable String contentUrl) {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        intent.setType("image/jpeg");
        if (imageUri == null) {
            return intent;
        }

        intent.putExtra(Intent.EXTRA_STREAM, imageUri);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);

        // Add text, title and clip data preview for the image being shared.
        ContentResolver resolver = ContextUtils.getApplicationContext().getContentResolver();
        intent.setType(resolver.getType(imageUri));
        intent.setClipData(ClipData.newUri(resolver, null, imageUri));
        if (!TextUtils.isEmpty(contentUrl)) {
            intent.putExtra(Intent.EXTRA_TEXT, contentUrl);
        }
        return intent;
    }
}
