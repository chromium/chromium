// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.base.test.util.HistogramWatcher.newBuilder;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.components.bookmarks.BookmarkBarVisibilityState;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link BookmarkBarUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkBarUtilsTest {

    @Rule
    public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule
    public OverrideContextWrapperTestRule mOverrideContextRule =
            new OverrideContextWrapperTestRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PrefService mPrefService;
    @Mock private Profile mProfile;
    @Mock private UserPrefsJni mUserPrefsJni;

    @Before
    public void setUp() {
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);

        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);
    }

    @After
    public void tearDown() {
        UserPrefsJni.setInstanceForTesting(null);
        mOverrideContextRule.setIsDesktop(false);
    }

    private void setBooleanPref(
            boolean isManaged,
            boolean managedPolicyValue,
            boolean hasRecommendation,
            boolean isFollowing,
            boolean currentUserPref) {
        when(mPrefService.getBoolean(Pref.SHOW_BOOKMARK_BAR))
                .thenReturn(isManaged ? managedPolicyValue : currentUserPref);
        when(mPrefService.isManagedPreference(Pref.SHOW_BOOKMARK_BAR)).thenReturn(isManaged);
        when(mPrefService.hasRecommendation(Pref.SHOW_BOOKMARK_BAR)).thenReturn(hasRecommendation);
        when(mPrefService.isFollowingRecommendation(Pref.SHOW_BOOKMARK_BAR))
                .thenReturn(isFollowing);
    }

    private void setIntegerPref(
            boolean isManaged,
            int managedPolicyValue,
            boolean hasRecommendation,
            boolean isFollowing,
            int currentUserPref) {
        when(mPrefService.getInteger(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(isManaged ? managedPolicyValue : currentUserPref);
        when(mPrefService.isManagedPreference(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(isManaged);
        when(mPrefService.hasRecommendation(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(hasRecommendation);
        when(mPrefService.isFollowingRecommendation(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(isFollowing);
    }

    // ---------------------------------------------------------------------------------------------
    // Group 1: V1 (Boolean) - UserPrefs
    // ---------------------------------------------------------------------------------------------

    @Test
    @SmallTest
    public void testShowBookmarkBar_UserPrefs_Set() {
        mOverrideContextRule.setIsDesktop(true);
        BookmarkBarUtils.setUserPrefsShowBookmarksBar(mProfile, true, false);
        verify(mPrefService).setBoolean(Pref.SHOW_BOOKMARK_BAR, true);
    }

    @Test
    @SmallTest
    public void testShowBookmarkBar_UserPrefs_isEnabled_Default() {
        mOverrideContextRule.setIsDesktop(true);
        setBooleanPref(
                /* isManaged= */ false,
                /* managedPolicyValue= */ false,
                /* hasRecommendation= */ false,
                /* isFollowing= */ false,
                /* currentUserPref= */ false);
        assertFalse(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));
    }

    @Test
    @SmallTest
    public void testShowBookmarkBar_UserPrefs_isEnabled_WithUserChoice() {
        mOverrideContextRule.setIsDesktop(true);
        setBooleanPref(
                /* isManaged= */ false,
                /* managedPolicyValue= */ false,
                /* hasRecommendation= */ false,
                /* isFollowing= */ false,
                /* currentUserPref= */ true);
        assertTrue(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));
    }

    @Test
    @SmallTest
    public void testShowBookmarkBar_UserPrefs_isEnabled_Policy_Mandatory() {
        mOverrideContextRule.setIsDesktop(true);
        setBooleanPref(
                /* isManaged= */ true,
                /* managedPolicyValue= */ true,
                /* hasRecommendation= */ false,
                /* isFollowing= */ false,
                /* currentUserPref= */ false);
        assertTrue(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));
    }

    @Test
    @SmallTest
    public void testShowBookmarkBar_UserPrefs_isEnabled_Policy_Recommended() {
        mOverrideContextRule.setIsDesktop(true);
        setBooleanPref(
                /* isManaged= */ false,
                /* managedPolicyValue= */ false,
                /* hasRecommendation= */ true,
                /* isFollowing= */ true,
                /* currentUserPref= */ true);
        assertTrue(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));
    }

    @Test
    @SmallTest
    public void testShowBookmarkBar_UserPrefs_isEnabled_Policy_Recommended_Overridden() {
        mOverrideContextRule.setIsDesktop(true);
        setBooleanPref(
                /* isManaged= */ false,
                /* managedPolicyValue= */ false,
                /* hasRecommendation= */ true,
                /* isFollowing= */ false,
                /* currentUserPref= */ false);
        assertFalse(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));
    }

    // ---------------------------------------------------------------------------------------------
    // Group 1: V1 (Boolean) - DevicePrefs
    // ---------------------------------------------------------------------------------------------

    @Test
    @SmallTest
    public void testShowBookmarkBar_DevicePrefs_isEnabled_Default() {
        mOverrideContextRule.setIsDesktop(false);
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        setBooleanPref(
                /* isManaged= */ false,
                /* managedPolicyValue= */ false,
                /* hasRecommendation= */ false,
                /* isFollowing= */ false,
                /* currentUserPref= */ false);
        assertFalse(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));
    }

    @Test
    @SmallTest
    public void testShowBookmarkBar_DevicePrefs_isEnabled_WithUserChoice() {
        mOverrideContextRule.setIsDesktop(false);
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        BookmarkBarUtils.setDevicePrefShowBookmarksBar(true, false);
        setBooleanPref(
                /* isManaged= */ false,
                /* managedPolicyValue= */ false,
                /* hasRecommendation= */ false,
                /* isFollowing= */ false,
                /* currentUserPref= */ false);

        // UserPrefs fallback isn't used when device pref is set, so should be |true|.
        assertTrue(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));
    }

    @Test
    @SmallTest
    public void testShowBookmarkBar_DevicePrefs_isEnabled_Policy_Mandatory() {
        mOverrideContextRule.setIsDesktop(false);
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        setBooleanPref(
                /* isManaged= */ true,
                /* managedPolicyValue= */ true,
                /* hasRecommendation= */ false,
                /* isFollowing= */ false,
                /* currentUserPref= */ false);
        assertTrue(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));
    }

    @Test
    @SmallTest
    public void testShowBookmarkBar_DevicePrefs_isEnabled_Policy_Recommended() {
        mOverrideContextRule.setIsDesktop(false);
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        setBooleanPref(
                /* isManaged= */ false,
                /* managedPolicyValue= */ false,
                /* hasRecommendation= */ true,
                /* isFollowing= */ true,
                /* currentUserPref= */ true);
        assertTrue(
                "Recommended defaults should pass through when device pref absent.",
                BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));
    }

    @Test
    @SmallTest
    public void
            testShowBookmarkBar_DevicePrefs_isEnabled_Policy_Recommended_OverriddenByUserPrefs() {
        mOverrideContextRule.setIsDesktop(false);
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        // If a recommendation exists, but UserPref diverged, and device pref is absent, we deduce
        // original policy recommendation and follow that (NOT the set UserPref).
        setBooleanPref(
                /* isManaged= */ false,
                /* managedPolicyValue= */ false,
                /* hasRecommendation= */ true,
                /* isFollowing= */ false,
                /* currentUserPref= */ false); // Policy recommendation was overridden to |false|.
        // Although a Desktop UserPref was chosen to override the recommended policy, we still give
        // users an option to override the policy separately on tablets, so here should be |true|.
        assertTrue(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));
    }

    @Test
    @SmallTest
    public void
            testShowBookmarkBar_DevicePrefs_isEnabled_Policy_Recommended_OverriddenByDevicePrefs() {
        mOverrideContextRule.setIsDesktop(false);
        BookmarkBarUtils.setDevicePrefShowBookmarksBar(false, false);
        setBooleanPref(
                /* isManaged= */ false,
                /* managedPolicyValue= */ false,
                /* hasRecommendation= */ true,
                /* isFollowing= */ true,
                /* currentUserPref= */ true); // Policy recommendation being followed as |true|.
        // A device pref choice overrides the policy recommendation.
        assertFalse(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));
    }

    // ---------------------------------------------------------------------------------------------
    // Group 2: V2 (Tri-State Integers) - UserPrefs
    // ---------------------------------------------------------------------------------------------

    @Test
    @SmallTest
    public void testBookmarkBarVisibilityState_UserPrefs_Set() {
        mOverrideContextRule.setIsDesktop(true);
        BookmarkBarUtils.setUserPrefsBookmarkBarVisibilityState(
                mProfile, BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP, false);
        verify(mPrefService)
                .setInteger(
                        Pref.BOOKMARK_BAR_VISIBILITY_STATE,
                        BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP);
    }

    @Test
    @SmallTest
    public void testBookmarkBarVisibilityState_UserPrefs_isEnabled_Default() {
        mOverrideContextRule.setIsDesktop(true);
        setIntegerPref(
                /* isManaged= */ false,
                /* managedPolicyValue= */ BookmarkBarVisibilityState.ALWAYS_HIDE,
                /* hasRecommendation= */ false,
                /* isFollowing= */ false,
                /* currentUserPref= */ BookmarkBarVisibilityState.ALWAYS_HIDE);
        assertEquals(
                BookmarkBarVisibilityState.ALWAYS_HIDE,
                BookmarkBarUtils.getUserPrefsBookmarkBarVisibilityState(mProfile));
    }

    @Test
    @SmallTest
    public void testBookmarkBarVisibilityState_UserPrefs_isEnabled_WithUserChoice() {
        mOverrideContextRule.setIsDesktop(true);
        setIntegerPref(
                /* isManaged= */ false,
                /* managedPolicyValue= */ BookmarkBarVisibilityState.ALWAYS_HIDE,
                /* hasRecommendation= */ false,
                /* isFollowing= */ false,
                /* currentUserPref= */ BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP);
        assertEquals(
                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP,
                BookmarkBarUtils.getUserPrefsBookmarkBarVisibilityState(mProfile));
    }

    @Test
    @SmallTest
    public void testBookmarkBarVisibilityState_UserPrefs_isEnabled_Policy_Mandatory() {
        mOverrideContextRule.setIsDesktop(true);
        setIntegerPref(
                /* isManaged= */ true,
                /* managedPolicyValue= */ BookmarkBarVisibilityState.ALWAYS_SHOW,
                /* hasRecommendation= */ false,
                /* isFollowing= */ false,
                /* currentUserPref= */ BookmarkBarVisibilityState.ALWAYS_HIDE);
        assertEquals(
                BookmarkBarVisibilityState.ALWAYS_SHOW,
                BookmarkBarUtils.getUserPrefsBookmarkBarVisibilityState(mProfile));
    }

    @Test
    @SmallTest
    public void testBookmarkBarVisibilityState_UserPrefs_isEnabled_Policy_Recommended() {
        mOverrideContextRule.setIsDesktop(true);
        setIntegerPref(
                /* isManaged= */ false,
                /* managedPolicyValue= */ BookmarkBarVisibilityState.ALWAYS_HIDE,
                /* hasRecommendation= */ true,
                /* isFollowing= */ true,
                /* currentUserPref= */ BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP);
        assertEquals(
                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP,
                BookmarkBarUtils.getUserPrefsBookmarkBarVisibilityState(mProfile));
    }

    @Test
    @SmallTest
    public void testBookmarkBarVisibilityState_UserPrefs_isEnabled_Policy_Recommended_Overridden() {
        mOverrideContextRule.setIsDesktop(true);
        setIntegerPref(
                /* isManaged= */ false,
                /* managedPolicyValue= */ BookmarkBarVisibilityState.ALWAYS_HIDE,
                /* hasRecommendation= */ true,
                /* isFollowing= */ false,
                /* currentUserPref= */ BookmarkBarVisibilityState.ALWAYS_HIDE);
        assertEquals(
                BookmarkBarVisibilityState.ALWAYS_HIDE,
                BookmarkBarUtils.getUserPrefsBookmarkBarVisibilityState(mProfile));
    }

    // ---------------------------------------------------------------------------------------------
    // Group 2: V2 (Tri-State Integers) - DevicePrefs
    // ---------------------------------------------------------------------------------------------

    @Test
    @SmallTest
    public void testBookmarkBarVisibilityState_DevicePrefs_isEnabled_Default() {
        mOverrideContextRule.setIsDesktop(false);
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        setIntegerPref(
                /* isManaged= */ false,
                /* managedPolicyValue= */ BookmarkBarVisibilityState.ALWAYS_HIDE,
                /* hasRecommendation= */ false,
                /* isFollowing= */ false,
                /* currentUserPref= */ BookmarkBarVisibilityState.ALWAYS_HIDE);
        assertEquals(
                BookmarkBarVisibilityState.ALWAYS_HIDE,
                BookmarkBarUtils.getDevicePrefBookmarkBarVisibilityState(mProfile));
    }

    @Test
    @SmallTest
    public void testBookmarkBarVisibilityState_DevicePrefs_isEnabled_WithUserChoice() {
        mOverrideContextRule.setIsDesktop(false);
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        BookmarkBarUtils.setDevicePrefBookmarkBarVisibilityState(
                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP, false);
        setIntegerPref(
                /* isManaged= */ false,
                /* managedPolicyValue= */ BookmarkBarVisibilityState.ALWAYS_HIDE,
                /* hasRecommendation= */ false,
                /* isFollowing= */ false,
                /* currentUserPref= */ BookmarkBarVisibilityState.ALWAYS_HIDE);
        assertEquals(
                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP,
                BookmarkBarUtils.getDevicePrefBookmarkBarVisibilityState(mProfile));
    }

    @Test
    @SmallTest
    public void testBookmarkBarVisibilityState_DevicePrefs_isEnabled_Policy_Mandatory() {
        mOverrideContextRule.setIsDesktop(false);
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        setIntegerPref(
                /* isManaged= */ true,
                /* managedPolicyValue= */ BookmarkBarVisibilityState.ALWAYS_SHOW,
                /* hasRecommendation= */ false,
                /* isFollowing= */ false,
                /* currentUserPref= */ BookmarkBarVisibilityState.ALWAYS_HIDE);
        assertEquals(
                BookmarkBarVisibilityState.ALWAYS_SHOW,
                BookmarkBarUtils.getDevicePrefBookmarkBarVisibilityState(mProfile));
    }

    @Test
    @SmallTest
    public void testBookmarkBarVisibilityState_DevicePrefs_isEnabled_Policy_Recommended() {
        mOverrideContextRule.setIsDesktop(false);
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        setIntegerPref(
                /* isManaged= */ false,
                /* managedPolicyValue= */ BookmarkBarVisibilityState.ALWAYS_HIDE,
                /* hasRecommendation= */ true,
                /* isFollowing= */ true,
                /* currentUserPref= */ BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP);
        assertEquals(
                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP,
                BookmarkBarUtils.getDevicePrefBookmarkBarVisibilityState(mProfile));
    }

    @Test
    @SmallTest
    public void
            testBookmarkBarVisibilityState_DevicePrefs_isEnabled_Policy_Recommended_OverriddenByUserPrefs() {
        mOverrideContextRule.setIsDesktop(false);
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        // User overrides recommended policy to |ALWAYS_HIDE| in UserPrefs, but no device pref is
        // set, so we derive original policy recommendation and use that.
        setIntegerPref(
                /* isManaged= */ false,
                /* managedPolicyValue= */ BookmarkBarVisibilityState.ALWAYS_HIDE,
                /* hasRecommendation= */ true,
                /* isFollowing= */ false,
                /* currentUserPref= */ BookmarkBarVisibilityState.ALWAYS_HIDE);
        assertEquals(
                BookmarkBarVisibilityState.ALWAYS_SHOW,
                BookmarkBarUtils.getDevicePrefBookmarkBarVisibilityState(mProfile));
    }

    @Test
    @SmallTest
    public void
            testBookmarkBarVisibilityState_DevicePrefs_isEnabled_Policy_Recommended_OverriddenByDevicePrefs() {
        mOverrideContextRule.setIsDesktop(false);
        BookmarkBarUtils.setDevicePrefBookmarkBarVisibilityState(
                BookmarkBarVisibilityState.ALWAYS_SHOW, false);
        setIntegerPref(
                /* isManaged= */ false,
                /* managedPolicyValue= */ BookmarkBarVisibilityState.ALWAYS_HIDE,
                /* hasRecommendation= */ true,
                /* isFollowing= */ false,
                /* currentUserPref= */ BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP);
        // The user has override the policy recommendation locally and in UserPrefs, and we will
        // follow the local override for device prefs.
        assertEquals(
                BookmarkBarVisibilityState.ALWAYS_SHOW,
                BookmarkBarUtils.getDevicePrefBookmarkBarVisibilityState(mProfile));
    }

    // ---------------------------------------------------------------------------------------------
    // Baseline Tests (null Profile, compatibility checks, histograms, etc
    // ---------------------------------------------------------------------------------------------

    @Test
    @SmallTest
    public void testIsBookmarkBarManagedByPolicy() {
        assertFalse(
                "Should be false for null profile.",
                BookmarkBarUtils.isUserPrefsShowBookmarkBarManagedByPolicy(null));
        assertFalse(
                "Should be false for null profile (v2).",
                BookmarkBarUtils.isUserPrefsBookmarkBarVisibilityStateManagedByPolicy(null));

        when(mPrefService.isManagedPreference(Pref.SHOW_BOOKMARK_BAR)).thenReturn(true);
        assertTrue(BookmarkBarUtils.isUserPrefsShowBookmarkBarManagedByPolicy(mProfile));
        when(mPrefService.isManagedPreference(Pref.SHOW_BOOKMARK_BAR)).thenReturn(false);
        assertFalse(BookmarkBarUtils.isUserPrefsShowBookmarkBarManagedByPolicy(mProfile));

        when(mPrefService.isManagedPreference(Pref.BOOKMARK_BAR_VISIBILITY_STATE)).thenReturn(true);
        assertTrue(BookmarkBarUtils.isUserPrefsBookmarkBarVisibilityStateManagedByPolicy(mProfile));
        when(mPrefService.isManagedPreference(Pref.BOOKMARK_BAR_VISIBILITY_STATE))
                .thenReturn(false);
        assertFalse(
                BookmarkBarUtils.isUserPrefsBookmarkBarVisibilityStateManagedByPolicy(mProfile));
    }

    @Test
    @SmallTest
    public void testIsBookmarkBarRecommended() {
        assertFalse(
                "Should be false for null profile.",
                BookmarkBarUtils.isUserPrefsShowBookmarkBarRecommended(null));
        assertFalse(
                "Should be false for null profile (v2).",
                BookmarkBarUtils.isUserPrefsBookmarkBarVisibilityStateRecommended(null));

        when(mPrefService.hasRecommendation(Pref.SHOW_BOOKMARK_BAR)).thenReturn(true);
        assertTrue(BookmarkBarUtils.isUserPrefsShowBookmarkBarRecommended(mProfile));
        when(mPrefService.hasRecommendation(Pref.SHOW_BOOKMARK_BAR)).thenReturn(false);
        assertFalse(BookmarkBarUtils.isUserPrefsShowBookmarkBarRecommended(mProfile));

        when(mPrefService.hasRecommendation(Pref.BOOKMARK_BAR_VISIBILITY_STATE)).thenReturn(true);
        assertTrue(BookmarkBarUtils.isUserPrefsBookmarkBarVisibilityStateRecommended(mProfile));
        when(mPrefService.hasRecommendation(Pref.BOOKMARK_BAR_VISIBILITY_STATE)).thenReturn(false);
        assertFalse(BookmarkBarUtils.isUserPrefsBookmarkBarVisibilityStateRecommended(mProfile));
    }

    @Test
    @SmallTest
    public void testIsActivityStateBookmarkBarCompatible() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            // Case: Below "w412dp" threshold w/ feature disabled.
                            RuntimeEnvironment.setQualifiers("w411dp");
                            BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(false);
                            assertFalse(
                                    BookmarkBarUtils.isActivityStateBookmarkBarCompatible(
                                            activity));

                            // Case: Below "w412dp" threshold w/ feature enabled.
                            BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
                            assertFalse(
                                    BookmarkBarUtils.isActivityStateBookmarkBarCompatible(
                                            activity));

                            // Case: At "w412dp" threshold w/ feature disabled.
                            RuntimeEnvironment.setQualifiers("w412dp");
                            BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(false);
                            assertFalse(
                                    BookmarkBarUtils.isActivityStateBookmarkBarCompatible(
                                            activity));

                            // Case: At "w412dp" threshold w/ feature enabled.
                            BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
                            assertTrue(
                                    BookmarkBarUtils.isActivityStateBookmarkBarCompatible(
                                            activity));
                        });
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw599dp")
    public void testIsDeviceBookmarkBarCompatibleOnPhone() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity ->
                                assertFalse(
                                        BookmarkBarUtils.isDeviceBookmarkBarCompatible(activity)));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testIsDeviceBookmarkBarCompatibleOnTablet() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity ->
                                assertTrue(
                                        BookmarkBarUtils.isDeviceBookmarkBarCompatible(activity)));
    }

    @Test
    @SmallTest
    public void testIsBookmarkBarVisible_Desktop() {
        mOverrideContextRule.setIsDesktop(true);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            // Case: feature disallowed and setting disabled.
                            BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(false);
                            BookmarkBarUtils.setSettingEnabledForTesting(false);
                            assertFalse(
                                    BookmarkBarUtils.isBookmarkBarVisible(
                                            activity, mProfile, false));

                            // Case: feature disallowed and setting enabled.
                            BookmarkBarUtils.setSettingEnabledForTesting(true);
                            assertFalse(
                                    BookmarkBarUtils.isBookmarkBarVisible(
                                            activity, mProfile, false));

                            // Case: feature allowed and setting disabled.
                            BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
                            BookmarkBarUtils.setSettingEnabledForTesting(false);
                            assertFalse(
                                    BookmarkBarUtils.isBookmarkBarVisible(
                                            activity, mProfile, false));

                            // Case feature allowed and setting enabled.
                            BookmarkBarUtils.setSettingEnabledForTesting(true);
                            assertTrue(
                                    BookmarkBarUtils.isBookmarkBarVisible(
                                            activity, mProfile, false));
                        });
    }

    @Test
    @SmallTest
    public void testIsBookmarkBarVisible_Tablet() {
        mOverrideContextRule.setIsDesktop(false);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            // Case: feature disallowed.
                            BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(false);
                            assertFalse(
                                    BookmarkBarUtils.isBookmarkBarVisible(
                                            activity, mProfile, false));

                            // Case: feature allowed no device pref (default is false).
                            BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
                            assertFalse(
                                    BookmarkBarUtils.isBookmarkBarVisible(
                                            activity, mProfile, false));

                            // Case: feature allowed explicit device pref
                            BookmarkBarUtils.setDevicePrefShowBookmarksBar(
                                    true, /* fromKeyboardShortcut= */ false);
                            assertTrue(
                                    BookmarkBarUtils.isBookmarkBarVisible(
                                            activity, mProfile, false));
                        });
    }

    @Test
    @SmallTest
    public void testIsBookmarkBarVisible_XR() {
        mOverrideContextRule.setIsDesktop(false);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
                            BookmarkBarUtils.setDevicePrefShowBookmarksBar(
                                    true, /* fromKeyboardShortcut= */ false);

                            // Case: XR full space mode is enabled.
                            assertFalse(
                                    BookmarkBarUtils.isBookmarkBarVisible(
                                            activity, mProfile, true));

                            // Case: XR full space mode is disabled.
                            assertTrue(
                                    BookmarkBarUtils.isBookmarkBarVisible(
                                            activity, mProfile, false));

                            // Case: XR supplier is null.
                            assertTrue(
                                    BookmarkBarUtils.isBookmarkBarVisible(
                                            activity, mProfile, false));
                        });
    }

    @Test
    @SmallTest
    public void testMetrics_SetUserPrefsShowBookmarksBar() {
        mOverrideContextRule.setIsDesktop(true);

        var histogramWatcher =
                newBuilder()
                        .expectBooleanRecordTimes(
                                BookmarkBarUtils.TOGGLED_BY_KEYBOARD_SHORTCUT, true, 1)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_IN_SETTINGS)
                        .build();

        BookmarkBarUtils.setUserPrefsShowBookmarksBar(
                mProfile, true, /* fromKeyboardShortcut= */ true);

        histogramWatcher.assertExpected();
        verify(mPrefService).setBoolean(Pref.SHOW_BOOKMARK_BAR, true);

        var histogramWatcher2 =
                newBuilder()
                        .expectBooleanRecordTimes(BookmarkBarUtils.TOGGLED_IN_SETTINGS, false, 1)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_BY_KEYBOARD_SHORTCUT)
                        .build();

        BookmarkBarUtils.setUserPrefsShowBookmarksBar(
                mProfile, false, /* fromKeyboardShortcut= */ false);

        histogramWatcher2.assertExpected();
        verify(mPrefService).setBoolean(Pref.SHOW_BOOKMARK_BAR, false);
    }

    @Test
    @SmallTest
    public void testMetrics_SetDevicePrefShowBookmarksBar() {
        mOverrideContextRule.setIsDesktop(false);

        var histogramWatcher =
                newBuilder()
                        .expectBooleanRecordTimes(
                                BookmarkBarUtils.TOGGLED_BY_KEYBOARD_SHORTCUT, true, 1)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_IN_SETTINGS)
                        .build();

        BookmarkBarUtils.setDevicePrefShowBookmarksBar(true, /* fromKeyboardShortcut= */ true);

        histogramWatcher.assertExpected();
        assertTrue(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));

        var histogramWatcher2 =
                newBuilder()
                        .expectBooleanRecordTimes(BookmarkBarUtils.TOGGLED_IN_SETTINGS, false, 1)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_BY_KEYBOARD_SHORTCUT)
                        .build();

        BookmarkBarUtils.setDevicePrefShowBookmarksBar(false, /* fromKeyboardShortcut= */ false);

        histogramWatcher2.assertExpected();
        assertFalse(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));
    }
}
