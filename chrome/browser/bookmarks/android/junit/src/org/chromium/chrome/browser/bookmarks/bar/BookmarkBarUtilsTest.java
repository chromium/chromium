// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.when;

import androidx.annotation.NonNull;
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
import org.mockito.stubbing.Answer;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link BookmarkBarUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
public class BookmarkBarUtilsTest {

    private static final String PHONE_QUALIFIER =
            "sw" + (DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP - 1) + "dp";
    private static final String TABLET_QUALIFIER =
            "sw" + DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP + "dp";

    @Rule
    public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule
    public OverrideContextWrapperTestRule mOverrideContextRule =
            new OverrideContextWrapperTestRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PrefService mPrefService;
    @Mock private Profile mProfile;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private UserPrefsJni mUserPrefsJni;

    private final AtomicBoolean mSetting = new AtomicBoolean();

    private ObservableSupplierImpl<ProfileProvider> mProfileProviderSupplier;

    /** Helper class to mock different policy configurations for the bookmark bar. */
    private class BookmarkBarPolicyBuilder {
        private boolean mIsManaged;
        private boolean mManagedValue;
        private boolean mHasRecommendation;
        private boolean mIsFromRecommendation;

        BookmarkBarPolicyBuilder setManaged(boolean isManaged, boolean value) {
            mIsManaged = isManaged;
            mManagedValue = value;
            return this;
        }

        BookmarkBarPolicyBuilder setRecommended(
                boolean hasRecommendation, boolean isFromRecommendation) {
            mHasRecommendation = hasRecommendation;
            mIsFromRecommendation = isFromRecommendation;
            return this;
        }

        void build() {
            // Reset the mock for getBoolean to its default stateful behavior defined in setUp().
            // mSetting simulates the real PrefService by acting as the memory or external storage
            // for our mock object.
            when(mPrefService.getBoolean(Pref.SHOW_BOOKMARK_BAR)).thenAnswer(i -> mSetting.get());

            when(mPrefService.isManagedPreference(Pref.SHOW_BOOKMARK_BAR)).thenReturn(mIsManaged);
            if (mIsManaged) {
                when(mPrefService.getBoolean(Pref.SHOW_BOOKMARK_BAR)).thenReturn(mManagedValue);
            }

            when(mPrefService.hasRecommendation(Pref.SHOW_BOOKMARK_BAR))
                    .thenReturn(mHasRecommendation);
            when(mPrefService.isRecommendedPreference(Pref.SHOW_BOOKMARK_BAR))
                    .thenReturn(mIsFromRecommendation);
        }
    }

    @Before
    public void setUp() {
        doAnswer(runCallbackWithValueAtIndex(mSetting::set, 1))
                .when(mPrefService)
                .setBoolean(eq(Pref.SHOW_BOOKMARK_BAR), anyBoolean());

        when(mPrefService.getBoolean(Pref.SHOW_BOOKMARK_BAR)).thenAnswer(i -> mSetting.get());
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);

        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);

        mProfileProviderSupplier = new ObservableSupplierImpl<>(mProfileProvider);

        // Explicitly override FeatureParam for consistency.
        FeatureOverrides.Builder overrides = FeatureOverrides.newBuilder();
        overrides =
                overrides.param(ChromeFeatureList.ANDROID_BOOKMARK_BAR, "show_bookmark_bar", true);
        overrides.apply();
    }

    @After
    public void tearDown() {
        UserPrefsJni.setInstanceForTesting(null);
        mOverrideContextRule.setIsDesktop(false);
    }

    @Test
    @SmallTest
    public void testIsBookmarkBarManagedByPolicy() {
        assertFalse(
                "Should be false for null profile.",
                BookmarkBarUtils.isBookmarkBarManagedByPolicy(null));

        when(mPrefService.isManagedPreference(Pref.SHOW_BOOKMARK_BAR)).thenReturn(true);
        assertTrue(
                "Should be true when preference is managed.",
                BookmarkBarUtils.isBookmarkBarManagedByPolicy(mProfile));

        when(mPrefService.isManagedPreference(Pref.SHOW_BOOKMARK_BAR)).thenReturn(false);
        assertFalse(
                "Should be false when preference is not managed.",
                BookmarkBarUtils.isBookmarkBarManagedByPolicy(mProfile));
    }

    @Test
    @SmallTest
    public void testIsBookmarkBarRecommended() {
        assertFalse(
                "Should be false for null profile.",
                BookmarkBarUtils.isBookmarkBarRecommended(null));

        when(mPrefService.hasRecommendation(Pref.SHOW_BOOKMARK_BAR)).thenReturn(true);
        assertTrue(
                "Should be true when pref service has a recommendation.",
                BookmarkBarUtils.isBookmarkBarRecommended(mProfile));

        when(mPrefService.hasRecommendation(Pref.SHOW_BOOKMARK_BAR)).thenReturn(false);
        assertFalse(
                "Should be false when pref service has no recommendation.",
                BookmarkBarUtils.isBookmarkBarRecommended(mProfile));
    }

    @Test
    @SmallTest
    public void testIsDevicePrefShowBookmarksBarEnabled_PolicyScenarios() {
        mOverrideContextRule.setIsDesktop(false);

        // Case 1: Mandatory policy exists, policy set to show bookmarks bar enabled.
        new BookmarkBarPolicyBuilder().setManaged(/* isManaged= */ true, /* value= */ true).build();
        assertTrue(
                "Should be true when managed by policy to be on.",
                BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));

        // Case 2: Mandatory policy exists, policy set to show bookmarks bar disabled.
        new BookmarkBarPolicyBuilder()
                .setManaged(/* isManaged= */ true, /* value= */ false)
                .build();
        assertFalse(
                "Should be false when managed by policy to be off.",
                BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));

        // Case 3: Recommended policy exists (toggle on) and user has not set a preference.
        mSetting.set(true); // mock mPrefService.getBoolean(Pref.SHOW_BOOKMARK_BAR).
        new BookmarkBarPolicyBuilder()
                .setManaged(/* isManaged= */ false, /* value= */ false)
                .setRecommended(/* hasRecommendation= */ true, /* isFromRecommendation= */ true)
                .build();
        assertTrue(
                "Should be true when value is from recommendation (on).",
                BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));

        // Case 4: Recommended policy exists (toggle on) but user has overridden it to off.

        // Sets the shadow SharedPref (a simple hashmap in memory) to disabled and mSetting (pref
        // service simulation) variable to false.
        BookmarkBarUtils.setDevicePrefShowBookmarksBar(
                mProfile, /* enabled= */ false, /* fromKeyboardShortcut= */ false);

        // Simulates the policy configuration.
        new BookmarkBarPolicyBuilder()
                .setManaged(/* isManaged= */ false, /* value= */ false)
                .setRecommended(/* hasRecommendation= */ true, /* isFromRecommendation= */ false)
                .build();

        // #isDevicePrefShowBookmarksBarEnabled should fallback to the user's local choice and read
        // from the shadow SharedPreferences in the test file.
        assertFalse(
                "Should be false when user overrides recommendation to off.",
                BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));

        // Case 5: No policies, user preference is on.

        // Write true to both the shadow SharedPref and mSettings (our PrefService mock).
        BookmarkBarUtils.setDevicePrefShowBookmarksBar(
                mProfile, /* enabled= */ true, /* fromKeyboardShortcut= */ false);

        // Resets configurations and applies default (all false).
        new BookmarkBarPolicyBuilder().build();

        // Falls back to the user's local choice stored in our shadow SharedPref.
        assertTrue(
                "Should be true when user pref is on and no policies exist.",
                BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));

        // Case 6: No policies, no user preference, default is on (from setUp).

        // Grabs the shadow SharedPref and wipes it clean, simulating the fresh install state where
        // the user has no preference saved.
        ContextUtils.getAppSharedPreferences().edit().clear().apply();

        new BookmarkBarPolicyBuilder().build();

        // We have the feature param set to true in setUp() in this test file.
        assertTrue(
                "Should be true from feature param when no policies or user pref exist.",
                BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));
    }

    @Test
    @SmallTest
    public void testToggleDevicePrefShowBookmarksBar() {
        mOverrideContextRule.setIsDesktop(false);
        new BookmarkBarPolicyBuilder().build(); // Restet, no policies active.

        // User should not have set any preference yet.
        assertFalse(BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar());

        //  Even though user has not set a device preference, should fallback to true because of the
        // FeatureParam in setUp().
        assertTrue(
                "Initial state should be true due to feature param.",
                BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));

        // First toggle: true -> false.
        BookmarkBarUtils.toggleDevicePrefShowBookmarksBar(
                mProfile, /* fromKeyboardShortcut= */ false);
        assertFalse(
                "Should be false after first toggle.",
                BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));

        assertTrue(
                "After the first toggle, the user should now have set pa reference.",
                BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar());

        // Second toggle: false -> true.
        BookmarkBarUtils.toggleDevicePrefShowBookmarksBar(
                mProfile, /* fromKeyboardShortcut= */ false);
        assertTrue(
                "Should be true after second toggle.",
                BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));
        assertTrue(
                "After the second toggle, the user should still have set preference.",
                BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar());
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
    @Config(qualifiers = PHONE_QUALIFIER)
    @DisableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
    public void testIsFeatureEnabledWhenFlagIsDisabledOnPhone() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity ->
                                assertFalse(
                                        BookmarkBarUtils.isDeviceBookmarkBarCompatible(activity)));
    }

    @Test
    @SmallTest
    @Config(qualifiers = PHONE_QUALIFIER)
    public void testIsFeatureEnabledWhenFlagIsEnabledOnPhone() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity ->
                                assertFalse(
                                        BookmarkBarUtils.isDeviceBookmarkBarCompatible(activity)));
    }

    @Test
    @SmallTest
    @Config(qualifiers = TABLET_QUALIFIER)
    @DisableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
    public void testIsFeatureEnabledWhenFlagIsDisabledOnTablet() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity ->
                                assertFalse(
                                        BookmarkBarUtils.isDeviceBookmarkBarCompatible(activity)));
    }

    @Test
    @SmallTest
    @Config(qualifiers = TABLET_QUALIFIER)
    public void testIsFeatureEnabledWhenFlagIsEnabledOnTablet() {
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
                            assertFalse(BookmarkBarUtils.isBookmarkBarVisible(activity, mProfile));

                            // Case: feature disallowed and setting enabled.
                            BookmarkBarUtils.setSettingEnabledForTesting(true);
                            assertFalse(BookmarkBarUtils.isBookmarkBarVisible(activity, mProfile));

                            // Case: feature allowed and setting disabled.
                            BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
                            BookmarkBarUtils.setSettingEnabledForTesting(false);
                            assertFalse(BookmarkBarUtils.isBookmarkBarVisible(activity, mProfile));

                            // Case feature allowed and setting enabled.
                            BookmarkBarUtils.setSettingEnabledForTesting(true);
                            assertTrue(BookmarkBarUtils.isBookmarkBarVisible(activity, mProfile));
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
                            assertFalse(BookmarkBarUtils.isBookmarkBarVisible(activity, mProfile));

                            // Case: feature allowed no device pref (FeatureParam = true).
                            BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
                            assertTrue(BookmarkBarUtils.isBookmarkBarVisible(activity, mProfile));

                            // Apply new FeatureParam override.
                            FeatureOverrides.Builder overrides = FeatureOverrides.newBuilder();
                            overrides =
                                    overrides.param(
                                            ChromeFeatureList.ANDROID_BOOKMARK_BAR,
                                            "show_bookmark_bar",
                                            false);
                            overrides.apply();

                            // Case: feature allowed no device pref (FeatureParam = false).
                            assertFalse(BookmarkBarUtils.isBookmarkBarVisible(activity, mProfile));

                            // Case: feature allowed explicit device pref
                            BookmarkBarUtils.setDevicePrefShowBookmarksBar(
                                    mProfile, true, /* fromKeyboardShortcut= */ false);
                            assertTrue(BookmarkBarUtils.isBookmarkBarVisible(activity, mProfile));
                        });
    }

    // Test UserPrefs - only on Desktop

    @Test
    @SmallTest
    public void testIsUserPrefsShowBookmarksBarEnabled() {
        mOverrideContextRule.setIsDesktop(true);

        mSetting.set(false);
        assertFalse(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));
        assertFalse(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(null));

        mSetting.set(true);
        assertTrue(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));
        assertFalse(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(null));
    }

    @Test
    @SmallTest
    public void testSetUserPrefsShowBookmarksBar() {
        mOverrideContextRule.setIsDesktop(true);

        mSetting.set(false);
        assertFalse(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                BookmarkBarUtils.TOGGLED_BY_KEYBOARD_SHORTCUT, true, 1)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_IN_SETTINGS)
                        .build();

        BookmarkBarUtils.setUserPrefsShowBookmarksBar(
                mProfile, true, /* fromKeyboardShortcut= */ true);
        assertTrue(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));

        histogramWatcher.assertExpected();

        var histogramWatcher2 =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(BookmarkBarUtils.TOGGLED_IN_SETTINGS, false, 1)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_BY_KEYBOARD_SHORTCUT)
                        .build();

        BookmarkBarUtils.setUserPrefsShowBookmarksBar(
                mProfile, false, /* fromKeyboardShortcut= */ false);
        assertFalse(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));

        histogramWatcher2.assertExpected();
    }

    @Test
    @SmallTest
    public void testToggleUserPrefsShowBookmarksBar() {
        mOverrideContextRule.setIsDesktop(true);

        mSetting.set(false);
        assertFalse(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));

        BookmarkBarUtils.toggleUserPrefsShowBookmarksBar(mProfile, true);
        assertTrue(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));

        BookmarkBarUtils.toggleUserPrefsShowBookmarksBar(mProfile, false);
        assertFalse(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(mProfile));
    }

    // Test device prefs - only on Tablet

    @Test
    @SmallTest
    public void testIsDevicePrefShowBookmarksBarEnabled() {
        mOverrideContextRule.setIsDesktop(false);

        // User should not have set any preference yet.
        assertFalse(BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar());

        // Even though user has not set a device preference, the FeatureParam will make it true.
        assertTrue(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));

        // Apply new FeatureParam override.
        FeatureOverrides.Builder overrides = FeatureOverrides.newBuilder();
        overrides =
                overrides.param(ChromeFeatureList.ANDROID_BOOKMARK_BAR, "show_bookmark_bar", false);
        overrides.apply();

        assertFalse(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));
    }

    @Test
    @SmallTest
    public void testSetDevicePrefShowBookmarksBar() {
        mOverrideContextRule.setIsDesktop(false);
        // User should not have set any preference yet.
        assertFalse(BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar());

        // Even though user has not set a device preference, the FeatureParam will make it true.
        assertTrue(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                BookmarkBarUtils.TOGGLED_BY_KEYBOARD_SHORTCUT, true, 1)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_IN_SETTINGS)
                        .build();

        BookmarkBarUtils.setDevicePrefShowBookmarksBar(
                mProfile, true, /* fromKeyboardShortcut= */ true);
        assertTrue(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));
        assertTrue(BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar());

        histogramWatcher.assertExpected();

        var histogramWatcher2 =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(BookmarkBarUtils.TOGGLED_IN_SETTINGS, false, 1)
                        .expectNoRecords(BookmarkBarUtils.TOGGLED_BY_KEYBOARD_SHORTCUT)
                        .build();

        BookmarkBarUtils.setDevicePrefShowBookmarksBar(
                mProfile, false, /* fromKeyboardShortcut= */ false);
        assertFalse(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(mProfile));
        assertTrue(BookmarkBarUtils.hasUserSetDevicePrefShowBookmarksBar());

        histogramWatcher2.assertExpected();
    }

    private @NonNull <T> Answer<Void> runCallbackWithValueAtIndex(
            @NonNull Callback<T> callback, int index) {
        return invocation -> {
            final T value = invocation.getArgument(index);
            callback.onResult(value);
            return null;
        };
    }
}
