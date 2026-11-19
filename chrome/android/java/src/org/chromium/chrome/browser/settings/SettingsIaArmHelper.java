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
import java.util.ArrayList;
import java.util.List;
import java.util.Set;

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

    /**
     * Rows that some arms move from the top level into privacy and security. Direction 1 nests
     * both, direction 2 nests only safety check and direction 3 keeps both on the top level.
     */
    private static final Set<String> NESTED_IN_PRIVACY_DIRECTION_1 =
            Set.of(MainSettings.PREF_CONTENT_SETTINGS, MainSettings.PREF_SAFETY_HUB);

    private static final Set<String> NESTED_IN_PRIVACY_DIRECTION_2 =
            Set.of(MainSettings.PREF_SAFETY_HUB);

    private static final List<Group> GROUPS_DIRECTION_1 =
            createGroups(NESTED_IN_PRIVACY_DIRECTION_1);
    private static final List<Group> GROUPS_DIRECTION_2 =
            createGroups(NESTED_IN_PRIVACY_DIRECTION_2);
    private static final List<Group> GROUPS_DIRECTION_3 = createGroups(Set.of());

    private SettingsIaArmHelper() {}

    /**
     * Returns the top-level groups in display order, leaving out {@code nestedInPrivacy}. The order
     * follows the desktop Glow Up layout.
     */
    private static List<Group> createGroups(Set<String> nestedInPrivacy) {
        return List.of(
                new Group(
                        GROUP_USER_AND_ACCOUNT,
                        R.string.prefs_group_user_and_account,
                        List.of(MainSettings.PREF_SIGN_IN, MainSettings.PREF_GOOGLE_SERVICES)),
                new Group(
                        GROUP_SAFETY_AND_PROTECTION,
                        R.string.prefs_group_safety_and_protection,
                        withoutNested(
                                nestedInPrivacy,
                                MainSettings.PREF_AUTOFILL_AND_PASSWORDS,
                                MainSettings.PREF_PRIVACY,
                                MainSettings.PREF_CONTENT_SETTINGS,
                                MainSettings.PREF_SAFETY_HUB)),
                new Group(
                        GROUP_PERSONALIZATION,
                        R.string.prefs_group_personalization,
                        List.of(
                                MainSettings.PREF_GLIC,
                                MainSettings.PREF_APPEARANCE,
                                // Shown only until they move into appearance.
                                MainSettings.PREF_ADDRESS_BAR,
                                MainSettings.PREF_HOMEPAGE,
                                MainSettings.PREF_TABS,
                                MainSettings.PREF_NOTIFICATIONS,
                                MainSettings.PREF_ON_STARTUP,
                                MainSettings.PREF_DEFAULT_BROWSER,
                                MainSettings.PREF_SEARCH_ENGINE)),
                new Group(
                        GROUP_SUPPORT,
                        R.string.prefs_group_support,
                        List.of(
                                MainSettings.PREF_LANGUAGES,
                                MainSettings.PREF_DOWNLOADS,
                                MainSettings.PREF_ACCESSIBILITY,
                                MainSettings.PREF_DEVELOPER,
                                MainSettings.PREF_ABOUT_CHROME)));
    }

    private static List<String> withoutNested(Set<String> nestedInPrivacy, String... keys) {
        List<String> result = new ArrayList<>();
        for (String key : keys) {
            if (!nestedInPrivacy.contains(key)) result.add(key);
        }
        return result;
    }

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

    /** Returns the top-level groups of the current arm in display order. */
    static List<Group> getGroups() {
        return getGroups(getArm());
    }

    /** Returns the top-level groups of {@code arm} in display order. */
    static List<Group> getGroups(@Arm String arm) {
        switch (arm) {
            case Arm.DIRECTION_2:
                return GROUPS_DIRECTION_2;
            case Arm.DIRECTION_3:
                return GROUPS_DIRECTION_3;
            default:
                return GROUPS_DIRECTION_1;
        }
    }

    /** Returns the keys of the top-level rows the current arm moves into privacy and security. */
    static Set<String> getRowsNestedInPrivacy() {
        return getRowsNestedInPrivacy(getArm());
    }

    /** Returns the keys of the top-level rows {@code arm} moves into privacy and security. */
    static Set<String> getRowsNestedInPrivacy(@Arm String arm) {
        switch (arm) {
            case Arm.DIRECTION_2:
                return NESTED_IN_PRIVACY_DIRECTION_2;
            case Arm.DIRECTION_3:
                return Set.of();
            default:
                return NESTED_IN_PRIVACY_DIRECTION_1;
        }
    }

    /**
     * Returns whether privacy and security shows the row of {@code mainSettingsKey}, a key of
     * {@link MainSettings}, instead of the top level.
     */
    public static boolean isNestedInPrivacy(String mainSettingsKey) {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.SETTINGS_IA_REORGANIZATION)
                && getRowsNestedInPrivacy().contains(mainSettingsKey);
    }

    /**
     * Updates the top-level settings search entries for the current arm: each row is shown under
     * its group's name, since the old category headers are no longer on the page, and rows nested
     * in privacy and security are removed from the top level.
     */
    static void updateSearchIndex(
            Context context, SettingsIndexData indexData, BaseSearchIndexProvider provider) {
        @Arm String arm = getArm();
        for (String key : getRowsNestedInPrivacy(arm)) {
            indexData.removeEntry(provider.getUniqueId(key));
        }
        for (Group group : getGroups(arm)) {
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
