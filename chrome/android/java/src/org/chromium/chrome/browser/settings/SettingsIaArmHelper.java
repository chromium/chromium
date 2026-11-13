// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;

import androidx.annotation.StringDef;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.settings.search.BaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Holds the layout of the top-level settings page used when SettingsIaReorganization is enabled,
 * for the experiment arm selected by the feature's "arm" param.
 */
@NullMarked
public final class SettingsIaArmHelper {
    /** Experiment arms of SettingsIaReorganization, set by its "arm" param. */
    @StringDef({Arm.DIRECTION_1, Arm.DIRECTION_2, Arm.DIRECTION_3})
    @Retention(RetentionPolicy.SOURCE)
    @interface Arm {
        String DIRECTION_1 = "direction_1";
        String DIRECTION_2 = "direction_2";
        String DIRECTION_3 = "direction_3";
    }

    static final String GROUP_USER_AND_ACCOUNT = "user_and_account";
    static final String GROUP_SAFETY_AND_PROTECTION = "safety_and_protection";
    static final String GROUP_PERSONALIZATION = "personalization";
    static final String GROUP_SUPPORT = "support";

    /** A top-level group of rows, shown as one card without a header. */
    static final class Group {
        /** The id of the group, e.g. used as the section id of its rows. */
        public final String id;

        /** The group's name, e.g. shown in settings search results. */
        public final @StringRes int nameRes;

        /** The preference keys of the rows in the group, in display order. */
        public final List<String> keys;

        Group(String id, @StringRes int nameRes, List<String> keys) {
            this.id = id;
            this.nameRes = nameRes;
            this.keys = List.copyOf(keys);
        }
    }

    private static final List<Group> GROUPS =
            List.of(
                    new Group(
                            GROUP_USER_AND_ACCOUNT,
                            R.string.prefs_group_user_and_account,
                            List.of(MainSettings.PREF_SIGN_IN, MainSettings.PREF_GOOGLE_SERVICES)),
                    new Group(
                            GROUP_SAFETY_AND_PROTECTION,
                            R.string.prefs_group_safety_and_protection,
                            List.of(
                                    MainSettings.PREF_PRIVACY,
                                    // TODO(crbug.com/581489048): Move site settings and safety
                                    // check into privacy and security for direction 1, and only
                                    // safety check for direction 2.
                                    MainSettings.PREF_CONTENT_SETTINGS,
                                    MainSettings.PREF_SAFETY_HUB,
                                    MainSettings.PREF_AUTOFILL_AND_PASSWORDS)),
                    new Group(
                            GROUP_PERSONALIZATION,
                            R.string.prefs_group_personalization,
                            List.of(
                                    MainSettings.PREF_APPEARANCE,
                                    // Shown only until they move into appearance.
                                    MainSettings.PREF_ADDRESS_BAR,
                                    MainSettings.PREF_HOMEPAGE,
                                    MainSettings.PREF_TABS,
                                    MainSettings.PREF_SEARCH_ENGINE,
                                    MainSettings.PREF_NOTIFICATIONS,
                                    MainSettings.PREF_ON_STARTUP,
                                    MainSettings.PREF_DEFAULT_BROWSER,
                                    MainSettings.PREF_GLIC)),
                    new Group(
                            GROUP_SUPPORT,
                            R.string.prefs_group_support,
                            List.of(
                                    MainSettings.PREF_LANGUAGES,
                                    MainSettings.PREF_DOWNLOADS,
                                    MainSettings.PREF_ACCESSIBILITY,
                                    MainSettings.PREF_DEVELOPER,
                                    MainSettings.PREF_ABOUT_CHROME)));

    private SettingsIaArmHelper() {}

    /** Returns the experiment arm, or {@link Arm#DIRECTION_1} if the param is not a known arm. */
    static @Arm String getArm() {
        String arm = ChromeFeatureList.sSettingsIaReorganizationArm.getValue();
        switch (arm) {
            case Arm.DIRECTION_2:
            case Arm.DIRECTION_3:
                return arm;
            default:
                return Arm.DIRECTION_1;
        }
    }

    /**
     * Returns the top-level groups in display order.
     *
     * <p>TODO(crbug.com/581489048): All arms use the same layout until directions 2 and 3 are
     * defined.
     */
    static List<Group> getGroups() {
        return GROUPS;
    }

    /**
     * Shows each top-level row under its group's name in the settings search results, since the old
     * category headers are no longer on the page.
     */
    static void setGroupHeaders(
            Context context, SettingsIndexData indexData, BaseSearchIndexProvider provider) {
        for (Group group : getGroups()) {
            String name = context.getString(group.nameRes);
            for (String key : group.keys) {
                String id = provider.getUniqueId(key);
                SettingsIndexData.Entry entry = indexData.getEntry(id);
                if (entry == null) continue;
                indexData.updateEntry(
                        id, new SettingsIndexData.Entry.Builder(entry).setHeader(name).build());
            }
        }
    }
}
