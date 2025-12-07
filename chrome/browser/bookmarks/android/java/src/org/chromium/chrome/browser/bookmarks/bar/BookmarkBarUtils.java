// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefChangeRegistrar.PrefObserver;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.DeviceFormFactor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collection;

/** Utilities for the bookmark bar which provides users with bookmark access from top chrome. */
@NullMarked
public class BookmarkBarUtils {

    /** Enumeration of view type identifiers for views which are rendered in the bookmark bar. */
    @IntDef({ViewType.ITEM})
    @Retention(RetentionPolicy.SOURCE)
    @interface ViewType {
        int ITEM = 1;
    }

    /**
     * Enum that defines the possible types of clicks on the Bookmark Bar. These values are
     * persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    // LINT.IfChange(BookmarkBarClickType)
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        BookmarkBarClickType.UNKNOWN,
        BookmarkBarClickType.BOOKMARK_BAR_URL,
        BookmarkBarClickType.BOOKMARK_BAR_FOLDER,
        BookmarkBarClickType.OVERFLOW_MENU,
        BookmarkBarClickType.ALL_BOOKMARKS,
        BookmarkBarClickType.POP_UP_URL,
        BookmarkBarClickType.POP_UP_FOLDER,
        BookmarkBarClickType.NUM_ENTRIES
    })
    public @interface BookmarkBarClickType {
        int UNKNOWN = 0;
        int BOOKMARK_BAR_URL = 1;
        int BOOKMARK_BAR_FOLDER = 2;
        int OVERFLOW_MENU = 3;
        int ALL_BOOKMARKS = 4;
        int POP_UP_URL = 5;
        int POP_UP_FOLDER = 6;
        int NUM_ENTRIES = 7;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/bookmarks/enums.xml:BookmarkBarClickType)

    /**
     * Enum that defines the possible reasons the bookmark bar may be shown or hidden. These values
     * are persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    // LINT.IfChange(BookmarkBarShownReason)
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        BookmarkBarShownReason.UNKNOWN,
        BookmarkBarShownReason.DISABLED_BY_USER_PREF,
        BookmarkBarShownReason.ENABLED_BY_USER_PREF,
        BookmarkBarShownReason.DISABLED_BY_DEVICE_PREF,
        BookmarkBarShownReason.ENABLED_BY_DEVICE_PREF,
        BookmarkBarShownReason.DISABLED_BY_FEATURE_PARAM,
        BookmarkBarShownReason.ENABLED_BY_FEATURE_PARAM,
    })
    public @interface BookmarkBarShownReason {
        int UNKNOWN = 0;
        int DISABLED_BY_USER_PREF = 1;
        int ENABLED_BY_USER_PREF = 2;
        int DISABLED_BY_DEVICE_PREF = 3;
        int ENABLED_BY_DEVICE_PREF = 4;
        int DISABLED_BY_FEATURE_PARAM = 5;
        int ENABLED_BY_FEATURE_PARAM = 6;
        int NUM_ENTRIES = 7;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/bookmarks/enums.xml:BookmarkBarShownReason)

    // Histogram names:
    public static final String TOGGLED_IN_SETTINGS = "Bookmarks.BookmarkBar.ToggledInSettings";
    public static final String TOGGLED_BY_KEYBOARD_SHORTCUT =
            "Bookmarks.BookmarkBar.ToggledByKeyboardShortcut";
    public static final String BOOKMARK_BAR_CLICK = "Bookmarks.BookmarkBar.Click";
    public static final String BOOKMARK_BAR_SHOWN_ON_START_UP =
            "Bookmarks.BookmarkBar.Android.ShownOnStartUp";
    public static final String BOOKMARK_BAR_SHOWN_ON_START_UP_REASON =
            "Bookmarks.BookmarkBar.Android.ShownOnStartUpReason";

    /** Whether the bookmark bar feature is forcibly allowed/disallowed for testing. */
    private static @Nullable Boolean sActivityStateBookmarkBarCompatibleForTesting;

    /** Whether the bookmark bar feature is forcibly enabled/disabled for testing. */
    private static @Nullable Boolean sDeviceBookmarkBarCompatibleForTesting;

    /** Whether the bookmark bar feature is forcibly visible/invisible for testing. */
    private static @Nullable Boolean sBookmarkBarVisibleForTesting;

    /** Whether the bookmark bar user setting is forcibly enabled/disabled for testing. */
    private static @Nullable Boolean sSettingEnabledForTesting;

    /** Collection in which to cache bookmark bar user setting observers for testing. */
    private static @Nullable Collection<PrefObserver> sSettingObserverCacheForTesting;

    private BookmarkBarUtils() {}

    /**
     * Returns true if the current state is compatible with the Bookmark Bar. The Bookmark Bar
     * requires certain device types, as well as certain activity states, e.g. window sizes. It may
     * be true that a device supports the Bookmark Bar and so the feature is exposed to the user for
     * this session, but, the user could be in a state where interaction with, or parts of, the
     * Bookmark Bar should be disabled.
     *
     * <p>Check this value in cases such as responding to user actions that interact with the
     * Bookmark Bar.
     *
     * <p>See {@link #isDeviceBookmarkBarCompatible(Context)} and {@link
     * #isWindowBookmarkBarCompatible(Context)}.
     *
     * @param context The context in which activity state compatibility should be assessed.
     * @return Whether the current activity state supports the Bookmark Bar.
     */
    public static boolean isActivityStateBookmarkBarCompatible(Context context) {
        if (sActivityStateBookmarkBarCompatibleForTesting != null) {
            return sActivityStateBookmarkBarCompatibleForTesting;
        }
        return isDeviceBookmarkBarCompatible(context) && isWindowBookmarkBarCompatible(context);
    }

    /**
     * Returns true if the device is compatible with, and can support, the Bookmark Bar, and
     * therefore if the feature should be exposed to the user. If true, user flows such as keyboard
     * shortcuts, IPH, settings toggles, device policies, etc should be present. This value should
     * always return the same value for a device. Compatible devices include Desktop, large tablets,
     * and (unfolded) foldables.
     *
     * <p>Check this value when determining which user actions to expose to users for the Bookmark
     * Bar.
     *
     * <p>See {@link #isWindowBookmarkBarCompatible(Context)} and {@link
     * #isActivityStateBookmarkBarCompatible(Context)}.
     *
     * <p>Note: This also checks the feature flag for simplicity for clients.
     *
     * @param context The context in which device compatibility should be assessed.
     * @return Whether the device supports the Bookmark Bar.
     */
    public static boolean isDeviceBookmarkBarCompatible(Context context) {
        if (sDeviceBookmarkBarCompatibleForTesting != null) {
            return sDeviceBookmarkBarCompatibleForTesting;
        }
        return ChromeFeatureList.sAndroidBookmarkBar.isEnabled()
                && DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }

    /**
     * Returns true if the current activity window is compatible with the Bookmark Bar. The Bookmark
     * Bar is disabled for narrow windows, so the window size needs to be of sufficient width for
     * the Bookmark Bar to be displayed. The current requirement is a width >= 412dp, see {@link
     * //chrome/android/java/res/values-w412dp/bools.xml}. This value is not constant for a device,
     * and can change based on user interactions.
     *
     * <p>Note: There is no reasonable use-case to check this in isolation, so it is private.
     *
     * @param context The context in which window compatibility should be assessed.
     * @return Whether the window supports the Bookmark Bar.
     */
    private static boolean isWindowBookmarkBarCompatible(Context context) {
        return context.getResources().getBoolean(R.bool.bookmark_bar_allowed);
    }

    /**
     * Returns true if the Bookmark Bar currently visible. The feature is visible when it is allowed
     * in the given context, and the show bookmark bar UserPref is enabled for the current user.
     * When on tablets, we do not use the UserPref and instead use the device preference.
     *
     * @param context The context in which compatibility should be assessed.
     * @param profile The profile for which the user UserPref should be assessed.
     * @return Whether the Bookmark Bar is currently visible.
     */
    public static boolean isBookmarkBarVisible(Context context, @Nullable Profile profile) {
        if (sBookmarkBarVisibleForTesting != null) {
            return sBookmarkBarVisibleForTesting;
        }

        if (!isActivityStateBookmarkBarCompatible(context)) {
            return false;
        }
        // On Desktop, we sync with the UserPrefs.
        // On tablets we use the device preference logic (policy (pref service)  > local pref
        // (shared pref) > FeatureParam).
        return DeviceInfo.isDesktop()
                ? isUserPrefsShowBookmarksBarEnabled(profile)
                : isDevicePrefShowBookmarksBarEnabled(profile);
    }

    /**
     * Returns whether the bookmark bar visibility is controlled by enterprise policy.
     *
     * @param profile The profile for which the policy should be assessed.
     * @return Whether the bookmark bar visibility is managed by the policy.
     */
    public static boolean isBookmarkBarManagedByPolicy(@Nullable Profile profile) {
        return profile != null
                ? getPrefService(profile).isManagedPreference(Pref.SHOW_BOOKMARK_BAR)
                : false;
    }

    /**
     * Returns the value of the bookmark bar visibility if the bookmark bar visibility is managed by
     * the enterprise policy.
     *
     * @param profile The profile for which the policy value should be retrieved.
     * @return The policy's value for showing the bookmark bar.
     */
    public static boolean isBookmarkBarEnabledByPolicy(@Nullable Profile profile) {
        assert isBookmarkBarManagedByPolicy(profile);
        return profile != null ? getPrefService(profile).getBoolean(Pref.SHOW_BOOKMARK_BAR) : false;
    }

    /**
     * Returns whether the bookmark bar visibility has a recommended value from a policy.
     *
     * @param profile The profile for which the policy should be assessed.
     * @return Whether a recommended value exists for the bookmark bar visibility preference.
     */
    public static boolean isBookmarkBarRecommended(@Nullable Profile profile) {
        return profile != null
                ? getPrefService(profile).hasRecommendation(Pref.SHOW_BOOKMARK_BAR)
                : false;
    }

    /**
     * Returns whether the user's current setting matches the recommended policy value. Should only
     * be called when isBookmarkBarRecommended is true.
     *
     * @param profile The profile for which the policy should be assessed.
     * @return Whether the user's setting matches the recommended value.
     */
    public static boolean isFollowingBookmarkBarRecommendation(@Nullable Profile profile) {
        assert isBookmarkBarRecommended(profile);
        return profile != null
                ? getPrefService(profile).isFollowingRecommendation(Pref.SHOW_BOOKMARK_BAR)
                : false;
    }

    /**
     * Returns whether the preference's value is currently sourced from a recommended policy. This
     * occurs when a recommendation is active and the user has not set their own overriding value,
     * effectively making the recommendation the default.
     *
     * @param profile The profile for which the policy should be assessed.
     * @return True if the preference is using the recommended value as its default.
     */
    public static boolean isBookmarkBarValueFromRecommendation(@Nullable Profile profile) {
        return profile != null
                ? getPrefService(profile).isRecommendedPreference(Pref.SHOW_BOOKMARK_BAR)
                : false;
    }

    // UserPrefs methods - used on Desktop.

    /**
     * Returns whether the bookmark bar should be shown based on the current user's UserPrefs. Note:
     * This is synced across devices for the user's profile.
     *
     * @param profile The profile for which the UserPref should be assessed.
     * @return The user's current preference for showing the bookmark bar.
     */
    public static boolean isUserPrefsShowBookmarksBarEnabled(@Nullable Profile profile) {
        if (sSettingEnabledForTesting != null) {
            return sSettingEnabledForTesting;
        }
        return profile != null ? getPrefService(profile).getBoolean(Pref.SHOW_BOOKMARK_BAR) : false;
    }

    /**
     * Sets whether the bookmark bar user setting is currently enabled.
     *
     * @param profile The profile for which the user setting should be set.
     * @param enabled Whether the user setting should be set to enabled/disabled.
     */
    public static void setUserPrefsShowBookmarksBar(
            Profile profile, boolean enabled, boolean fromKeyboardShortcut) {
        RecordHistogram.recordBooleanHistogram(
                fromKeyboardShortcut ? TOGGLED_BY_KEYBOARD_SHORTCUT : TOGGLED_IN_SETTINGS, enabled);
        getPrefService(profile).setBoolean(Pref.SHOW_BOOKMARK_BAR, enabled);
    }

    /**
     * Toggles the value of the show bookmarks bar UserPref for the current user.
     *
     * @param profile The profile for which the UserPref should be toggled.
     */
    public static void toggleUserPrefsShowBookmarksBar(
            Profile profile, boolean fromKeyboardShortcut) {
        setUserPrefsShowBookmarksBar(
                profile,
                !getPrefService(profile).getBoolean(Pref.SHOW_BOOKMARK_BAR),
                fromKeyboardShortcut);
    }

    // Device preferences methods - used on tablets.

    /**
     * Returns whether or not the bookmark bar should be shown based on the local device
     * preferences, while respecting enterprise policies. This is only used on tablets, where
     * bookmarks bar does not sync with the user's desktop preference, but is instead stored locally
     * on device.
     *
     * <p>This method establishes a priority for which value to return:
     *
     * <ol>
     *   <li>A mandatory enterprise policy.
     *   <li>A recommended enterprise policy, if the user has not made an explicit choice.
     *   <li>The user's explicit local choice from SharedPreferences.
     *   <li>The system default (controlled by a feature flag).
     * </ol>
     *
     * <p>Note: When a user has not previously set the device preference, the default return value
     * is currently controlled by a FeatureParam for testing.
     *
     * @param profile The profile for which policies should be assessed.
     * @return Whether or not the bookmarks bar should be shown based on device preference.
     */
    public static boolean isDevicePrefShowBookmarksBarEnabled(@Nullable Profile profile) {
        // Highest priority: Mandatory policy (checks pref service).
        if (isBookmarkBarManagedByPolicy(profile)) {
            return isBookmarkBarEnabledByPolicy(profile);
        }

        // Returns true if the value is currently set to the recommended value AND and the user has
        // not yet set an overriding value in PrefService for this session. Note that in the
        // PrefService hierarchy, the user's overridden value takes priority over the recommended
        // value.
        if (isBookmarkBarValueFromRecommendation(profile)) {
            return isUserPrefsShowBookmarksBarEnabled(profile);
        }

        // Fallback: If no policies are active (or the user has overridden the recommendation), then
        // we respect the user's local choice.
        // If a user has set the show bookmarks bar setting explicitly, then we will use that value.
        // If the user has never set the preference, then we will return a default, which is
        // currently controlled with a FeatureParam.
        return hasUserSetDevicePrefShowBookmarksBar()
                ? ContextUtils.getAppSharedPreferences()
                        .getBoolean(BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR, false)
                : ChromeFeatureList.sAndroidBookmarkBarShowBookmarkBar.getValue();
    }

    /**
     * Set whether the bookmark bar should be shown at a device preferences level. This is only used
     * on tablets, where bookmarks bar does not sync with the user's desktop preference, but is
     * instead stored locally on the device.
     *
     * <p>This writes the value to two places: 1. Locally to SharedPreferences to preserve the
     * non-syncing behavior for tablets. 2. To the profile's PrefService to ensure the user's choice
     * correctly overrides any recommended policies.
     *
     * @param profile The profile for which the policy system should be updated.
     * @param enabled The new device preference for enabling the bookmark bar.
     * @param fromKeyboardShortcut True if the change was triggered by a keyboard shortcut.
     */
    public static void setDevicePrefShowBookmarksBar(
            Profile profile, boolean enabled, boolean fromKeyboardShortcut) {

        // Write to SharedPreferences to save the user's local choice.
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR, enabled)
                .apply();

        // Also write to PrefService to update the policy system.
        setUserPrefsShowBookmarksBar(profile, enabled, fromKeyboardShortcut);
    }

    /**
     * Returns true when the user has previously set the visibility of the bookmarks bar explicitly
     * at the device preference level. This is only used on tablets, where bookmarks bar does not
     * sync with the user's desktop preference, but is instead stored locally on the device.
     *
     * @return Whether the user has set show bookmarks bar device preference manually.
     */
    public static boolean hasUserSetDevicePrefShowBookmarksBar() {
        return ContextUtils.getAppSharedPreferences()
                .contains(BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR);
    }

    /**
     * Toggles the value of the show bookmarks bar device preference, this is stored locally and
     * only used on tablets, correctly interacting with enterprise policies.
     */
    public static void toggleDevicePrefShowBookmarksBar(
            Profile profile, boolean fromKeyboardShortcut) {
        setDevicePrefShowBookmarksBar(
                profile, !isDevicePrefShowBookmarksBarEnabled(profile), fromKeyboardShortcut);
    }

    // Histogram recording methods.

    public static void recordClick(@BookmarkBarClickType int clickType) {
        RecordHistogram.recordEnumeratedHistogram(
                BOOKMARK_BAR_CLICK, clickType, BookmarkBarClickType.NUM_ENTRIES);
    }

    public static void recordStartUpMetrics(Context context, @Nullable Profile profile) {
        boolean isCurrentlyVisible = isBookmarkBarVisible(context, profile);

        // Record if the Bookmark Bar is visible, but not in cases of a forced feature param.
        if (DeviceInfo.isDesktop() || hasUserSetDevicePrefShowBookmarksBar()) {
            RecordHistogram.recordBooleanHistogram(
                    BOOKMARK_BAR_SHOWN_ON_START_UP, isCurrentlyVisible);
        }

        // Record the reason why the Bookmark Bar is visible (hidden) in this instance.
        if (DeviceInfo.isDesktop()) {
            RecordHistogram.recordEnumeratedHistogram(
                    BOOKMARK_BAR_SHOWN_ON_START_UP_REASON,
                    isCurrentlyVisible
                            ? BookmarkBarShownReason.ENABLED_BY_USER_PREF
                            : BookmarkBarShownReason.DISABLED_BY_USER_PREF,
                    BookmarkBarShownReason.NUM_ENTRIES);
        } else {
            // On non-Desktop, we need to consider whether the device preference has been explicitly
            // chosen by the user, or if they have a default feature param value.
            if (hasUserSetDevicePrefShowBookmarksBar()) {
                RecordHistogram.recordEnumeratedHistogram(
                        BOOKMARK_BAR_SHOWN_ON_START_UP_REASON,
                        isCurrentlyVisible
                                ? BookmarkBarShownReason.ENABLED_BY_DEVICE_PREF
                                : BookmarkBarShownReason.DISABLED_BY_DEVICE_PREF,
                        BookmarkBarShownReason.NUM_ENTRIES);
            } else {
                RecordHistogram.recordEnumeratedHistogram(
                        BOOKMARK_BAR_SHOWN_ON_START_UP_REASON,
                        isCurrentlyVisible
                                ? BookmarkBarShownReason.ENABLED_BY_FEATURE_PARAM
                                : BookmarkBarShownReason.DISABLED_BY_FEATURE_PARAM,
                        BookmarkBarShownReason.NUM_ENTRIES);
            }
        }
    }

    // Helper methods.

    private static PrefService getPrefService(Profile profile) {
        return UserPrefs.get(profile.getOriginalProfile());
    }

    // ForTesting methods.

    /**
     * Sets whether the bookmark bar feature is forcibly allowed/disallowed for testing.
     *
     * @param allowed Whether the feature is forcibly allowed/disallowed.
     */
    public static void setActivityStateBookmarkBarCompatibleForTesting(@Nullable Boolean allowed) {
        sActivityStateBookmarkBarCompatibleForTesting = allowed;
        ResettersForTesting.register(() -> sActivityStateBookmarkBarCompatibleForTesting = null);
    }

    /**
     * Sets whether the bookmark bar feature is forcibly enabled/disabled for testing.
     *
     * @param enabled Whether the feature is forcibly enabled/disabled.
     */
    public static void setDeviceBookmarkBarCompatibleForTesting(@Nullable Boolean enabled) {
        sDeviceBookmarkBarCompatibleForTesting = enabled;
        ResettersForTesting.register(() -> sDeviceBookmarkBarCompatibleForTesting = null);
    }

    /**
     * Sets whether the bookmark bar feature is forcibly visible/invisible for testing.
     *
     * @param visible Whether the feature is forcibly visible/invisible.
     */
    public static void setBookmarkBarVisibleForTesting(@Nullable Boolean visible) {
        sBookmarkBarVisibleForTesting = visible;
        ResettersForTesting.register(() -> sBookmarkBarVisibleForTesting = null);
    }

    /**
     * Sets whether the bookmark bar user setting is forcibly enabled/disabled for testing.
     *
     * @param enabled Whether the user setting is forcibly enabled/disabled.
     */
    public static void setSettingEnabledForTesting(@Nullable Boolean enabled) {
        sSettingEnabledForTesting = enabled;
        ResettersForTesting.register(() -> sSettingEnabledForTesting = null);
    }

    /**
     * Sets the collection in which to cache bookmark bar user setting observers for testing.
     *
     * @param cache The collection in which to cache observers.
     */
    public static void setSettingObserverCacheForTesting(@Nullable Collection<PrefObserver> cache) {
        sSettingObserverCacheForTesting = cache;
        ResettersForTesting.register(() -> sSettingObserverCacheForTesting = null);
    }
}
