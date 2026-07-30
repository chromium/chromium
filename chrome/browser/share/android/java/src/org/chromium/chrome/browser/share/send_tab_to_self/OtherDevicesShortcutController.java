// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.app.Activity;
import android.app.Person;
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
import android.os.PersistableBundle;
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
import java.util.Collections;
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
    private static final String CATEGORY =
            "org.chromium.chrome.browser.share.send_tab_to_self.category.DEVICE";

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

    /**
     * Checks if the intent is a "Send Tab to Self" DirectShare target intent.
     *
     * @param intent The intent to check.
     * @return True if the intent is a Send Tab to Self DirectShare target intent.
     */
    static boolean isShareTargetIntent(Intent intent) {
        if (intent == null || !Intent.ACTION_SEND.equals(intent.getAction())) return false;
        String shortcutId = intent.getStringExtra(Intent.EXTRA_SHORTCUT_ID);
        return shortcutId != null && shortcutId.startsWith(SHORTCUT_ID_PREFIX);
    }

    /**
     * Handles forwarding the intent to the translucent activity if it is a "Send Tab to Self"
     * DirectShare target. Meant to be called from ChromeLauncherActivity.
     *
     * @param activity The activity receiving the intent.
     * @param intent The intent to handle.
     * @return Whether the intent was forwarded.
     */
    public static boolean handleShareTargetIntentForwarding(Activity activity, Intent intent) {
        if (!ChromeFeatureList.sSendTabToSelfDynamicShortcuts.isEnabled()) {
            return false;
        }
        if (!isShareTargetIntent(intent)) return false;

        Intent forwardIntent = new Intent(intent);
        forwardIntent.setClass(activity, SendTabToSelfShareTargetActivity.class);
        activity.startActivity(forwardIntent);
        return true;
    }

    /**
     * Handles the ACTION_SEND intent when a "Send Tab to Self" DirectShare target is selected.
     * Meant to be called from SendTabToSelfShareTargetActivity.
     *
     * <p>Note: This method requires native libraries to be loaded.
     *
     * @param activity The activity receiving the intent. This should be
     *     SendTabToSelfShareTargetActivity in practice.
     * @param intent The intent to handle.
     */
    static void handleShareTargetIntent(Activity activity, Intent intent, Profile profile) {
        if (!ChromeFeatureList.sSendTabToSelfDynamicShortcuts.isEnabled()) {
            return;
        }
        if (!isShareTargetIntent(intent)) return;

        String shortcutId = intent.getStringExtra(Intent.EXTRA_SHORTCUT_ID);
        assert shortcutId != null; // Guaranteed by isShareTargetIntent().

        Context appContext = activity.getApplicationContext();

        String url = IntentHandler.getUrlFromIntent(intent);
        if (TextUtils.isEmpty(url)) return;

        String title = intent.getStringExtra(Intent.EXTRA_SUBJECT);
        // Title is allowed to be empty!

        // Accessing the shortcut from ShortcutManager should be done on a background thread.
        sTaskRunner.execute(
                () -> {
                    ShortcutManager shortcutManager =
                            appContext.getSystemService(ShortcutManager.class);
                    if (shortcutManager == null) return;
                    ShortcutInfo selectedShortcut = null;
                    // Search through dynamic shortcuts for a match.
                    for (ShortcutInfo shortcut : shortcutManager.getDynamicShortcuts()) {
                        if (shortcutId.equals(shortcut.getId())) {
                            selectedShortcut = shortcut;
                            break;
                        }
                    }
                    if (selectedShortcut == null) return;

                    PersistableBundle extras = selectedShortcut.getExtras();
                    if (extras == null) return;

                    String targetDeviceSyncCacheGuid = extras.getString(EXTRA_DEVICE_GUID);
                    if (TextUtils.isEmpty(targetDeviceSyncCacheGuid)) return;

                    String targetDeviceName = extras.getString(EXTRA_DEVICE_NAME);
                    if (TextUtils.isEmpty(targetDeviceName)) return;

                    PostTask.postTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                SendTabToSelfAndroidBridge.sendTabToDevice(
                                        profile,
                                        null,
                                        targetDeviceSyncCacheGuid,
                                        targetDeviceName,
                                        url,
                                        title != null ? title : "",
                                        ShareEntryPoint.SHARE_SHEET_DIRECT_SHARE);
                            });
                });
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

                // Create a ShortcutInfo corresponding to `device`. Note that the shortcut has two
                // separate purposes, which use (overlapping) subsets of all the fields.
                // Launcher shortcut: Displayed using the shortcut's short/long label and icon. The
                // behavior is specified by the passed Intent.
                // DirectShare target: Displayed using the shortcut's short/long label, overlaid
                // with Chrome's icon. Does NOT use the passed Intent; instead the system generates
                // an ACTION_SEND Intent, which gets routed to the activity defined as the
                // share-target in launchershortcuts.xml (i.e. IntentDispatcher aka
                // ChromeLauncherActivity).

                Intent intent = new Intent(ACTION_OPEN_RECENT_TABS);
                intent.setClassName(
                        mContext, BrowserIntentUtils.LAUNCHER_SHORTCUT_ACTIVITY_CLASS_NAME);
                intent.putExtra(EXTRA_DEVICE_GUID, device.cacheGuid);
                intent.putExtra(IntentHandler.EXTRA_INVOKED_FROM_SHORTCUT, true);

                Icon icon = createAdaptiveIcon(device.formFactor);

                // Note: A Person can also have a name and an icon, but those are not used for the
                // display of DirectShare targets.
                // TODO(crbug.com/484887324): Is there any point in providing name/icon/key/uri?
                Person person = new Person.Builder().setImportant(true).build();

                PersistableBundle shortcutExtras = new PersistableBundle();
                shortcutExtras.putString(EXTRA_DEVICE_GUID, device.cacheGuid);
                shortcutExtras.putString(EXTRA_DEVICE_NAME, device.deviceName);

                // The ID passed to the constructor will become EXTRA_SHORTCUT_ID in the received
                // Intent.
                String id = SHORTCUT_ID_PREFIX + device.cacheGuid;
                ShortcutInfo shortcut =
                        new ShortcutInfo.Builder(mContext, id)
                                // Common fields:
                                .setShortLabel(device.deviceName)
                                // TODO(crbug.com/484887324): Include the email in the long label?
                                .setLongLabel(device.deviceName)
                                .setIcon(icon)
                                // For launcher shortcut:
                                .setIntent(intent)
                                .setRank(nextRank++)
                                // For DirectShare target:
                                .setCategories(Collections.singleton(CATEGORY))
                                .setLongLived(true)
                                .setPerson(person)
                                .setExtras(shortcutExtras)
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
                            for (ShortcutInfo shortcut : newShortcuts) {
                                shortcutManager.pushDynamicShortcut(shortcut);
                            }
                            Log.d(TAG, "Pushed " + newShortcuts.size() + " shortcuts");
                        } catch (IllegalArgumentException e) {
                            Log.e(TAG, "Tried to update immutable shortcut", e);
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
