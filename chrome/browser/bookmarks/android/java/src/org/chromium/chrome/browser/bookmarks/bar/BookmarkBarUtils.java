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
import org.chromium.chrome.browser.flags.ChromeFeatureMap;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.bookmarks.BookmarkBarVisibilityState;
import org.chromium.components.embedder_support.util.UrlUtilities;
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

    // LINT.IfChange(BookmarkBarSettingChangeOrigin)
    /**
     * Enum that defines the possible origins from which the bookmark bar visibility setting can be
     * changed.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        BookmarkBarSettingChangeOrigin.KEYBOARD_SHORTCUT,
        BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS,
        BookmarkBarSettingChangeOrigin.BOOKMARK_BAR_CONTEXT_MENU,
        BookmarkBarSettingChangeOrigin.APP_MENU,
    })
    public @interface BookmarkBarSettingChangeOrigin {
        int KEYBOARD_SHORTCUT = 0;
        int APPEARANCE_SETTINGS = 1;
        int BOOKMARK_BAR_CONTEXT_MENU = 2;
        int APP_MENU = 3;
        int NUM_ENTRIES = 4;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/bookmarks/enums.xml:BookmarkBarSettingChangeOrigin)

    // LINT.IfChange(BookmarkBarVisibilityStateOnStartUpReason)
    /**
     * Enum that defines the possible reasons the bookmark bar may be shown or hidden when using the
     * tri-state visibility preference. These values are persisted to logs. Entries should not be
     * renumbered and numeric values should never be reused.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        BookmarkBarVisibilityStateOnStartUpReason.UNKNOWN,
        BookmarkBarVisibilityStateOnStartUpReason.ALWAYS_SHOW_BY_USER_PREF,
        BookmarkBarVisibilityStateOnStartUpReason.ALWAYS_HIDE_BY_USER_PREF,
        BookmarkBarVisibilityStateOnStartUpReason.ONLY_SHOW_ON_NTP_BY_USER_PREF,
        BookmarkBarVisibilityStateOnStartUpReason.ALWAYS_SHOW_BY_DEVICE_PREF,
        BookmarkBarVisibilityStateOnStartUpReason.ALWAYS_HIDE_BY_DEVICE_PREF,
        BookmarkBarVisibilityStateOnStartUpReason.ONLY_SHOW_ON_NTP_BY_DEVICE_PREF,
        BookmarkBarVisibilityStateOnStartUpReason.DEFAULT_DEVICE_VALUE,
    })
    public @interface BookmarkBarVisibilityStateOnStartUpReason {
        int UNKNOWN = 0;
        int ALWAYS_SHOW_BY_USER_PREF = 1;
        int ALWAYS_HIDE_BY_USER_PREF = 2;
        int ONLY_SHOW_ON_NTP_BY_USER_PREF = 3;
        int ALWAYS_SHOW_BY_DEVICE_PREF = 4;
        int ALWAYS_HIDE_BY_DEVICE_PREF = 5;
        int ONLY_SHOW_ON_NTP_BY_DEVICE_PREF = 6;
        int DEFAULT_DEVICE_VALUE = 7;
        int NUM_ENTRIES = 8;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/bookmarks/enums.xml:BookmarkBarVisibilityStateOnStartUpReason)

    // [v1] Histogram names:
    public static final String TOGGLED_IN_SETTINGS = "Bookmarks.BookmarkBar.ToggledInSettings";
    public static final String TOGGLED_BY_KEYBOARD_SHORTCUT =
            "Bookmarks.BookmarkBar.ToggledByKeyboardShortcut";
    public static final String BOOKMARK_BAR_SHOWN_ON_START_UP =
            "Bookmarks.BookmarkBar.Android.ShownOnStartUp";
    public static final String BOOKMARK_BAR_SHOWN_ON_START_UP_REASON =
            "Bookmarks.BookmarkBar.Android.ShownOnStartUpReason";

    // [v2] Histogram names:
    public static final String TOGGLED_KEYBOARD = "Bookmarks.BookmarkBar.TriState.ToggledKeyboard";
    public static final String TOGGLED_APPEARANCE_SETTINGS =
            "Bookmarks.BookmarkBar.TriState.ToggledAppearanceSettings";
    public static final String TOGGLED_CONTEXT_MENU =
            "Bookmarks.BookmarkBar.TriState.ToggledContextMenu";
    public static final String TOGGLED_APP_MENU = "Bookmarks.BookmarkBar.TriState.ToggledAppMenu";
    public static final String VISIBILITY_STATE_CHANGE_ORIGIN =
            "Bookmarks.BookmarkBar.TriState.VisibilityStateChangeOrigin";
    public static final String VISIBILITY_STATE_ON_START_UP =
            "Bookmarks.BookmarkBar.TriState.VisibilityStateOnStartUp";
    public static final String VISIBILITY_STATE_ON_START_UP_REASON =
            "Bookmarks.BookmarkBar.TriState.VisibilityStateOnStartUpReason";

    // Common histogram names:
    public static final String BOOKMARK_BAR_CLICK = "Bookmarks.BookmarkBar.Click";

    /**
     * Whether the bookmark bar feature is considered compatible with the current activity state
     * (e.g. window width) for testing.
     */
    private static @Nullable Boolean sActivityStateBookmarkBarCompatibleForTesting;

    /**
     * Whether the bookmark bar feature is considered compatible with the selected device form
     * factor for testing.
     */
    private static @Nullable Boolean sDeviceBookmarkBarCompatibleForTesting;

    /** Whether the bookmark bar feature is forcibly visible/invisible for testing. */
    private static @Nullable Boolean sBookmarkBarVisibleForTesting;

    /** Whether the bookmark bar user setting is forcibly enabled/disabled for testing. */
    private static @Nullable Boolean sSettingEnabledForTesting;

    /** Collection in which to cache bookmark bar user setting observers for testing. */
    private static @Nullable Collection<PrefObserver> sSettingObserverCacheForTesting;

    private BookmarkBarUtils() {}

    // ---------------------------------------------------------------------------------------------
    // Shared logic for Bookmark Bar compatibility.
    // ---------------------------------------------------------------------------------------------

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
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
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

    // ---------------------------------------------------------------------------------------------
    // Shared logic for Bookmark Bar current visibility state and visibility control.
    // ---------------------------------------------------------------------------------------------

    /**
     * Returns true if the Bookmark Bar currently visible. The feature is visible when it is allowed
     * in the given context, and the show bookmark bar UserPref is enabled for the current user.
     * When on tablets, we do not use the UserPref and instead use the device preference.
     *
     * @param context The context in which compatibility should be assessed.
     * @param profile The profile for which the user UserPref should be assessed.
     * @param isXrFullSpaceMode Supplier for whether the device is in XR full space mode.
     * @return Whether the Bookmark Bar is currently visible.
     */
    public static boolean isBookmarkBarVisible(
            Context context, @Nullable Profile profile, boolean isXrFullSpaceMode) {
        if (sBookmarkBarVisibleForTesting != null) {
            return sBookmarkBarVisibleForTesting;
        }

        if (isXrFullSpaceMode || !isActivityStateBookmarkBarCompatible(context)) {
            return false;
        }

        // On Desktop, we sync with the UserPrefs.
        // On tablets we use the device preference logic (policy (pref service)  > local pref
        // (shared pref)).
        return shouldUseProfileUserPrefs()
                ? isUserPrefsShowBookmarksBarEnabled(profile)
                : isDevicePrefShowBookmarksBarEnabled(profile);
    }

    /**
     * Toggles the visibility of the bookmarks bar, automatically choosing between UserPrefs
     * (Desktop) and Device preferences (Tablet) based on the device type.
     *
     * @param profile The profile for which the bookmarks bar visibility should be toggled.
     * @param fromKeyboardShortcut True if the change was triggered by a keyboard shortcut.
     */
    public static void toggleShowBookmarksBar(Profile profile, boolean fromKeyboardShortcut) {
        if (shouldUseProfileUserPrefs()) {
            toggleUserPrefsShowBookmarksBar(profile, fromKeyboardShortcut);
        } else {
            toggleDevicePrefShowBookmarksBar(profile, fromKeyboardShortcut);
        }
    }

    // [v2] (Tri-state) Using the Pref.BOOKMARK_BAR_VISIBILITY_STATE preference or
    // BookmarkBarConstants.BOOKMARK_BAR_BOOKMARK_BAR_VISIBILITY_STATE.

    /**
     * Returns the current visibility state of the Bookmark Bar. The feature is visible when it is
     * allowed in the given context, and the bookmark bar visibility state UserPref is set to a
     * value that allows it to be enabled in the current context (determined by the caller). When on
     * tablets, we do not use the UserPref and instead use the device preference.
     *
     * @param context The context in which compatibility should be assessed.
     * @param profile The profile for which the user UserPref should be assessed.
     * @param isXrFullSpaceMode Supplier for whether the device is in XR full space mode.
     * @return Whether the Bookmark Bar is currently visible.
     */
    public static @BookmarkBarVisibilityState int getBookmarkBarVisibilityState(
            Context context, @Nullable Profile profile, boolean isXrFullSpaceMode) {
        // This should only be called if the tri-state feature flag is enabled.
        assert ChromeFeatureMap.isEnabled(ChromeFeatureList.BOOKMARKS_BAR_NTP)
                : "Tri-state visibility preference should not be used without feature flag.";

        if (sBookmarkBarVisibleForTesting != null) {
            return sBookmarkBarVisibleForTesting
                    ? BookmarkBarVisibilityState.ALWAYS_SHOW
                    : BookmarkBarVisibilityState.ALWAYS_HIDE;
        }

        // The bookmark bar is never visible in XR, so return a force hide value here.
        if (isXrFullSpaceMode || !isActivityStateBookmarkBarCompatible(context)) {
            return BookmarkBarVisibilityState.ALWAYS_HIDE;
        }

        // On Desktop, we sync with the UserPrefs.
        // On tablets we use the device preference logic (policy (pref service)  > local pref
        // (shared pref)).
        return shouldUseProfileUserPrefs()
                ? getUserPrefsBookmarkBarVisibilityState(profile)
                : getDevicePrefBookmarkBarVisibilityState(profile);
    }

    /**
     * Sets the visibility state of the bookmarks bar, automatically choosing between UserPrefs
     * (Desktop) and Device preferences (Tablet) based on the device type.
     *
     * @param profile The profile for which the bookmarks bar visibility should be toggled.
     * @param state The new visibility state for the bookmark bar.
     * @param origin The origin from which the setting change was triggered.
     */
    public static void setBookmarkBarVisibilityState(
            Profile profile,
            @BookmarkBarVisibilityState int state,
            @BookmarkBarSettingChangeOrigin int origin) {
        // This should only be called if the tri-state feature flag is enabled.
        assert ChromeFeatureMap.isEnabled(ChromeFeatureList.BOOKMARKS_BAR_NTP)
                : "Tri-state visibility preference should not be used without feature flag.";

        if (shouldUseProfileUserPrefs()) {
            setUserPrefsBookmarkBarVisibilityState(profile, state, origin);
        } else {
            setDevicePrefBookmarkBarVisibilityState(state, origin);
        }
    }

    /**
     * Returns true if the Bookmark Bar should be visible based on the visibility state and the
     * current state of the Tab. The feature is visible when it is allowed in the given context, and
     * the bookmark bar visibility state UserPref or DevicePref is set to a value that allows it to
     * be enabled in the current context. When set to ONLY_SHOW_ON_NTP, the visibility is evaluated
     * against the active tab.
     *
     * @param context The context in which compatibility should be assessed.
     * @param profile The profile for which the user UserPref should be assessed.
     * @param isXrFullSpaceMode Supplier for whether the device is in XR full space mode.
     * @param activeTab The currently active tab, if any.
     * @return Whether the Bookmark Bar is currently visible.
     */
    public static boolean isBookmarkBarVisibleForState(
            Context context,
            @Nullable Profile profile,
            boolean isXrFullSpaceMode,
            @Nullable Tab activeTab) {
        if (sBookmarkBarVisibleForTesting != null) {
            return sBookmarkBarVisibleForTesting;
        }

        if (isXrFullSpaceMode || !isActivityStateBookmarkBarCompatible(context)) {
            return false;
        }

        // On Desktop, we sync with the UserPrefs.
        // On tablets we use the device preference logic (policy (pref service) > local pref
        // (shared pref)).
        @BookmarkBarVisibilityState
        int visibilityState =
                shouldUseProfileUserPrefs()
                        ? getUserPrefsBookmarkBarVisibilityState(profile)
                        : getDevicePrefBookmarkBarVisibilityState(profile);

        if (visibilityState == BookmarkBarVisibilityState.ALWAYS_SHOW) {
            return true;
        } else if (visibilityState == BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP) {
            return activeTab != null && UrlUtilities.isNtpUrl(activeTab.getUrl());
        }
        return false;
    }

    // ---------------------------------------------------------------------------------------------
    // Enterprise policy related methods for Bookmark Bar.
    // ---------------------------------------------------------------------------------------------

    // [v1] (Boolean) Using the Pref.SHOW_BOOKMARK_BAR preference.

    /**
     * Returns whether Pref.SHOW_BOOKMARK_BAR is controlled by an enterprise policy.
     *
     * @param profile The profile for which the policy should be assessed.
     * @return Whether Pref.SHOW_BOOKMARK_BAR is managed by the policy.
     */
    public static boolean isUserPrefsShowBookmarkBarManagedByPolicy(@Nullable Profile profile) {
        return profile != null
                ? getPrefService(profile).isManagedPreference(Pref.SHOW_BOOKMARK_BAR)
                : false;
    }

    /**
     * Returns whether Pref.SHOW_BOOKMARK_BAR has a recommended value from a policy.
     *
     * @param profile The profile for which the policy should be assessed.
     * @return Whether a recommended value exists for Pref.SHOW_BOOKMARK_BAR.
     */
    public static boolean isUserPrefsShowBookmarkBarRecommended(@Nullable Profile profile) {
        return profile != null
                ? getPrefService(profile).hasRecommendation(Pref.SHOW_BOOKMARK_BAR)
                : false;
    }

    /**
     * Returns the recommended value of the policy for Pref.SHOW_BOOKMARK_BAR if one exists. This
     * should only be called when the preference has a recommended value set by a policy.
     *
     * <p>Note: The recommended value of a policy is not accessible via a direct API call, so we
     * deduce the value by comparing the UserPref value to whether or not the UserPref value is
     * following the recommendation. If these values are equal, the recommended policy value is
     * |true|.
     *
     * @param profile The profile for which the policy should be assessed.
     * @return The recommended value of the policy for Pref.SHOW_BOOKMARK_BAR.
     */
    public static boolean getUserPrefsShowBookmarkBarRecommendedValue(@Nullable Profile profile) {
        assert isUserPrefsShowBookmarkBarRecommended(profile)
                : "Pref.SHOW_BOOKMARK_BAR has no policy configured with a recommended value";
        return isUserPrefsShowBookmarksBarEnabled(profile)
                == isUserPrefsShowBookmarkBarFollowingRecommendation(profile);
    }

    /**
     * Returns whether the user's current setting matches the recommended policy value. Should only
     * be called when isUserPrefsShowBookmarkBarRecommended is true.
     *
     * @param profile The profile for which the policy should be assessed.
     * @return Whether the user's setting matches the recommended value.
     */
    public static boolean isUserPrefsShowBookmarkBarFollowingRecommendation(
            @Nullable Profile profile) {
        assert isUserPrefsShowBookmarkBarRecommended(profile);
        return profile != null
                ? getPrefService(profile).isFollowingRecommendation(Pref.SHOW_BOOKMARK_BAR)
                : false;
    }

    // [v2] (Tri-state) Using the Pref.BOOKMARK_BAR_VISIBILITY_STATE preference.

    /**
     * Returns whether Pref.BOOKMARK_BAR_VISIBILITY_STATE is controlled by an enterprise policy.
     *
     * @param profile The profile for which the policy should be assessed.
     * @return Whether Pref.BOOKMARK_BAR_VISIBILITY_STATE is managed by the policy.
     */
    public static boolean isUserPrefsBookmarkBarVisibilityStateManagedByPolicy(
            @Nullable Profile profile) {
        return profile != null
                ? getPrefService(profile).isManagedPreference(Pref.BOOKMARK_BAR_VISIBILITY_STATE)
                : false;
    }

    /**
     * Returns whether Pref.BOOKMARK_BAR_VISIBILITY_STATE has a recommended value from a policy.
     *
     * @param profile The profile for which the policy should be assessed.
     * @return Whether a recommended value exists for Pref.BOOKMARK_BAR_VISIBILITY_STATE.
     */
    public static boolean isUserPrefsBookmarkBarVisibilityStateRecommended(
            @Nullable Profile profile) {
        return profile != null
                ? getPrefService(profile).hasRecommendation(Pref.BOOKMARK_BAR_VISIBILITY_STATE)
                : false;
    }

    /**
     * Returns the recommended value of the policy for Pref.BOOKMARK_BAR_VISIBILITY_STATE if one
     * exists. This should only be called when the preference has a recommended value set by a
     * policy.
     *
     * <p>Note: The recommended value of a policy is not accessible via a direct API call, so we
     * deduce the value by comparing the UserPref value to whether or not the UserPref value is
     * following the recommendation. However, this Pref has 3 states but only 2 can be recommended
     * by the policy. If the profile's UserPref option is set to the |ONLY_SHOW_ON_NTP| option, we
     * cannot deduce the policy's recommended value, so we return ALWAYS_HIDE for now.
     *
     * @param profile The profile for which the policy should be assessed.
     * @return The recommended value of the policy for Pref.BOOKMARK_BAR_VISIBILITY_STATE.
     */
    public static @BookmarkBarVisibilityState int
            getUserPrefsBookmarkBarVisibilityStateRecommendedValue(@Nullable Profile profile) {
        assert isUserPrefsBookmarkBarVisibilityStateRecommended(profile)
                : "Pref.BOOKMARK_BAR_VISIBILITY_STATE has no policy configured with a recommended"
                        + " value";
        boolean isFollowing = isUserPrefsBookmarkBarVisibilityStateFollowingRecommendation(profile);
        @BookmarkBarVisibilityState
        int currentValue = getUserPrefsBookmarkBarVisibilityState(profile);

        // If the user is following the recommendation, their current value IS the recommended
        // value.
        if (isFollowing) {
            return currentValue;
        }

        // Since the user is not following the recommendation, the recommended value is the opposite
        // of whatever they currently have active (since policy only recommends SHOW or HIDE).
        switch (currentValue) {
            case BookmarkBarVisibilityState.ALWAYS_HIDE:
                return BookmarkBarVisibilityState.ALWAYS_SHOW;

            case BookmarkBarVisibilityState.ALWAYS_SHOW:
                return BookmarkBarVisibilityState.ALWAYS_HIDE;

            case BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP:
            default:
                // We can't deduce the policy value here. By definition the user is not following
                // the recommended value since ONLY_SHOW_ON_NTP is not an option for policy
                // recommendation, but we can't tell in what way they are not following the
                // recommendation and will choose to return ALWAYS_HIDE as a default guess.
                // TODO(crbug.com/544112043): Find alt way to deduce value or add a new Prefs API.
                return BookmarkBarVisibilityState.ALWAYS_HIDE;
        }
    }

    /**
     * Returns whether the user's current setting matches the recommended policy value. Should only
     * be called when isUserPrefsBookmarkBarVisibilityStateRecommended is true.
     *
     * @param profile The profile for which the policy should be assessed.
     * @return Whether the user's setting matches the recommended value.
     */
    public static boolean isUserPrefsBookmarkBarVisibilityStateFollowingRecommendation(
            @Nullable Profile profile) {
        assert isUserPrefsBookmarkBarVisibilityStateRecommended(profile);
        return profile != null
                ? getPrefService(profile)
                        .isFollowingRecommendation(Pref.BOOKMARK_BAR_VISIBILITY_STATE)
                : false;
    }

    // ---------------------------------------------------------------------------------------------
    // UserPrefs methods - used on Desktop.
    // ---------------------------------------------------------------------------------------------

    // [v1] (Boolean) Using the Pref.SHOW_BOOKMARK_BAR preference.

    /**
     * Returns whether the bookmark bar should be shown based on the current user's UserPrefs. Note:
     * This is synced across devices for the user's profile via Pref.SHOW_BOOKMARK_BAR.
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
     * Sets the value of the UserPref Pref.SHOW_BOOKMARK_BAR for the current user.
     *
     * @param profile The profile for which the user setting should be set.
     * @param enabled Whether the user setting should be set to enabled/disabled.
     * @param fromKeyboardShortcut True if the change was triggered by a keyboard shortcut.
     */
    public static void setUserPrefsShowBookmarksBar(
            Profile profile, boolean enabled, boolean fromKeyboardShortcut) {
        RecordHistogram.recordBooleanHistogram(
                fromKeyboardShortcut ? TOGGLED_BY_KEYBOARD_SHORTCUT : TOGGLED_IN_SETTINGS, enabled);
        getPrefService(profile).setBoolean(Pref.SHOW_BOOKMARK_BAR, enabled);
    }

    /**
     * Toggles the value of the UserPref Pref.SHOW_BOOKMARK_BAR for the current user.
     *
     * @param profile The profile for which the UserPref should be toggled.
     * @param fromKeyboardShortcut True if the change was triggered by a keyboard shortcut.
     */
    private static void toggleUserPrefsShowBookmarksBar(
            Profile profile, boolean fromKeyboardShortcut) {
        setUserPrefsShowBookmarksBar(
                profile,
                !getPrefService(profile).getBoolean(Pref.SHOW_BOOKMARK_BAR),
                fromKeyboardShortcut);
    }

    // [v2] (Tri-state) Using the Pref.BOOKMARK_BAR_VISIBILITY_STATE preference.

    /**
     * Returns the visibility state of the bookmark bar based on the current user's UserPrefs. Note:
     * This is synced across devices for the user's profile via Pref.BOOKMARK_BAR_VISIBILITY_STATE.
     *
     * @param profile The profile for which the UserPref should be assessed.
     * @return The user's current preference for the bookmark bar visibility state.
     */
    public static @BookmarkBarVisibilityState int getUserPrefsBookmarkBarVisibilityState(
            @Nullable Profile profile) {
        return profile != null
                ? getPrefService(profile).getInteger(Pref.BOOKMARK_BAR_VISIBILITY_STATE)
                : BookmarkBarVisibilityState.ALWAYS_HIDE;
    }

    /**
     * Sets the value of the UserPref Pref.BOOKMARK_BAR_VISIBILITY_STATE for the current user.
     *
     * @param profile The profile for which the user setting should be set.
     * @param state The new state for the visibility state of the bookmarks bar.
     * @param origin The origin from which the setting change was triggered.
     */
    public static void setUserPrefsBookmarkBarVisibilityState(
            Profile profile,
            @BookmarkBarVisibilityState int state,
            @BookmarkBarSettingChangeOrigin int origin) {
        recordBookmarkBarVisibilityStateToggled(state, origin);
        getPrefService(profile).setInteger(Pref.BOOKMARK_BAR_VISIBILITY_STATE, state);
    }

    // ---------------------------------------------------------------------------------------------
    // Device preferences methods - used on tablets.
    // ---------------------------------------------------------------------------------------------

    // [v1] (Boolean) Using BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR key.

    /**
     * Returns whether or not the bookmark bar should be shown based on the local device
     * preferences, while respecting enterprise policies. This is only used on tablets, where
     * bookmarks bar does not sync with the user's Desktop preference, but is instead stored locally
     * on device with the key: BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR.
     *
     * <p>This method establishes a priority for which value to return:
     *
     * <ol>
     *   <li>A mandatory enterprise policy.
     *   <li>The user's explicit local choice from SharedPreferences.
     *   <li>The recommended enterprise policy.
     *   <li>The system default (false, i.e. bookmark bar hidden).
     * </ol>
     *
     * <p>Note: When a user has not previously set the device preference, the default return value
     * is false (i.e. bookmark bar hidden).
     *
     * @param profile The profile for which policies should be assessed.
     * @return Whether or not the bookmarks bar should be shown based on device preference.
     */
    public static boolean isDevicePrefShowBookmarksBarEnabled(@Nullable Profile profile) {
        // 1. Mandatory policy (must be obeyed).
        if (isUserPrefsShowBookmarkBarManagedByPolicy(profile)) {
            return isUserPrefsShowBookmarksBarEnabled(profile);
        }

        // 2. Local explicit override (takes precedence over recommendations and defaults).
        if (hasUserSetDevicePrefShowBookmarksBar()) {
            return ContextUtils.getAppSharedPreferences()
                    .getBoolean(BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR, false);
        }

        // 3. Recommended policy value (if it exists).
        if (isUserPrefsShowBookmarkBarRecommended(profile)) {
            return getUserPrefsShowBookmarkBarRecommendedValue(profile);
        }

        // 4. Default fallback for when there is no policy or explicit user choice.
        return false;
    }

    /**
     * Set whether the bookmark bar should be shown at a device preferences level. This is only used
     * on tablets, where bookmarks bar does not sync with the user's Desktop preference, but is
     * instead stored locally on the device with the key:
     * BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR
     *
     * <p>This writes the value locally to SharedPreferences to preserve the non-syncing behavior
     * for tablets. Local overrides do not need to be propagated to the profile's PrefService.
     *
     * @param enabled The new device preference for enabling the bookmark bar.
     * @param fromKeyboardShortcut True if the change was triggered by a keyboard shortcut.
     */
    public static void setDevicePrefShowBookmarksBar(
            boolean enabled, boolean fromKeyboardShortcut) {
        RecordHistogram.recordBooleanHistogram(
                fromKeyboardShortcut ? TOGGLED_BY_KEYBOARD_SHORTCUT : TOGGLED_IN_SETTINGS, enabled);

        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR, enabled)
                .apply();
    }

    /**
     * Returns true when the user has previously set the visibility of the bookmarks bar explicitly
     * at the device preference level. This is only used on tablets, where bookmarks bar does not
     * sync with the user's Desktop preference, but is instead stored locally on the device with the
     * key: BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR.
     *
     * @return Whether the user has set the BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR
     *     device preference manually.
     */
    public static boolean hasUserSetDevicePrefShowBookmarksBar() {
        return ContextUtils.getAppSharedPreferences()
                .contains(BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR);
    }

    /**
     * Toggles the value of the BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR device
     * preference, this is stored locally and only used on tablets, correctly interacting with
     * enterprise policies.
     *
     * @param profile The profile for which policies should be assessed.
     * @param fromKeyboardShortcut True if the change was triggered by a keyboard shortcut.
     */
    private static void toggleDevicePrefShowBookmarksBar(
            Profile profile, boolean fromKeyboardShortcut) {
        setDevicePrefShowBookmarksBar(
                !isDevicePrefShowBookmarksBarEnabled(profile), fromKeyboardShortcut);
    }

    // [v2] (Tri-state) Using BookmarkBarConstants.BOOKMARK_BAR_BOOKMARK_BAR_VISIBILITY_STATE key.

    /**
     * Returns the visibility state of the bookmark bar based on the local device preferences, while
     * respecting enterprise policies. This is only used on tablets, where bookmarks bar does not
     * sync with the user's Desktop preference, but is instead stored locally on device with the
     * key: BookmarkBarConstants.BOOKMARK_BAR_BOOKMARK_BAR_VISIBILITY_STATE.
     *
     * <p>This method establishes a priority for which value to return:
     *
     * <ol>
     *   <li>A mandatory enterprise policy.
     *   <li>The user's explicit local choice from SharedPreferences.
     *   <li>The recommended enterprise policy.
     *   <li>The system default (BookmarkBarVisibilityState.ALWAYS_HIDE).
     * </ol>
     *
     * <p>Note: When a user has not previously set the device preference, the default return value
     * is BookmarkBarVisibilityState.ALWAYS_HIDE.
     *
     * @param profile The profile for which policies should be assessed.
     * @return The visibility state of the bookmarks bar based on device preference.
     */
    public static @BookmarkBarVisibilityState int getDevicePrefBookmarkBarVisibilityState(
            @Nullable Profile profile) {
        if (isUserPrefsBookmarkBarVisibilityStateManagedByPolicy(profile)) {
            return getUserPrefsBookmarkBarVisibilityState(profile);
        }

        if (hasUserSetDevicePrefBookmarkBarVisibilityState()) {
            return ContextUtils.getAppSharedPreferences()
                    .getInt(
                            BookmarkBarConstants.BOOKMARK_BAR_BOOKMARK_BAR_VISIBILITY_STATE,
                            BookmarkBarVisibilityState.ALWAYS_HIDE);
        }

        if (isUserPrefsBookmarkBarVisibilityStateRecommended(profile)) {
            return getUserPrefsBookmarkBarVisibilityStateRecommendedValue(profile);
        }

        return BookmarkBarVisibilityState.ALWAYS_HIDE;
    }

    /**
     * Set whether the bookmark bar should be shown at a device preferences level. This is only used
     * on tablets, where bookmarks bar does not sync with the user's Desktop preference, but is
     * instead stored locally on the device with the key:
     * BookmarkBarConstants.BOOKMARK_BAR_BOOKMARK_BAR_VISIBILITY_STATE.
     *
     * <p>This writes the value locally to SharedPreferences to preserve the non-syncing behavior
     * for tablets. Local overrides do not need to be propagated to the profile's PrefService.
     *
     * @param state The new device preference for the visibility state of the bookmark bar.
     * @param origin The origin from which the setting change was triggered.
     */
    public static void setDevicePrefBookmarkBarVisibilityState(
            @BookmarkBarVisibilityState int state, @BookmarkBarSettingChangeOrigin int origin) {
        recordBookmarkBarVisibilityStateToggled(state, origin);
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putInt(BookmarkBarConstants.BOOKMARK_BAR_BOOKMARK_BAR_VISIBILITY_STATE, state)
                .apply();
    }

    /**
     * Returns true when the user has previously set the visibility of the bookmarks bar explicitly
     * at the device preference level. This is only used on tablets, where bookmarks bar does not
     * sync with the user's Desktop preference, but is instead stored locally on the device with the
     * key: BookmarkBarConstants.BOOKMARK_BAR_BOOKMARK_BAR_VISIBILITY_STATE.
     *
     * @return Whether the user has set the
     *     BookmarkBarConstants.BOOKMARK_BAR_BOOKMARK_BAR_VISIBILITY_STATE device preference
     *     manually.
     */
    public static boolean hasUserSetDevicePrefBookmarkBarVisibilityState() {
        return ContextUtils.getAppSharedPreferences()
                .contains(BookmarkBarConstants.BOOKMARK_BAR_BOOKMARK_BAR_VISIBILITY_STATE);
    }

    // ---------------------------------------------------------------------------------------------
    // Metrics recording, helper methods, testing methods, etc.
    // ---------------------------------------------------------------------------------------------

    // Histogram recording methods.

    private static void recordBookmarkBarVisibilityStateToggled(
            @BookmarkBarVisibilityState int state, @BookmarkBarSettingChangeOrigin int origin) {
        RecordHistogram.recordEnumeratedHistogram(
                VISIBILITY_STATE_CHANGE_ORIGIN, origin, BookmarkBarSettingChangeOrigin.NUM_ENTRIES);
        switch (origin) {
            case BookmarkBarSettingChangeOrigin.KEYBOARD_SHORTCUT:
                RecordHistogram.recordEnumeratedHistogram(
                        TOGGLED_KEYBOARD, state, BookmarkBarVisibilityState.MAX_VALUE + 1);
                break;
            case BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS:
                RecordHistogram.recordEnumeratedHistogram(
                        TOGGLED_APPEARANCE_SETTINGS,
                        state,
                        BookmarkBarVisibilityState.MAX_VALUE + 1);
                break;
            case BookmarkBarSettingChangeOrigin.BOOKMARK_BAR_CONTEXT_MENU:
                RecordHistogram.recordEnumeratedHistogram(
                        TOGGLED_CONTEXT_MENU, state, BookmarkBarVisibilityState.MAX_VALUE + 1);
                break;
            case BookmarkBarSettingChangeOrigin.APP_MENU:
                RecordHistogram.recordEnumeratedHistogram(
                        TOGGLED_APP_MENU, state, BookmarkBarVisibilityState.MAX_VALUE + 1);
                break;
        }
    }

    public static void recordClick(@BookmarkBarClickType int clickType) {
        RecordHistogram.recordEnumeratedHistogram(
                BOOKMARK_BAR_CLICK, clickType, BookmarkBarClickType.NUM_ENTRIES);
    }

    public static void recordStartUpMetrics(
            Context context, @Nullable Profile profile, boolean isXrFullSpaceMode) {
        boolean isCurrentlyVisible = isBookmarkBarVisible(context, profile, isXrFullSpaceMode);

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

    public static void recordStartUpMetricsForVisibilityState(@Nullable Profile profile) {
        @BookmarkBarVisibilityState
        int settingState =
                shouldUseProfileUserPrefs()
                        ? getUserPrefsBookmarkBarVisibilityState(profile)
                        : getDevicePrefBookmarkBarVisibilityState(profile);

        // Record if the Bookmark Bar is visible, but not in cases of an unselected default state.
        if (shouldUseProfileUserPrefs() || hasUserSetDevicePrefBookmarkBarVisibilityState()) {
            RecordHistogram.recordEnumeratedHistogram(
                    VISIBILITY_STATE_ON_START_UP,
                    settingState,
                    BookmarkBarVisibilityState.MAX_VALUE + 1);
        }

        // Record the reason why the Bookmark Bar is visible (hidden) in this instance.
        @BookmarkBarVisibilityStateOnStartUpReason int reason;
        if (shouldUseProfileUserPrefs()) {
            reason =
                    switch (settingState) {
                        case BookmarkBarVisibilityState.ALWAYS_SHOW ->
                                BookmarkBarVisibilityStateOnStartUpReason.ALWAYS_SHOW_BY_USER_PREF;
                        case BookmarkBarVisibilityState.ALWAYS_HIDE ->
                                BookmarkBarVisibilityStateOnStartUpReason.ALWAYS_HIDE_BY_USER_PREF;
                        case BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP ->
                                BookmarkBarVisibilityStateOnStartUpReason
                                        .ONLY_SHOW_ON_NTP_BY_USER_PREF;
                        default -> BookmarkBarVisibilityStateOnStartUpReason.UNKNOWN;
                    };
        } else {
            if (hasUserSetDevicePrefBookmarkBarVisibilityState()) {
                reason =
                        switch (settingState) {
                            case BookmarkBarVisibilityState.ALWAYS_SHOW ->
                                    BookmarkBarVisibilityStateOnStartUpReason
                                            .ALWAYS_SHOW_BY_DEVICE_PREF;
                            case BookmarkBarVisibilityState.ALWAYS_HIDE ->
                                    BookmarkBarVisibilityStateOnStartUpReason
                                            .ALWAYS_HIDE_BY_DEVICE_PREF;
                            case BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP ->
                                    BookmarkBarVisibilityStateOnStartUpReason
                                            .ONLY_SHOW_ON_NTP_BY_DEVICE_PREF;
                            default -> BookmarkBarVisibilityStateOnStartUpReason.UNKNOWN;
                        };
            } else {
                reason = BookmarkBarVisibilityStateOnStartUpReason.DEFAULT_DEVICE_VALUE;
            }
        }

        RecordHistogram.recordEnumeratedHistogram(
                VISIBILITY_STATE_ON_START_UP_REASON,
                reason,
                BookmarkBarVisibilityStateOnStartUpReason.NUM_ENTRIES);
    }

    // Helper methods.

    /**
     * Returns whether the bookmark bar should use profile user preferences (synced across devices,
     * e.g. Desktop) rather than local device preferences (e.g. tablet). This method should not be
     * used in lieu of 'DeviceInfo.isDesktop().' Rather, it should only be used to determine syncing
     * behavior for the bookmark bar visibility settings.
     *
     * @return True when the system should be using profile prefs.
     */
    public static boolean shouldUseProfileUserPrefs() {
        return DeviceInfo.isDesktop();
    }

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
