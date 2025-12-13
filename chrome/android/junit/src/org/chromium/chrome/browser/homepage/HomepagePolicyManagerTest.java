// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage;

import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager.HomepagePolicyStateListener;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils.BooleanPolicyState;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Tests for the {@link HomepagePolicyManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({
    ChromeFeatureList.SHOW_HOME_BUTTON_POLICY_ANDROID,
    ChromeFeatureList.HOMEPAGE_IS_NEW_TAB_PAGE_POLICY_ANDROID
})
public class HomepagePolicyManagerTest {
    public static final String TEST_URL = JUnitTestGURLs.EXAMPLE_URL.getSpec();
    public static final String CHROME_NTP = JUnitTestGURLs.NTP_URL.getSpec();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private HomepagePolicyManager mHomepagePolicyManager;

    @Mock private PrefService mMockPrefService;
    @Mock private PrefChangeRegistrar mMockRegistrar;
    @Mock private ChromeBrowserInitializer mChromeBrowserInitializer;

    @Mock private HomepagePolicyStateListener mListener;

    private SharedPreferencesManager mSharedPreferenceManager;

    private class PolicyBuilder {
        private boolean mIsHomeLocationPolicyManaged;
        private String mHomepageLocation = "";
        private boolean mIsButtonPolicyManaged;
        private boolean mButtonPolicyValue;
        private boolean mHomepageIsNtpPolicyManaged;
        private boolean mHomepageIsNtpPolicyValue;
        private boolean mHasButtonRecommendation;
        private boolean mIsFollowingButtonRecommendation;
        private boolean mHasNtpRecommendation;
        private boolean mIsFollowingNtpRecommendation;
        private boolean mHasLocationRecommendation;
        private boolean mIsFollowingLocationRecommendation;
        @Nullable private HomepagePolicyStateListener mListener;

        public PolicyBuilder withHomepageLocationPolicyManaged(boolean managed) {
            mIsHomeLocationPolicyManaged = managed;
            return this;
        }

        public PolicyBuilder withHomepageLocation(String location) {
            mHomepageLocation = location;
            return this;
        }

        public PolicyBuilder withButtonPolicyManaged(boolean managed) {
            mIsButtonPolicyManaged = managed;
            return this;
        }

        public PolicyBuilder withButtonPolicyValue(boolean value) {
            mButtonPolicyValue = value;
            return this;
        }

        public PolicyBuilder withHomepageIsNtpPolicyManaged(boolean managed) {
            mHomepageIsNtpPolicyManaged = managed;
            return this;
        }

        public PolicyBuilder withHomepageIsNtpPolicyValue(boolean value) {
            mHomepageIsNtpPolicyValue = value;
            return this;
        }

        public PolicyBuilder withHasButtonRecommendation(boolean recommended) {
            mHasButtonRecommendation = recommended;
            return this;
        }

        public PolicyBuilder withIsFollowingButtonRecommendation(boolean following) {
            mIsFollowingButtonRecommendation = following;
            return this;
        }

        public PolicyBuilder withHasNtpRecommendation(boolean recommended) {
            mHasNtpRecommendation = recommended;
            return this;
        }

        public PolicyBuilder withIsFollowingNtpRecommendation(boolean following) {
            mIsFollowingNtpRecommendation = following;
            return this;
        }

        public PolicyBuilder withHasLocationRecommendation(boolean recommended) {
            mHasLocationRecommendation = recommended;
            return this;
        }

        public PolicyBuilder withIsFollowingLocationRecommendation(boolean following) {
            mIsFollowingLocationRecommendation = following;
            return this;
        }

        public PolicyBuilder withListener(@Nullable HomepagePolicyStateListener listener) {
            mListener = listener;
            return this;
        }

        public void build() {
            Mockito.when(mMockPrefService.isManagedPreference(Pref.HOME_PAGE))
                    .thenReturn(mIsHomeLocationPolicyManaged);
            Mockito.when(mMockPrefService.getString(Pref.HOME_PAGE)).thenReturn(mHomepageLocation);

            Mockito.when(mMockPrefService.isManagedPreference(Pref.SHOW_HOME_BUTTON))
                    .thenReturn(mIsButtonPolicyManaged);
            Mockito.when(mMockPrefService.getBoolean(Pref.SHOW_HOME_BUTTON))
                    .thenReturn(mButtonPolicyValue);

            Mockito.when(mMockPrefService.isManagedPreference(Pref.HOME_PAGE_IS_NEW_TAB_PAGE))
                    .thenReturn(mHomepageIsNtpPolicyManaged);
            Mockito.when(mMockPrefService.getBoolean(Pref.HOME_PAGE_IS_NEW_TAB_PAGE))
                    .thenReturn(mHomepageIsNtpPolicyValue);

            Mockito.when(mMockPrefService.hasRecommendation(Pref.SHOW_HOME_BUTTON))
                    .thenReturn(mHasButtonRecommendation);
            Mockito.when(mMockPrefService.isFollowingRecommendation(Pref.SHOW_HOME_BUTTON))
                    .thenReturn(mIsFollowingButtonRecommendation);

            Mockito.when(mMockPrefService.hasRecommendation(Pref.HOME_PAGE_IS_NEW_TAB_PAGE))
                    .thenReturn(mHasNtpRecommendation);
            Mockito.when(mMockPrefService.isFollowingRecommendation(Pref.HOME_PAGE_IS_NEW_TAB_PAGE))
                    .thenReturn(mIsFollowingNtpRecommendation);

            Mockito.when(mMockPrefService.hasRecommendation(Pref.HOME_PAGE))
                    .thenReturn(mHasLocationRecommendation);
            Mockito.when(mMockPrefService.isFollowingRecommendation(Pref.HOME_PAGE))
                    .thenReturn(mIsFollowingLocationRecommendation);
        }
    }

    @Before
    public void setup() {
        // Reset shared preferences and mocks before each test.
        mSharedPreferenceManager = ChromeSharedPreferences.getInstance();

        ChromeBrowserInitializer.setForTesting(mChromeBrowserInitializer);
        HomepagePolicyManager.setPrefServiceForTesting(mMockPrefService);
    }

    @Test
    public void testInit_DefaultStateWithEmptyPrefs() {
        mHomepagePolicyManager = new HomepagePolicyManager();
        Assert.assertFalse(
                "Manager should not be initialized", mHomepagePolicyManager.isInitialized());
        Assert.assertFalse(
                "Homepage location should not be managed",
                mHomepagePolicyManager.isHomepageLocationPolicyManaged());
        Assert.assertFalse(
                "Show home button should not be managed",
                mHomepagePolicyManager.isShowHomeButtonPolicyManaged());
        Assert.assertFalse(
                "Homepage selection should not be managed",
                mHomepagePolicyManager.isHomepageIsNtpPolicyManaged());
        Assert.assertFalse(
                "Show home button should not be recommended",
                mHomepagePolicyManager.isShowHomeButtonPolicyRecommended());
        Assert.assertFalse(
                "Homepage selection should not be recommended",
                mHomepagePolicyManager.isHomepageSelectionPolicyRecommended());
    }

    @Test
    public void testInit_LoadsLocationFromSharedPrefs() {
        mSharedPreferenceManager.writeString(
                ChromePreferenceKeys.HOMEPAGE_LOCATION_POLICY_GURL,
                new GURL(CHROME_NTP).serialize());
        mHomepagePolicyManager = new HomepagePolicyManager();
        Assert.assertTrue(
                "Homepage location should be managed",
                mHomepagePolicyManager.isHomepageLocationPolicyManaged());
        Assert.assertEquals(
                "Homepage URL should match value from SharedPreferences",
                CHROME_NTP,
                mHomepagePolicyManager.getHomepageLocationPolicyUrl().getSpec());
    }

    @Test
    public void testInit_LoadsShowHomeButtonStatesFromSharedPrefs() {
        mSharedPreferenceManager.writeInt(
                ChromePreferenceKeys.SHOW_HOME_BUTTON_POLICY_STATE,
                BooleanPolicyState.MANAGED_BY_POLICY_ON);
        mHomepagePolicyManager = new HomepagePolicyManager();
        Assert.assertTrue(
                "Button should be managed for MANAGED_BY_POLICY_ON",
                mHomepagePolicyManager.isShowHomeButtonPolicyManaged());
        Assert.assertTrue(
                "Button value should be true for MANAGED_BY_POLICY_ON",
                mHomepagePolicyManager.getShowHomeButtonPolicyValue());

        mSharedPreferenceManager.writeInt(
                ChromePreferenceKeys.SHOW_HOME_BUTTON_POLICY_STATE,
                BooleanPolicyState.RECOMMENDED_IS_NOT_FOLLOWED);
        mHomepagePolicyManager = new HomepagePolicyManager();
        Assert.assertFalse(
                "Button should not be managed for RECOMMENDED_IS_NOT_FOLLOWED",
                mHomepagePolicyManager.isShowHomeButtonPolicyManaged());
        Assert.assertTrue(
                "Button should be recommended for RECOMMENDED_IS_NOT_FOLLOWED",
                mHomepagePolicyManager.isShowHomeButtonPolicyRecommended());
        Assert.assertFalse(
                "User should not be following recommendation for RECOMMENDED_IS_NOT_FOLLOWED",
                mHomepagePolicyManager.isFollowingHomepageButtonPolicyRecommendation());
    }

    @Test
    public void testInit_LoadsHomepageSelectionStateFromSharedPrefs() {
        mSharedPreferenceManager.writeInt(
                ChromePreferenceKeys.HOMEPAGE_SELECTION_POLICY_STATE,
                BooleanPolicyState.MANAGED_BY_POLICY_OFF);
        mHomepagePolicyManager = new HomepagePolicyManager();
        Assert.assertTrue(
                "Selection should be managed for MANAGED_BY_POLICY_OFF",
                mHomepagePolicyManager.isHomepageIsNtpPolicyManaged());
        Assert.assertFalse(
                "Selection value should be false for MANAGED_BY_POLICY_OFF",
                mHomepagePolicyManager.getHomepageIsNtpPolicyValue());

        mSharedPreferenceManager.writeInt(
                ChromePreferenceKeys.HOMEPAGE_SELECTION_POLICY_STATE,
                BooleanPolicyState.RECOMMENDED_IS_FOLLOWED);
        mHomepagePolicyManager = new HomepagePolicyManager();
        Assert.assertFalse(
                "Selection should not be managed for RECOMMENDED_IS_FOLLOWED",
                mHomepagePolicyManager.isHomepageIsNtpPolicyManaged());
        Assert.assertTrue(
                "Selection should be recommended for RECOMMENDED_IS_FOLLOWED",
                mHomepagePolicyManager.isHomepageSelectionPolicyRecommended());
        Assert.assertTrue(
                "User should be following recommendation for RECOMMENDED_IS_FOLLOWED",
                mHomepagePolicyManager.isFollowingHomepageSelectionPolicyRecommendation());
    }

    @Test
    public void testInit_HandlesDeprecatedLocationKey() {
        mSharedPreferenceManager.writeString(
                ChromePreferenceKeys.DEPRECATED_HOMEPAGE_LOCATION_POLICY, TEST_URL);
        mHomepagePolicyManager = new HomepagePolicyManager();
        Assert.assertTrue(
                "Homepage location should be managed via deprecated key",
                mHomepagePolicyManager.isHomepageLocationPolicyManaged());
        Assert.assertEquals(
                "Homepage URL should match deprecated key",
                TEST_URL,
                mHomepagePolicyManager.getHomepageLocationPolicyUrl().getSpec());
    }

    @Test
    public void testInit_NewLocationKeyTakesPrecedence() {
        mSharedPreferenceManager.writeString(
                ChromePreferenceKeys.DEPRECATED_HOMEPAGE_LOCATION_POLICY, TEST_URL);
        mSharedPreferenceManager.writeString(
                ChromePreferenceKeys.HOMEPAGE_LOCATION_POLICY_GURL,
                new GURL(CHROME_NTP).serialize());
        mHomepagePolicyManager = new HomepagePolicyManager();
        Assert.assertEquals(
                "New GURL key should take precedence",
                CHROME_NTP,
                mHomepagePolicyManager.getHomepageLocationPolicyUrl().getSpec());
    }

    @Test
    public void testNativeLifecycle_InitializationAndDestruction() {
        mHomepagePolicyManager = new HomepagePolicyManager();
        HomepagePolicyManager.setInstanceForTests(mHomepagePolicyManager);

        // Test native initialization
        mHomepagePolicyManager.initializeWithNative(mMockRegistrar);
        Assert.assertTrue("Manager should be initialized", mHomepagePolicyManager.isInitialized());
        Mockito.verify(mMockRegistrar, Mockito.times(1))
                .addObserver(Pref.HOME_PAGE, mHomepagePolicyManager);
        Mockito.verify(mMockRegistrar, Mockito.times(1))
                .addObserver(Pref.SHOW_HOME_BUTTON, mHomepagePolicyManager);
        Mockito.verify(mMockRegistrar, Mockito.times(1))
                .addObserver(Pref.HOME_PAGE_IS_NEW_TAB_PAGE, mHomepagePolicyManager);

        // Test destruction
        HomepagePolicyManager.destroy();
        Mockito.verify(mMockRegistrar).destroy();
        Assert.assertTrue(
                "Listeners should be cleared on destroy",
                mHomepagePolicyManager.getListenersForTesting().isEmpty());
    }

    @Test
    public void testStateUpdate_EnablingAndDisablingPolicies() {
        new PolicyBuilder().withListener(mListener).build();
        mHomepagePolicyManager = new HomepagePolicyManager(mMockRegistrar, mListener);
        HomepagePolicyManager.setInstanceForTests(mHomepagePolicyManager);

        // Enable HomepageLocation policy.
        Mockito.when(mMockPrefService.isManagedPreference(Pref.HOME_PAGE)).thenReturn(true);
        Mockito.when(mMockPrefService.getString(Pref.HOME_PAGE)).thenReturn(TEST_URL);
        mHomepagePolicyManager.onPreferenceChange();
        Assert.assertTrue(
                "Homepage location should be managed after enabling the policy.",
                mHomepagePolicyManager.isHomepageLocationPolicyManaged());
        Mockito.verify(mListener, Mockito.times(1)).onHomepagePolicyUpdate();

        // Enable ShowHomeButton policy.
        Mockito.when(mMockPrefService.isManagedPreference(Pref.SHOW_HOME_BUTTON)).thenReturn(true);
        Mockito.when(mMockPrefService.getBoolean(Pref.SHOW_HOME_BUTTON)).thenReturn(true);
        mHomepagePolicyManager.onPreferenceChange();
        Assert.assertTrue(
                "Show home button should be managed after enabling the policy.",
                mHomepagePolicyManager.isShowHomeButtonPolicyManaged());
        Mockito.verify(mListener, Mockito.times(2)).onHomepagePolicyUpdate();

        // Enable HomepageIsNewTabPage policy.
        Mockito.when(mMockPrefService.isManagedPreference(Pref.HOME_PAGE_IS_NEW_TAB_PAGE))
                .thenReturn(true);
        Mockito.when(mMockPrefService.getBoolean(Pref.HOME_PAGE_IS_NEW_TAB_PAGE)).thenReturn(true);
        mHomepagePolicyManager.onPreferenceChange();
        Assert.assertTrue(
                "Homepage selection should be managed after enabling the policy.",
                mHomepagePolicyManager.isHomepageIsNtpPolicyManaged());
        Mockito.verify(mListener, Mockito.times(3)).onHomepagePolicyUpdate();

        // Disable HomepageLocation policy.
        Mockito.when(mMockPrefService.isManagedPreference(Pref.HOME_PAGE)).thenReturn(false);
        Mockito.when(mMockPrefService.getString(Pref.HOME_PAGE)).thenReturn("");
        mHomepagePolicyManager.onPreferenceChange();
        Assert.assertFalse(
                "Homepage location should not be managed after disabling the policy.",
                mHomepagePolicyManager.isHomepageLocationPolicyManaged());
        Mockito.verify(mListener, Mockito.times(4)).onHomepagePolicyUpdate();

        // Disable ShowHomeButton policy.
        Mockito.when(mMockPrefService.isManagedPreference(Pref.SHOW_HOME_BUTTON)).thenReturn(false);
        mHomepagePolicyManager.onPreferenceChange();
        Assert.assertFalse(
                "Show home button should not be managed after disabling the policy.",
                mHomepagePolicyManager.isShowHomeButtonPolicyManaged());
        Mockito.verify(mListener, Mockito.times(5)).onHomepagePolicyUpdate();

        // Disable HomepageIsNewTabPage policy.
        Mockito.when(mMockPrefService.isManagedPreference(Pref.HOME_PAGE_IS_NEW_TAB_PAGE))
                .thenReturn(false);
        mHomepagePolicyManager.onPreferenceChange();
        Assert.assertFalse(
                "Homepage selection should not be managed after disabling the policy.",
                mHomepagePolicyManager.isHomepageIsNtpPolicyManaged());
        Mockito.verify(mListener, Mockito.times(6)).onHomepagePolicyUpdate();
    }

    @Test
    public void testRefresh_NoChangeInPolicyDoesNotNotifyListeners() {
        new PolicyBuilder()
                .withHomepageLocationPolicyManaged(true)
                .withHomepageLocation(TEST_URL)
                .withButtonPolicyManaged(true)
                .withButtonPolicyValue(true)
                .withHomepageIsNtpPolicyManaged(true)
                .withHomepageIsNtpPolicyValue(true)
                .withListener(mListener)
                .build();
        mHomepagePolicyManager = new HomepagePolicyManager(mMockRegistrar, mListener);
        HomepagePolicyManager.setInstanceForTests(mHomepagePolicyManager);
        Mockito.verify(mListener).onHomepagePolicyUpdate();

        // Reset the mock to forget the initial invocation.
        Mockito.reset(mListener);

        // A second refresh with identical values should not trigger the listener.
        mHomepagePolicyManager.onPreferenceChange();
        Mockito.verify(mListener, Mockito.never()).onHomepagePolicyUpdate();
    }

    @Test
    public void testHomepageSelection_ManagedByNtpPolicy() {
        new PolicyBuilder()
                .withHomepageIsNtpPolicyManaged(true)
                .withHomepageIsNtpPolicyValue(true)
                .build();
        mHomepagePolicyManager = new HomepagePolicyManager(mMockRegistrar, mListener);
        HomepagePolicyManager.setInstanceForTests(mHomepagePolicyManager);

        Assert.assertTrue(
                "Selection should be managed",
                mHomepagePolicyManager.isHomepageIsNtpPolicyManaged());
        Assert.assertTrue(
                "Selection value should be true",
                mHomepagePolicyManager.getHomepageIsNtpPolicyValue());

        Mockito.when(mMockPrefService.getBoolean(Pref.HOME_PAGE_IS_NEW_TAB_PAGE)).thenReturn(false);
        mHomepagePolicyManager.onPreferenceChange();

        Assert.assertTrue(
                "Selection should still be managed",
                mHomepagePolicyManager.isHomepageIsNtpPolicyManaged());
        Assert.assertFalse(
                "Selection value should now be false",
                mHomepagePolicyManager.getHomepageIsNtpPolicyValue());
    }

    @Test
    public void testRefresh_UpdatesShowHomeButtonStateWhenRecommendationChanges() {
        // Homepage disabled for client
        mSharedPreferenceManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, false);

        // Mock that the preference is controlled by a recommendation.
        Mockito.when(mMockPrefService.isRecommendedPreference(Pref.SHOW_HOME_BUTTON))
                .thenReturn(true);

        // Simulate admin recommending the button to be ON, with the user NOT following.
        new PolicyBuilder()
                .withButtonPolicyValue(true)
                .withHasButtonRecommendation(true)
                .withIsFollowingButtonRecommendation(false)
                .build();
        mHomepagePolicyManager = new HomepagePolicyManager(mMockRegistrar, mListener);
        HomepagePolicyManager.setInstanceForTests(mHomepagePolicyManager);

        // Assert: The user's setting should be updated to match the recommendation.
        Assert.assertTrue(
                "HOMEPAGE_ENABLED should be true when recommendation is true",
                mSharedPreferenceManager.readBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, false));

        // Now, simulate the user is following the recommendation.
        Mockito.when(mMockPrefService.isFollowingRecommendation(Pref.SHOW_HOME_BUTTON))
                .thenReturn(true);
        mHomepagePolicyManager.onPreferenceChange();
        Assert.assertTrue(
                "HOMEPAGE_ENABLED should remain true when already following recommendation",
                mSharedPreferenceManager.readBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, false));

        // Simulate admin changing the recommendation to be OFF. The user is now not following.
        Mockito.when(mMockPrefService.getBoolean(Pref.SHOW_HOME_BUTTON)).thenReturn(false);
        Mockito.when(mMockPrefService.isFollowingRecommendation(Pref.SHOW_HOME_BUTTON))
                .thenReturn(false);

        // Act: Trigger the refresh logic again.
        mHomepagePolicyManager.onPreferenceChange();

        // Assert: The user's setting should be automatically updated to 'false'.
        Assert.assertFalse(
                "HOMEPAGE_ENABLED should be false after recommendation changes to false",
                mSharedPreferenceManager.readBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true));
    }

    @Test
    public void testRefresh_UpdatesHomepageSelectionWhenRecommendationChanges() {
        // Arrange: Start with user setting for NTP, but admin recommends a custom URL.
        mSharedPreferenceManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, true);
        Mockito.when(mMockPrefService.isRecommendedPreference(Pref.HOME_PAGE)).thenReturn(true);
        Mockito.when(mMockPrefService.isRecommendedPreference(Pref.HOME_PAGE_IS_NEW_TAB_PAGE))
                .thenReturn(false);

        new PolicyBuilder()
                .withHomepageLocation(TEST_URL)
                .withHasLocationRecommendation(true)
                .withIsFollowingLocationRecommendation(false)
                .build();
        mHomepagePolicyManager = new HomepagePolicyManager(mMockRegistrar, mListener);
        HomepagePolicyManager.setInstanceForTests(mHomepagePolicyManager);

        // Assert: User settings should update to the recommended custom URL.
        // NOTE: This tests logic where policy recommendations automatically update user
        // preferences.
        Assert.assertFalse(
                "HOMEPAGE_USE_CHROME_NTP should be false to follow URL recommendation.",
                mSharedPreferenceManager.readBoolean(
                        ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, true));
        Assert.assertEquals(
                "HOMEPAGE_CUSTOM_GURL should be updated to the recommended URL.",
                new GURL(TEST_URL).serialize(),
                mSharedPreferenceManager.readString(
                        ChromePreferenceKeys.HOMEPAGE_CUSTOM_GURL, null));

        // Arrange: Admin now changes recommendation to NTP.
        Mockito.when(mMockPrefService.isRecommendedPreference(Pref.HOME_PAGE)).thenReturn(false);
        Mockito.when(mMockPrefService.hasRecommendation(Pref.HOME_PAGE)).thenReturn(false);
        Mockito.when(mMockPrefService.isRecommendedPreference(Pref.HOME_PAGE_IS_NEW_TAB_PAGE))
                .thenReturn(true);
        Mockito.when(mMockPrefService.getBoolean(Pref.HOME_PAGE_IS_NEW_TAB_PAGE)).thenReturn(true);
        Mockito.when(mMockPrefService.hasRecommendation(Pref.HOME_PAGE_IS_NEW_TAB_PAGE))
                .thenReturn(true);
        Mockito.when(mMockPrefService.isFollowingRecommendation(Pref.HOME_PAGE_IS_NEW_TAB_PAGE))
                .thenReturn(false);

        // Act: Refresh policies.
        mHomepagePolicyManager.onPreferenceChange();

        // Assert: User settings should update to the new NTP recommendation.
        Assert.assertTrue(
                "HOMEPAGE_USE_CHROME_NTP should now be true to follow NTP recommendation.",
                mSharedPreferenceManager.readBoolean(
                        ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, false));
    }

    @Test
    public void testHomepageSelection_InferredFromLocationPolicy() {
        new PolicyBuilder()
                .withHomepageLocationPolicyManaged(true)
                .withHomepageLocation(CHROME_NTP)
                .build();
        mHomepagePolicyManager = new HomepagePolicyManager(mMockRegistrar, mListener);
        HomepagePolicyManager.setInstanceForTests(mHomepagePolicyManager);

        Assert.assertTrue(
                "Selection should be inferred as managed when location is NTP.",
                mHomepagePolicyManager.isHomepageIsNtpPolicyManaged());
        Assert.assertTrue(
                "Selection value should be true when location is NTP.",
                mHomepagePolicyManager.getHomepageIsNtpPolicyValue());

        Mockito.when(mMockPrefService.getString(Pref.HOME_PAGE)).thenReturn(TEST_URL);
        mHomepagePolicyManager.onPreferenceChange();

        Assert.assertTrue(
                "Selection should remain inferred as managed when location changes to a custom"
                        + " URL.",
                mHomepagePolicyManager.isHomepageIsNtpPolicyManaged());
        Assert.assertFalse(
                "Selection value should be false when location is a custom URL.",
                mHomepagePolicyManager.getHomepageIsNtpPolicyValue());
    }

    @Test
    public void testHomepageSelection_Recommended() {
        new PolicyBuilder()
                .withHasNtpRecommendation(true)
                .withIsFollowingNtpRecommendation(true)
                .build();
        mHomepagePolicyManager = new HomepagePolicyManager(mMockRegistrar, mListener);
        HomepagePolicyManager.setInstanceForTests(mHomepagePolicyManager);

        Assert.assertTrue(
                "Policy should be marked as recommended.",
                mHomepagePolicyManager.isHomepageSelectionPolicyRecommended());
        Assert.assertTrue(
                "User should be marked as following the recommendation.",
                mHomepagePolicyManager.isFollowingHomepageSelectionPolicyRecommendation());

        Mockito.when(mMockPrefService.isFollowingRecommendation(Pref.HOME_PAGE_IS_NEW_TAB_PAGE))
                .thenReturn(false);
        mHomepagePolicyManager.onPreferenceChange();

        Assert.assertTrue(
                "Policy should still be marked as recommended even when not followed.",
                mHomepagePolicyManager.isHomepageSelectionPolicyRecommended());
        Assert.assertFalse(
                "User should now be marked as not following the recommendation.",
                mHomepagePolicyManager.isFollowingHomepageSelectionPolicyRecommendation());
    }

    @Test
    public void testHomepageSelection_CombinedRecommendations_NotFollowed() {
        new PolicyBuilder()
                .withHasNtpRecommendation(true)
                .withIsFollowingNtpRecommendation(true)
                .withHasLocationRecommendation(true)
                .build();
        mHomepagePolicyManager = new HomepagePolicyManager(mMockRegistrar, mListener);
        HomepagePolicyManager.setInstanceForTests(mHomepagePolicyManager);
        Assert.assertTrue(
                "Policy should be recommended",
                mHomepagePolicyManager.isHomepageSelectionPolicyRecommended());
        Assert.assertFalse(
                "If either recommendation is not followed, the state should be not followed.",
                mHomepagePolicyManager.isFollowingHomepageSelectionPolicyRecommendation());
    }

    @Test
    public void testHomepageSelection_CombinedRecommendations_Followed() {
        new PolicyBuilder()
                .withHasNtpRecommendation(true)
                .withIsFollowingNtpRecommendation(true)
                .withHasLocationRecommendation(true)
                .withIsFollowingLocationRecommendation(true)
                .build();
        mHomepagePolicyManager = new HomepagePolicyManager(mMockRegistrar, mListener);
        HomepagePolicyManager.setInstanceForTests(mHomepagePolicyManager);
        Assert.assertTrue(
                "Policy should be recommended",
                mHomepagePolicyManager.isHomepageSelectionPolicyRecommended());
        Assert.assertTrue(
                "If all recommendations are followed, the state should be followed.",
                mHomepagePolicyManager.isFollowingHomepageSelectionPolicyRecommendation());
    }

    @Test(expected = AssertionError.class)
    public void testGetHomepageUrl_ThrowsWhenNotManaged() {
        new PolicyBuilder().build();
        mHomepagePolicyManager = new HomepagePolicyManager(mMockRegistrar, mListener);
        HomepagePolicyManager.setInstanceForTests(mHomepagePolicyManager);
        mHomepagePolicyManager.getHomepageLocationPolicyUrl(); // Should throw
    }
}
