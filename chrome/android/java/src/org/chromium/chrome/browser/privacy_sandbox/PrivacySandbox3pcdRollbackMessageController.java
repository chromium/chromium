// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.page_info.SiteSettingsHelper;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

/** Shows a message notifying the user that they have been removed from the 3PCD 1% experiment. */
@NullMarked
public class PrivacySandbox3pcdRollbackMessageController {
    private final Context mContext;
    private final Profile mProfile;
    private final ActivityTabProvider mActivityTabProvider;
    private final MessageDispatcher mMessageDispatcher;
    private @Nullable ActivityTabTabObserver mActivityTabTabObserver;

    public PrivacySandbox3pcdRollbackMessageController(
            Context context,
            Profile profile,
            ActivityTabProvider activityTabProvider,
            MessageDispatcher messageDispatcher) {
        mContext = context;
        mProfile = profile;
        mActivityTabProvider = activityTabProvider;
        mMessageDispatcher = messageDispatcher;
        if (!UserPrefs.get(profile).getBoolean(Pref.BLOCK_ALL3PC_TOGGLE_ENABLED)) {
            TrackingProtectionSettingsBridge.maybeSetRollbackPrefsModeB(profile);
        }
        createActivityTabTabObserver(profile);
    }

    public void destroy() {
        destroyActivityTabTabObserver();
    }

    public boolean maybeShow() {
        final PrefService prefService = UserPrefs.get(mProfile);
        // The message should only be shown for regular profiles when the associated pref and
        // feature are true.
        if (!prefService.getBoolean(Pref.SHOW_ROLLBACK_UI_MODE_B)
                || mProfile.isOffTheRecord()
                || !ChromeFeatureList.isEnabled(ChromeFeatureList.ROLL_BACK_MODE_B)) {
            return false;
        }

        Resources resources = mContext.getResources();
        Supplier<Integer> onPrimaryAction =
                () -> {
                    return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                };
        Callback<Boolean> onFullyVisible =
                (fullyVisible) -> {
                    if (fullyVisible) {
                        prefService.setBoolean(Pref.SHOW_ROLLBACK_UI_MODE_B, false);
                    }
                };
        PropertyModel message =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.MODE_B_ROLLBACK_MESSAGE)
                        .with(MessageBannerProperties.ICON_RESOURCE_ID, R.drawable.cookie_24dp)
                        .with(
                                MessageBannerProperties.DESCRIPTION,
                                resources.getString(R.string.mode_b_rollback_description))
                        .with(
                                MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID,
                                R.drawable.ic_settings_gear_24dp)
                        .with(
                                MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                resources.getString(R.string.mode_b_rollback_got_it))
                        .with(MessageBannerProperties.ON_PRIMARY_ACTION, onPrimaryAction)
                        .with(MessageBannerProperties.ON_FULLY_VISIBLE, onFullyVisible)
                        .with(
                                MessageBannerProperties.ON_DISMISSED,
                                (dismissReason) -> {
                                    recordMetrics(dismissReason);
                                })
                        .build();
        // When the settings icon is clicked, dismiss the message and navigate to cookie settings.
        message.set(
                MessageBannerProperties.ON_SECONDARY_ACTION,
                () -> {
                    mMessageDispatcher.dismissMessage(message, DismissReason.SECONDARY_ACTION);
                    SiteSettingsHelper.showCategorySettings(
                            mContext, SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
                });
        mMessageDispatcher.enqueueWindowScopedMessage(message, /* highPriority= */ true);
        // Stop observation to ensure the message is only ever queued once.
        destroyActivityTabTabObserver();
        return true;
    }

    private static void recordMetrics(@DismissReason int dismissReason) {
        switch (dismissReason) {
            case DismissReason.PRIMARY_ACTION:
                recordActionMetrics(RollBack3pcdNoticeAction.GOT_IT);
                return;
            case DismissReason.SECONDARY_ACTION:
                recordActionMetrics(RollBack3pcdNoticeAction.SETTINGS);
                return;
            case DismissReason.GESTURE:
            case DismissReason.CLOSE_BUTTON:
                recordActionMetrics(RollBack3pcdNoticeAction.CLOSED);
                return;
            default:
                RecordHistogram.recordBooleanHistogram(
                        "Privacy.3PCD.RollbackNotice.AutomaticallyDismissed", true);
        }
    }

    private static void recordActionMetrics(@RollBack3pcdNoticeAction int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "Privacy.3PCD.RollbackNotice.Action", action, RollBack3pcdNoticeAction.MAX_VALUE);
        RecordHistogram.recordBooleanHistogram(
                "Privacy.3PCD.RollbackNotice.AutomaticallyDismissed", false);
    }

    private void destroyActivityTabTabObserver() {
        if (mActivityTabTabObserver != null) {
            mActivityTabTabObserver.destroy();
            mActivityTabTabObserver = null;
        }
    }

    private void createActivityTabTabObserver(Profile profile) {
        mActivityTabTabObserver =
                new ActivityTabTabObserver(mActivityTabProvider) {
                    @Override
                    public void onDidStartNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigationHandle) {
                        // Offboard here iff the user *did not* block 3PCs in Mode B and therefore
                        // will not need their 3PC blocking state updated (setting 3PC blocking
                        // state here creates a startup race condition with local pref resolution).
                        if (!UserPrefs.get(profile).getBoolean(Pref.BLOCK_ALL3PC_TOGGLE_ENABLED)) {
                            TrackingProtectionSettingsBridge.maybeSetRollbackPrefsModeB(profile);
                        }
                    }

                    @Override
                    public void onDidFinishNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        if (tab == null) return;
                        // Offboard here iff the user *did* block 3PCs in Mode B. There's no
                        // material difference between doing this here and on DidStartNavigation, as
                        // the user will continue having 3PCs blocked, and this avoids the race.
                        if (UserPrefs.get(profile).getBoolean(Pref.BLOCK_ALL3PC_TOGGLE_ENABLED)) {
                            TrackingProtectionSettingsBridge.maybeSetRollbackPrefsModeB(profile);
                            return;
                        }
                        if (navigation.hasCommitted()) maybeShow();
                    }
                };
    }

    @VisibleForTesting
    @Nullable ActivityTabTabObserver getActivityTabTabObserver() {
        return mActivityTabTabObserver;
    }
}
