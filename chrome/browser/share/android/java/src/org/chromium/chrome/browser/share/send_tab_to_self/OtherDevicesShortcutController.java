// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.RequiresApi;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.intents.BrowserIntentUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.sync_device_info.FormFactor;

import java.util.ArrayList;
import java.util.List;

/**
 * Controller that pushes dynamic shortcuts corresponding to other syncing devices from the same
 * user to the system. These shortcuts appear (a) as DirectShare targets in the system share sheet,
 * where they trigger a "Send tab to self" flow, and (b) as launcher shortcuts when long-pressing on
 * the app icon, in which case they open the "Recent tabs" page for the respective device.
 */
@NullMarked
public class OtherDevicesShortcutController implements Destroyable {
    public static final String ACTION_OPEN_RECENT_TABS =
            "org.chromium.chrome.browser.share.send_tab_to_self.action.OPEN_RECENT_TABS";
    public static final String EXTRA_DEVICE_GUID =
            "org.chromium.chrome.browser.share.send_tab_to_self.extra.DEVICE_GUID";
    public static final String EXTRA_DEVICE_NAME =
            "org.chromium.chrome.browser.share.send_tab_to_self.extra.DEVICE_NAME";

    private static final String SHORTCUT_ID_PREFIX = "stts-target-";

    // Limit to 2 devices to avoid overcrowding the share sheet and the launcher.
    private static final int MAX_SHORTCUTS = 2;

    private static final String TAG = "SttsShortcut";

    private static final SequencedTaskRunner sTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.USER_VISIBLE_MAY_BLOCK);

    private final Profile mProfile;
    private final Context mContext;
    private long mNativeDeviceInfoObserverBridge;
    private long mNativeModelObserverBridge;

    /**
     * Handles the ACTION_OPEN_RECENT_TABS intent when a launcher shortcut is selected. Meant to be
     * called from LauncherShortcutActivity.
     *
     * @param activity The activity receiving the intent.
     * @param intent The intent to handle.
     * @return Whether the intent was handled.
     */
    public static boolean handleLauncherShortcutIntent(Activity activity, Intent intent) {
        if (!ChromeFeatureList.sSendTabToSelfDynamicShortcuts.isEnabled()) {
            return false;
        }
        if (!ACTION_OPEN_RECENT_TABS.equals(intent.getAction())) return false;

        // This is a launcher shortcut intent. Re-launch it as a trusted ACTION_VIEW intent.
        String guid = intent.getStringExtra(EXTRA_DEVICE_GUID);
        if (TextUtils.isEmpty(guid)) return false;

        Intent trustedIntent =
                new Intent(
                        Intent.ACTION_VIEW, Uri.parse(UrlConstants.RECENT_TABS_URL + "#" + guid));
        trustedIntent.setPackage(activity.getPackageName());
        IntentUtils.addTrustedIntentExtras(trustedIntent);
        LaunchIntentDispatcher.dispatchToTabbedActivity(activity, trustedIntent);
        return true;
    }

    public OtherDevicesShortcutController(Profile profile) {
        assert (profile != null);
        mProfile = profile;
        mContext = ContextUtils.getApplicationContext();

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return;

        if (ChromeFeatureList.sSendTabToSelfDynamicShortcuts.isEnabled()) {
            mNativeDeviceInfoObserverBridge =
                    SendTabToSelfAndroidBridge.addDeviceInfoObserver(
                            mProfile, this::updateShortcuts);
            mNativeModelObserverBridge =
                    SendTabToSelfAndroidBridge.addModelObserver(mProfile, this::updateShortcuts);
        }
        updateShortcuts();
    }

    @Override
    public void destroy() {
        if (mNativeDeviceInfoObserverBridge != 0) {
            SendTabToSelfAndroidBridge.removeDeviceInfoObserver(mNativeDeviceInfoObserverBridge);
            mNativeDeviceInfoObserverBridge = 0;
        }
        if (mNativeModelObserverBridge != 0) {
            SendTabToSelfAndroidBridge.removeModelObserver(mNativeModelObserverBridge);
            mNativeModelObserverBridge = 0;
        }
    }

    @RequiresApi(Build.VERSION_CODES.R)
    private void updateShortcuts() {
        List<ShortcutInfo> newShortcuts = new ArrayList<>();

        if (ChromeFeatureList.sSendTabToSelfDynamicShortcuts.isEnabled()) {
            List<TargetDeviceInfo> devices =
                    SendTabToSelfAndroidBridge.getAllTargetDeviceInfos(mProfile);
            // TODO(crbug.com/484887324): Consider filtering out devices which won't show up in the
            // "Recent Tabs" page - this may happen if a device has no eligible open tabs.

            // LauncherShortcutActivity may create up to 2 dynamic shortcuts, which should always
            // appear before the STTS shortcuts.
            // TODO(crbug.com/484887324): Introduce a common manager class for all dynamic
            // shortcuts.
            int nextRank = 2;
            // Limit the number of devices to avoid overcrowding the share sheet and the launcher.
            // The list of devices is sorted, so the most-recently-used devices will be used for
            // shortcuts.
            for (int i = 0; i < Math.min(devices.size(), MAX_SHORTCUTS); i++) {
                TargetDeviceInfo device = devices.get(i);

                Intent intent = new Intent(ACTION_OPEN_RECENT_TABS);
                intent.setClassName(
                        mContext, BrowserIntentUtils.LAUNCHER_SHORTCUT_ACTIVITY_CLASS_NAME);
                intent.putExtra(EXTRA_DEVICE_GUID, device.cacheGuid);
                intent.putExtra(IntentHandler.EXTRA_INVOKED_FROM_SHORTCUT, true);

                Icon icon = createAdaptiveIcon(device.formFactor);

                // The ID passed to the constructor will become EXTRA_SHORTCUT_ID in the received
                // Intent.
                String id = SHORTCUT_ID_PREFIX + device.cacheGuid;
                ShortcutInfo shortcut =
                        new ShortcutInfo.Builder(mContext, id)
                                .setShortLabel(device.deviceName)
                                // TODO(crbug.com/484887324): Include the email in the long label?
                                .setLongLabel(device.deviceName)
                                .setIcon(icon)
                                .setIntent(intent)
                                .setRank(nextRank++)
                                .build();
                newShortcuts.add(shortcut);
            }
        }
        // Note: The code below to update the shortcuts still runs even if the feature flag is
        // disabled. This is to clean up any remaining shortcuts from a previous run, which would
        // not be functional anymore with the feature flag disabled.

        // Updating the shortcuts must be done on a background thread since it may involve IPCs.
        Context context = mContext;
        sTaskRunner.execute(
                () -> {
                    ShortcutManager shortcutManager =
                            context.getSystemService(ShortcutManager.class);
                    if (shortcutManager == null) return;

                    // Remove old STTS shortcuts that are no longer valid.
                    List<ShortcutInfo> existingShortcuts = new ArrayList<>();
                    existingShortcuts.addAll(shortcutManager.getDynamicShortcuts());
                    existingShortcuts.addAll(shortcutManager.getPinnedShortcuts());

                    List<String> newShortcutIds = new ArrayList<>();
                    for (ShortcutInfo shortcut : newShortcuts) {
                        newShortcutIds.add(shortcut.getId());
                    }

                    List<String> shortcutIdsToRemove = new ArrayList<>();
                    for (ShortcutInfo existingShortcut : existingShortcuts) {
                        String id = existingShortcut.getId();
                        if (id.startsWith(SHORTCUT_ID_PREFIX)
                                && !newShortcutIds.contains(id)
                                && !shortcutIdsToRemove.contains(id)) {
                            shortcutIdsToRemove.add(id);
                        }
                    }
                    if (!shortcutIdsToRemove.isEmpty()) {
                        try {
                            shortcutManager.disableShortcuts(shortcutIdsToRemove);
                            shortcutManager.removeDynamicShortcuts(shortcutIdsToRemove);
                            shortcutManager.removeLongLivedShortcuts(shortcutIdsToRemove);
                        } catch (IllegalArgumentException e) {
                            Log.e(TAG, "Trying to remove immutable shortcuts", e);
                        } catch (IllegalStateException e) {
                            Log.e(TAG, "Failed to remove dynamic shortcuts", e);
                        }
                    }

                    // Finally, add the new shortcuts, or update them if they already existed.
                    if (!newShortcuts.isEmpty()) {
                        assert ChromeFeatureList.sSendTabToSelfDynamicShortcuts.isEnabled();
                        try {
                            boolean result = shortcutManager.addDynamicShortcuts(newShortcuts);
                            Log.d(TAG, "Set " + newShortcuts.size() + " shortcuts: " + result);
                        } catch (IllegalArgumentException e) {
                            Log.e(TAG, "Max number of dynamic shortcuts exceeded", e);
                        } catch (IllegalStateException e) {
                            Log.e(TAG, "Failed to add dynamic shortcuts", e);
                        }
                    }
                });
    }

    private Icon createAdaptiveIcon(@FormFactor int formFactor) {
        int iconRes = getIconResForFormFactor(formFactor);
        Drawable drawable = mContext.getDrawable(iconRes);
        if (drawable == null) {
            return Icon.createWithResource(mContext, iconRes);
        }

        // See developer docs for adaptive icons:
        // https://developer.android.com/develop/ui/compose/system/icon_design_adaptive
        // https://developer.android.com/reference/android/graphics/drawable/AdaptiveIconDrawable
        // In particular, the icon should be 108x108 dp, but the outer 18 dp on each side may not be
        // used (reserved for use by the system UI). In addition, the remaining 72x72 dp will be
        // further masked (e.g. to a circle), leaving only a 66 dp-diameter circle as the safe zone
        // that will not be cropped. This fits at most a 46 dp square icon.
        Resources res = mContext.getResources();
        int size = (int) (res.getDisplayMetrics().density * 108);
        int iconSize = (int) (res.getDisplayMetrics().density * 46);
        Bitmap bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888);
        bitmap.eraseColor(Color.WHITE);

        // Center the icon in the bitmap.
        Canvas canvas = new Canvas(bitmap);
        int offset = (size - iconSize) / 2;
        drawable.setBounds(offset, offset, offset + iconSize, offset + iconSize);
        drawable.draw(canvas);

        return Icon.createWithAdaptiveBitmap(bitmap);
    }

    private static int getIconResForFormFactor(@FormFactor int formFactor) {
        switch (formFactor) {
            case FormFactor.DESKTOP:
                return R.drawable.computer_black_24dp;
            case FormFactor.PHONE:
                return R.drawable.smartphone_black_24dp;
            case FormFactor.TABLET:
                return R.drawable.tablet_black_24dp;
            default:
                return R.drawable.devices_black_24dp;
        }
    }
}
