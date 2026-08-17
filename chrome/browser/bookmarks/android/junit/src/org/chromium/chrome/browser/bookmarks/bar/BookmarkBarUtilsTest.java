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

import android.app.Activity;

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
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils.BookmarkBarSettingChangeOrigin;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils.BookmarkBarShownReason;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils.BookmarkBarVisibilityStateOnStartUpReason;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.components.bookmarks.BookmarkBarVisibilityState;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.JUnitTestGURLs;

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
    @Mock private Tab mTab;
    @Mock private UserPrefsJni mUserPrefsJni;
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
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

    private void setUserPrefShowBookmarksBar(boolean show) {
        setBooleanPref(
                /* isManaged= */ false,
                /* managedPolicyValue= */ false,
                /* hasRecommendation= */ false,
                /* isFollowing= */ false,
                /* currentUserPref= */ show);
    }

    private void setUserPrefState(@BookmarkBarVisibilityState int state) {
        setIntegerPref(
                /* isManaged= */ false,
                /* managedPolicyValue= */ BookmarkBarVisibilityState.ALWAYS_HIDE,
                /* hasRecommendation= */ false,
                /* isFollowing= */ false,
                /* currentUserPref= */ state);
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
                mProfile,
                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP,
                BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS);
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
                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP,
                BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS);
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
                BookmarkBarVisibilityState.ALWAYS_SHOW,
                BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS);
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
    // Baseline Tests (null Profile, compatibility checks, histograms, etc.)
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
        // Case: Below "w412dp" threshold w/ feature disabled.
        RuntimeEnvironment.setQualifiers("w411dp");
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(false);
        assertFalse(BookmarkBarUtils.isActivityStateBookmarkBarCompatible(mActivity));

        // Case: Below "w412dp" threshold w/ feature enabled.
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
        assertFalse(BookmarkBarUtils.isActivityStateBookmarkBarCompatible(mActivity));

        // Case: At "w412dp" threshold w/ feature disabled.
        RuntimeEnvironment.setQualifiers("w412dp");
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(false);
        assertFalse(BookmarkBarUtils.isActivityStateBookmarkBarCompatible(mActivity));

        // Case: At "w412dp" threshold w/ feature enabled.
        BookmarkBarUtils.setDeviceBookmarkBarCompatibleForTesting(true);
        assertTrue(BookmarkBarUtils.isActivityStateBookmarkBarCompatible(mActivity));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw599dp")
    public void testIsDeviceBookmarkBarCompatibleOnPhone() {
        assertFalse(BookmarkBarUtils.isDeviceBookmarkBarCompatible(mActivity));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testIsDeviceBookmarkBarCompatibleOnTablet() {
        assertTrue(BookmarkBarUtils.isDeviceBookmarkBarCompatible(mActivity));
    }

    // ---------------------------------------------------------------------------------------------
    // Group 1: V1 (Boolean) - Top-level visibility getter/setter, UserPrefs/DevicePrefs.
    // ---------------------------------------------------------------------------------------------

    @Test
    @SmallTest
    public void testIsBookmarkBarVisible_Desktop() {
        mOverrideContextRule.setIsDesktop(true);

        // Case: feature disallowed and setting disabled.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(false);
        BookmarkBarUtils.setSettingEnabledForTesting(false);
        assertFalse(BookmarkBarUtils.isBookmarkBarVisible(mActivity, mProfile, false));

        // Case: feature disallowed and setting enabled.
        BookmarkBarUtils.setSettingEnabledForTesting(true);
        assertFalse(BookmarkBarUtils.isBookmarkBarVisible(mActivity, mProfile, false));

        // Case: feature allowed and setting disabled.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        BookmarkBarUtils.setSettingEnabledForTesting(false);
        assertFalse(BookmarkBarUtils.isBookmarkBarVisible(mActivity, mProfile, false));

        // Case feature allowed and setting enabled.
        BookmarkBarUtils.setSettingEnabledForTesting(true);
        assertTrue(BookmarkBarUtils.isBookmarkBarVisible(mActivity, mProfile, false));
    }

    @Test
    @SmallTest
    public void testIsBookmarkBarVisible_Tablet() {
        mOverrideContextRule.setIsDesktop(false);

        // Case: feature disallowed.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(false);
        assertFalse(BookmarkBarUtils.isBookmarkBarVisible(mActivity, mProfile, false));

        // Case: feature allowed no device pref (default is false).
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        assertFalse(BookmarkBarUtils.isBookmarkBarVisible(mActivity, mProfile, false));

        // Case: feature allowed explicit device pref
        BookmarkBarUtils.setDevicePrefShowBookmarksBar(true, /* fromKeyboardShortcut= */ false);
        assertTrue(BookmarkBarUtils.isBookmarkBarVisible(mActivity, mProfile, false));
    }

    @Test
    @SmallTest
    public void testIsBookmarkBarVisible_XR() {
        mOverrideContextRule.setIsDesktop(false);
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        BookmarkBarUtils.setDevicePrefShowBookmarksBar(true, /* fromKeyboardShortcut= */ false);

        // Case: XR full space mode is enabled.
        assertFalse(BookmarkBarUtils.isBookmarkBarVisible(mActivity, mProfile, true));

        // Case: XR full space mode is disabled.
        assertTrue(BookmarkBarUtils.isBookmarkBarVisible(mActivity, mProfile, false));

        // Case: XR supplier is null.
        assertTrue(BookmarkBarUtils.isBookmarkBarVisible(mActivity, mProfile, false));
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

    @Test
    @SmallTest
    public void testMetrics_RecordStartUpMetrics_Desktop() {
        mOverrideContextRule.setIsDesktop(true);
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);

        // Case 1: user pref is enabled
        setUserPrefShowBookmarksBar(true);
        var watcher1 =
                newBuilder()
                        .expectBooleanRecordTimes(
                                BookmarkBarUtils.BOOKMARK_BAR_SHOWN_ON_START_UP, true, 1)
                        .expectIntRecord(
                                BookmarkBarUtils.BOOKMARK_BAR_SHOWN_ON_START_UP_REASON,
                                BookmarkBarShownReason.ENABLED_BY_USER_PREF)
                        .build();
        BookmarkBarUtils.recordStartUpMetrics(mActivity, mProfile, false);
        watcher1.assertExpected();

        // Case 2: user pref is disabled
        setUserPrefShowBookmarksBar(false);
        var watcher2 =
                newBuilder()
                        .expectBooleanRecordTimes(
                                BookmarkBarUtils.BOOKMARK_BAR_SHOWN_ON_START_UP, false, 1)
                        .expectIntRecord(
                                BookmarkBarUtils.BOOKMARK_BAR_SHOWN_ON_START_UP_REASON,
                                BookmarkBarShownReason.DISABLED_BY_USER_PREF)
                        .build();
        BookmarkBarUtils.recordStartUpMetrics(mActivity, mProfile, false);
        watcher2.assertExpected();
    }

    @Test
    @SmallTest
    public void testMetrics_RecordStartUpMetrics_Tablet() {
        mOverrideContextRule.setIsDesktop(false);
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);

        // Case 1: explicit device pref enabled
        BookmarkBarUtils.setDevicePrefShowBookmarksBar(true, false);
        var watcher1 =
                newBuilder()
                        .expectBooleanRecordTimes(
                                BookmarkBarUtils.BOOKMARK_BAR_SHOWN_ON_START_UP, true, 1)
                        .expectIntRecord(
                                BookmarkBarUtils.BOOKMARK_BAR_SHOWN_ON_START_UP_REASON,
                                BookmarkBarShownReason.ENABLED_BY_DEVICE_PREF)
                        .build();
        BookmarkBarUtils.recordStartUpMetrics(mActivity, mProfile, false);
        watcher1.assertExpected();

        // Case 2: explicit device pref disabled
        BookmarkBarUtils.setDevicePrefShowBookmarksBar(false, false);
        var watcher2 =
                newBuilder()
                        .expectBooleanRecordTimes(
                                BookmarkBarUtils.BOOKMARK_BAR_SHOWN_ON_START_UP, false, 1)
                        .expectIntRecord(
                                BookmarkBarUtils.BOOKMARK_BAR_SHOWN_ON_START_UP_REASON,
                                BookmarkBarShownReason.DISABLED_BY_DEVICE_PREF)
                        .build();
        BookmarkBarUtils.recordStartUpMetrics(mActivity, mProfile, false);
        watcher2.assertExpected();

        // Case 3: default (no explicit user setting)
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        var watcher3 =
                newBuilder()
                        .expectNoRecords(BookmarkBarUtils.BOOKMARK_BAR_SHOWN_ON_START_UP)
                        .expectIntRecord(
                                BookmarkBarUtils.BOOKMARK_BAR_SHOWN_ON_START_UP_REASON,
                                BookmarkBarShownReason.DISABLED_BY_FEATURE_PARAM)
                        .build();
        BookmarkBarUtils.recordStartUpMetrics(mActivity, mProfile, false);
        watcher3.assertExpected();
    }

    // ---------------------------------------------------------------------------------------------
    // Group 2: V2 (Tri-State Integers) - Top-level visibility getter/setter, UserPrefs/DevicePrefs.
    // ---------------------------------------------------------------------------------------------

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testGetBookmarkBarVisibilityState_Desktop() {
        mOverrideContextRule.setIsDesktop(true);

        // Cases for feature disallowed.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(false);

        setUserPrefState(BookmarkBarVisibilityState.ALWAYS_SHOW);
        assertEquals(
                BookmarkBarVisibilityState.ALWAYS_HIDE,
                BookmarkBarUtils.getBookmarkBarVisibilityState(mActivity, mProfile, false));

        setUserPrefState(BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP);
        assertEquals(
                BookmarkBarVisibilityState.ALWAYS_HIDE,
                BookmarkBarUtils.getBookmarkBarVisibilityState(mActivity, mProfile, false));

        setUserPrefState(BookmarkBarVisibilityState.ALWAYS_HIDE);
        assertEquals(
                BookmarkBarVisibilityState.ALWAYS_HIDE,
                BookmarkBarUtils.getBookmarkBarVisibilityState(mActivity, mProfile, false));

        // Cases for feature allowed.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);

        setUserPrefState(BookmarkBarVisibilityState.ALWAYS_SHOW);
        assertEquals(
                BookmarkBarVisibilityState.ALWAYS_SHOW,
                BookmarkBarUtils.getBookmarkBarVisibilityState(mActivity, mProfile, false));

        setUserPrefState(BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP);
        assertEquals(
                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP,
                BookmarkBarUtils.getBookmarkBarVisibilityState(mActivity, mProfile, false));

        setUserPrefState(BookmarkBarVisibilityState.ALWAYS_HIDE);
        assertEquals(
                BookmarkBarVisibilityState.ALWAYS_HIDE,
                BookmarkBarUtils.getBookmarkBarVisibilityState(mActivity, mProfile, false));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testGetBookmarkBarVisibilityState_Tablet() {
        mOverrideContextRule.setIsDesktop(false);

        // Case: feature disallowed.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(false);
        assertEquals(
                BookmarkBarVisibilityState.ALWAYS_HIDE,
                BookmarkBarUtils.getBookmarkBarVisibilityState(mActivity, mProfile, false));

        // Case: feature allowed no device pref (default is always hide).
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        assertEquals(
                BookmarkBarVisibilityState.ALWAYS_HIDE,
                BookmarkBarUtils.getBookmarkBarVisibilityState(mActivity, mProfile, false));

        // Case: feature allowed explicit device pref
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        BookmarkBarUtils.setDevicePrefBookmarkBarVisibilityState(
                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP,
                BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS);
        assertEquals(
                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP,
                BookmarkBarUtils.getBookmarkBarVisibilityState(mActivity, mProfile, false));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testGetBookmarkBarVisibilityState_XR() {
        mOverrideContextRule.setIsDesktop(false);
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        BookmarkBarUtils.setBookmarkBarVisibilityState(
                mProfile,
                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP,
                BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS);

        // Case: XR full space mode is enabled.
        assertEquals(
                BookmarkBarVisibilityState.ALWAYS_HIDE,
                BookmarkBarUtils.getBookmarkBarVisibilityState(mActivity, mProfile, true));

        // Case: XR full space mode is disabled.
        assertEquals(
                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP,
                BookmarkBarUtils.getBookmarkBarVisibilityState(mActivity, mProfile, false));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testMetrics_SetUserPrefsBookmarkBarVisibilityState() {
        mOverrideContextRule.setIsDesktop(true);

        // 1. KEYBOARD_SHORTCUT -> ALWAYS_SHOW
        var watcher1 =
                newBuilder()
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_CHANGE_ORIGIN,
                                BookmarkBarSettingChangeOrigin.KEYBOARD_SHORTCUT)
                        .expectIntRecord(
                                BookmarkBarUtils.TOGGLED_KEYBOARD,
                                BookmarkBarVisibilityState.ALWAYS_SHOW)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_APPEARANCE_SETTINGS)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_CONTEXT_MENU)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_APP_MENU)
                        .build();
        BookmarkBarUtils.setUserPrefsBookmarkBarVisibilityState(
                mProfile,
                BookmarkBarVisibilityState.ALWAYS_SHOW,
                BookmarkBarSettingChangeOrigin.KEYBOARD_SHORTCUT);
        watcher1.assertExpected();
        verify(mPrefService)
                .setInteger(
                        Pref.BOOKMARK_BAR_VISIBILITY_STATE, BookmarkBarVisibilityState.ALWAYS_SHOW);

        // 2. APPEARANCE_SETTINGS -> ONLY_SHOW_ON_NTP
        var watcher2 =
                newBuilder()
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_CHANGE_ORIGIN,
                                BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS)
                        .expectIntRecord(
                                BookmarkBarUtils.TOGGLED_APPEARANCE_SETTINGS,
                                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_KEYBOARD)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_CONTEXT_MENU)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_APP_MENU)
                        .build();
        BookmarkBarUtils.setUserPrefsBookmarkBarVisibilityState(
                mProfile,
                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP,
                BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS);
        watcher2.assertExpected();
        verify(mPrefService)
                .setInteger(
                        Pref.BOOKMARK_BAR_VISIBILITY_STATE,
                        BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP);

        // 3. BOOKMARK_BAR_CONTEXT_MENU -> ALWAYS_HIDE
        var watcher3 =
                newBuilder()
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_CHANGE_ORIGIN,
                                BookmarkBarSettingChangeOrigin.BOOKMARK_BAR_CONTEXT_MENU)
                        .expectIntRecord(
                                BookmarkBarUtils.TOGGLED_CONTEXT_MENU,
                                BookmarkBarVisibilityState.ALWAYS_HIDE)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_KEYBOARD)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_APPEARANCE_SETTINGS)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_APP_MENU)
                        .build();
        BookmarkBarUtils.setUserPrefsBookmarkBarVisibilityState(
                mProfile,
                BookmarkBarVisibilityState.ALWAYS_HIDE,
                BookmarkBarSettingChangeOrigin.BOOKMARK_BAR_CONTEXT_MENU);
        watcher3.assertExpected();
        verify(mPrefService)
                .setInteger(
                        Pref.BOOKMARK_BAR_VISIBILITY_STATE, BookmarkBarVisibilityState.ALWAYS_HIDE);

        // 4. APP_MENU -> ALWAYS_SHOW
        var watcher4 =
                newBuilder()
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_CHANGE_ORIGIN,
                                BookmarkBarSettingChangeOrigin.APP_MENU)
                        .expectIntRecord(
                                BookmarkBarUtils.TOGGLED_APP_MENU,
                                BookmarkBarVisibilityState.ALWAYS_SHOW)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_KEYBOARD)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_APPEARANCE_SETTINGS)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_CONTEXT_MENU)
                        .build();
        BookmarkBarUtils.setUserPrefsBookmarkBarVisibilityState(
                mProfile,
                BookmarkBarVisibilityState.ALWAYS_SHOW,
                BookmarkBarSettingChangeOrigin.APP_MENU);
        watcher4.assertExpected();
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testMetrics_SetDevicePrefBookmarkBarVisibilityState() {
        mOverrideContextRule.setIsDesktop(false);

        // 1. KEYBOARD_SHORTCUT -> ALWAYS_SHOW
        var watcher1 =
                newBuilder()
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_CHANGE_ORIGIN,
                                BookmarkBarSettingChangeOrigin.KEYBOARD_SHORTCUT)
                        .expectIntRecord(
                                BookmarkBarUtils.TOGGLED_KEYBOARD,
                                BookmarkBarVisibilityState.ALWAYS_SHOW)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_APPEARANCE_SETTINGS)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_CONTEXT_MENU)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_APP_MENU)
                        .build();
        BookmarkBarUtils.setDevicePrefBookmarkBarVisibilityState(
                BookmarkBarVisibilityState.ALWAYS_SHOW,
                BookmarkBarSettingChangeOrigin.KEYBOARD_SHORTCUT);
        watcher1.assertExpected();
        assertEquals(
                BookmarkBarVisibilityState.ALWAYS_SHOW,
                BookmarkBarUtils.getDevicePrefBookmarkBarVisibilityState(mProfile));

        // 2. APPEARANCE_SETTINGS -> ONLY_SHOW_ON_NTP
        var watcher2 =
                newBuilder()
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_CHANGE_ORIGIN,
                                BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS)
                        .expectIntRecord(
                                BookmarkBarUtils.TOGGLED_APPEARANCE_SETTINGS,
                                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_KEYBOARD)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_CONTEXT_MENU)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_APP_MENU)
                        .build();
        BookmarkBarUtils.setDevicePrefBookmarkBarVisibilityState(
                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP,
                BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS);
        watcher2.assertExpected();
        assertEquals(
                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP,
                BookmarkBarUtils.getDevicePrefBookmarkBarVisibilityState(mProfile));

        // 3. BOOKMARK_BAR_CONTEXT_MENU -> ALWAYS_HIDE
        var watcher3 =
                newBuilder()
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_CHANGE_ORIGIN,
                                BookmarkBarSettingChangeOrigin.BOOKMARK_BAR_CONTEXT_MENU)
                        .expectIntRecord(
                                BookmarkBarUtils.TOGGLED_CONTEXT_MENU,
                                BookmarkBarVisibilityState.ALWAYS_HIDE)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_KEYBOARD)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_APPEARANCE_SETTINGS)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_APP_MENU)
                        .build();
        BookmarkBarUtils.setDevicePrefBookmarkBarVisibilityState(
                BookmarkBarVisibilityState.ALWAYS_HIDE,
                BookmarkBarSettingChangeOrigin.BOOKMARK_BAR_CONTEXT_MENU);
        watcher3.assertExpected();
        assertEquals(
                BookmarkBarVisibilityState.ALWAYS_HIDE,
                BookmarkBarUtils.getDevicePrefBookmarkBarVisibilityState(mProfile));

        // 4. APP_MENU -> ALWAYS_SHOW
        var watcher4 =
                newBuilder()
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_CHANGE_ORIGIN,
                                BookmarkBarSettingChangeOrigin.APP_MENU)
                        .expectIntRecord(
                                BookmarkBarUtils.TOGGLED_APP_MENU,
                                BookmarkBarVisibilityState.ALWAYS_SHOW)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_KEYBOARD)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_APPEARANCE_SETTINGS)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_CONTEXT_MENU)
                        .build();
        BookmarkBarUtils.setDevicePrefBookmarkBarVisibilityState(
                BookmarkBarVisibilityState.ALWAYS_SHOW, BookmarkBarSettingChangeOrigin.APP_MENU);
        watcher4.assertExpected();
        assertEquals(
                BookmarkBarVisibilityState.ALWAYS_SHOW,
                BookmarkBarUtils.getDevicePrefBookmarkBarVisibilityState(mProfile));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testIsBookmarkBarVisibleForState_Desktop() {
        mOverrideContextRule.setIsDesktop(true);
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);

        // 1. ALWAYS_SHOW: returns true for both NTP and standard pages.
        setUserPrefState(BookmarkBarVisibilityState.ALWAYS_SHOW);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        assertTrue(BookmarkBarUtils.isBookmarkBarVisibleForState(mActivity, mProfile, false, mTab));
        assertTrue(BookmarkBarUtils.isBookmarkBarVisibleForState(mActivity, mProfile, false, null));

        // 2. ALWAYS_HIDE: returns false for all pages.
        setUserPrefState(BookmarkBarVisibilityState.ALWAYS_HIDE);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        assertFalse(
                BookmarkBarUtils.isBookmarkBarVisibleForState(mActivity, mProfile, false, mTab));

        // 3. ONLY_SHOW_ON_NTP: returns true only on NTP.
        setUserPrefState(BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP);
        assertFalse(
                BookmarkBarUtils.isBookmarkBarVisibleForState(mActivity, mProfile, false, null));
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        assertFalse(
                BookmarkBarUtils.isBookmarkBarVisibleForState(mActivity, mProfile, false, mTab));
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        assertTrue(BookmarkBarUtils.isBookmarkBarVisibleForState(mActivity, mProfile, false, mTab));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testIsBookmarkBarVisibleForState_Tablet() {
        mOverrideContextRule.setIsDesktop(false);
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);

        // 1. ALWAYS_SHOW on tablet.
        BookmarkBarUtils.setDevicePrefBookmarkBarVisibilityState(
                BookmarkBarVisibilityState.ALWAYS_SHOW,
                BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        assertTrue(BookmarkBarUtils.isBookmarkBarVisibleForState(mActivity, mProfile, false, mTab));
        assertTrue(BookmarkBarUtils.isBookmarkBarVisibleForState(mActivity, mProfile, false, null));

        // 2. ALWAYS_HIDE on tablet.
        BookmarkBarUtils.setDevicePrefBookmarkBarVisibilityState(
                BookmarkBarVisibilityState.ALWAYS_HIDE,
                BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        assertFalse(
                BookmarkBarUtils.isBookmarkBarVisibleForState(mActivity, mProfile, false, mTab));

        // 3. ONLY_SHOW_ON_NTP on tablet.
        BookmarkBarUtils.setDevicePrefBookmarkBarVisibilityState(
                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP,
                BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS);
        assertFalse(
                BookmarkBarUtils.isBookmarkBarVisibleForState(mActivity, mProfile, false, null));
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        assertFalse(
                BookmarkBarUtils.isBookmarkBarVisibleForState(mActivity, mProfile, false, mTab));
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        assertTrue(BookmarkBarUtils.isBookmarkBarVisibleForState(mActivity, mProfile, false, mTab));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testMetrics_RecordStartUpMetricsForVisibilityState_Desktop() {
        mOverrideContextRule.setIsDesktop(true);

        // 1. ALWAYS_SHOW: Verifies that even if activity is incompatible for testing, the
        // underlying user preference is recorded on startup.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(false);
        setUserPrefState(BookmarkBarVisibilityState.ALWAYS_SHOW);
        var watcher1 =
                newBuilder()
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_ON_START_UP,
                                BookmarkBarVisibilityState.ALWAYS_SHOW)
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_ON_START_UP_REASON,
                                BookmarkBarVisibilityStateOnStartUpReason.ALWAYS_SHOW_BY_USER_PREF)
                        .build();
        BookmarkBarUtils.recordStartUpMetricsForVisibilityState(mProfile);
        watcher1.assertExpected();

        // 2. ONLY_SHOW_ON_NTP
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        setUserPrefState(BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP);
        var watcher2 =
                newBuilder()
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_ON_START_UP,
                                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP)
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_ON_START_UP_REASON,
                                BookmarkBarVisibilityStateOnStartUpReason
                                        .ONLY_SHOW_ON_NTP_BY_USER_PREF)
                        .build();
        BookmarkBarUtils.recordStartUpMetricsForVisibilityState(mProfile);
        watcher2.assertExpected();

        // 3. ALWAYS_HIDE
        setUserPrefState(BookmarkBarVisibilityState.ALWAYS_HIDE);
        var watcher3 =
                newBuilder()
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_ON_START_UP,
                                BookmarkBarVisibilityState.ALWAYS_HIDE)
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_ON_START_UP_REASON,
                                BookmarkBarVisibilityStateOnStartUpReason.ALWAYS_HIDE_BY_USER_PREF)
                        .build();
        BookmarkBarUtils.recordStartUpMetricsForVisibilityState(mProfile);
        watcher3.assertExpected();
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testMetrics_RecordStartUpMetricsForVisibilityState_Tablet() {
        mOverrideContextRule.setIsDesktop(false);

        // 1. Explicit ALWAYS_SHOW: Verifies that even if activity is incompatible, the device
        // setting is recorded directly.
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(false);
        BookmarkBarUtils.setDevicePrefBookmarkBarVisibilityState(
                BookmarkBarVisibilityState.ALWAYS_SHOW,
                BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS);
        var watcher1 =
                newBuilder()
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_ON_START_UP,
                                BookmarkBarVisibilityState.ALWAYS_SHOW)
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_ON_START_UP_REASON,
                                BookmarkBarVisibilityStateOnStartUpReason
                                        .ALWAYS_SHOW_BY_DEVICE_PREF)
                        .build();
        BookmarkBarUtils.recordStartUpMetricsForVisibilityState(mProfile);
        watcher1.assertExpected();

        // 2. Explicit ONLY_SHOW_ON_NTP
        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        BookmarkBarUtils.setDevicePrefBookmarkBarVisibilityState(
                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP,
                BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS);
        var watcher2 =
                newBuilder()
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_ON_START_UP,
                                BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP)
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_ON_START_UP_REASON,
                                BookmarkBarVisibilityStateOnStartUpReason
                                        .ONLY_SHOW_ON_NTP_BY_DEVICE_PREF)
                        .build();
        BookmarkBarUtils.recordStartUpMetricsForVisibilityState(mProfile);
        watcher2.assertExpected();

        // 3. Explicit ALWAYS_HIDE
        BookmarkBarUtils.setDevicePrefBookmarkBarVisibilityState(
                BookmarkBarVisibilityState.ALWAYS_HIDE,
                BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS);
        var watcher3 =
                newBuilder()
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_ON_START_UP,
                                BookmarkBarVisibilityState.ALWAYS_HIDE)
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_ON_START_UP_REASON,
                                BookmarkBarVisibilityStateOnStartUpReason
                                        .ALWAYS_HIDE_BY_DEVICE_PREF)
                        .build();
        BookmarkBarUtils.recordStartUpMetricsForVisibilityState(mProfile);
        watcher3.assertExpected();

        // 4. Default device state (no explicit setting)
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        var watcher4 =
                newBuilder()
                        .expectNoRecords(BookmarkBarUtils.VISIBILITY_STATE_ON_START_UP)
                        .expectIntRecord(
                                BookmarkBarUtils.VISIBILITY_STATE_ON_START_UP_REASON,
                                BookmarkBarVisibilityStateOnStartUpReason.DEFAULT_DEVICE_VALUE)
                        .build();
        BookmarkBarUtils.recordStartUpMetricsForVisibilityState(mProfile);
        watcher4.assertExpected();
    }
}
