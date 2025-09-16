// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.content.res.Resources;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.page_info.SiteSettingsHelper;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

/** Shows a message notifying the user that they have been removed from the 3PCD 1% experiment. */
@NullMarked
public class PrivacySandbox3pcdRollbackMessageController {
    public static boolean maybeShow(
            Context context, Profile profile, MessageDispatcher messageDispatcher) {
        final PrefService prefService = UserPrefs.get(profile);
        // The message should only be shown for regular profiles when the associated pref and
        // feature are true.
        if (!prefService.getBoolean(Pref.SHOW_ROLLBACK_UI_MODE_B)
                || profile.isOffTheRecord()
                || !ChromeFeatureList.isEnabled(ChromeFeatureList.ROLL_BACK_MODE_B)) {
            return false;
        }

        Resources resources = context.getResources();
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
                        .build();
        // When the settings icon is clicked, dismiss the message and navigate to cookie settings.
        message.set(
                MessageBannerProperties.ON_SECONDARY_ACTION,
                () -> {
                    messageDispatcher.dismissMessage(message, DismissReason.SECONDARY_ACTION);
                    SiteSettingsHelper.showCategorySettings(
                            context, SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
                });
        messageDispatcher.enqueueWindowScopedMessage(message, /* highPriority= */ true);
        return true;
    }
}
