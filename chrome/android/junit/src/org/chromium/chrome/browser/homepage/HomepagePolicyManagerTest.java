// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager.HomepagePolicyStateListener;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Tests for the {@link HomepagePolicyManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HomepagePolicyManagerTest {
    public static final String TEST_URL = JUnitTestGURLs.EXAMPLE_URL.getSpec();
    public static final String CHROME_NTP = JUnitTestGURLs.NTP_URL.getSpec();

    private HomepagePolicyManager mHomepagePolicyManager;

    @Mock private PrefService mMockPrefService;
    @Mock private PrefChangeRegistrar mMockRegistrar;
    @Mock private ChromeBrowserInitializer mChromeBrowserInitializer;

    @Mock private HomepagePolicyStateListener mListener;

    private SharedPreferencesManager mSharedPreferenceManager;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);

        // Reset shared preference
        mSharedPreferenceManager = ChromeSharedPreferences.getInstance();
        setHomepageInSharedPreference(GURL.emptyGURL());

        ChromeBrowserInitializer.setForTesting(mChromeBrowserInitializer);

        // Disable the policy during setup
        HomepagePolicyManager.setPrefServiceForTesting(mMockPrefService);
        setupNewHomepagePolicyManagerForTests(false, "", null);

        // Verify setup
        Assert.assertFalse(
                "#isHomepageManagedByPolicy == true without homepage pref setup",
                mHomepagePolicyManager.isHomepageLocationPolicyEnabled());
    }

    /**
     * Set up the homepage location for Mock PrefService, and create HomepagePolicyManager instance.
     *
     * @param homepageLocation homepage preference that will be returned by mock pref service
     */
    private void setupNewHomepagePolicyManagerForTests(
            boolean isPolicyEnabled,
            String homepageLocation,
            @Nullable HomepagePolicyStateListener listener) {
        Mockito.when(mMockPrefService.isManagedPreference(Pref.HOME_PAGE))
                .thenReturn(isPolicyEnabled);
        Mockito.when(mMockPrefService.getString(Pref.HOME_PAGE)).thenReturn(homepageLocation);

        mHomepagePolicyManager = new HomepagePolicyManager(mMockRegistrar, listener);
        HomepagePolicyManager.setInstanceForTests(mHomepagePolicyManager);
    }

    private void setHomepageInSharedPreference(GURL homepageLocation) {
        mSharedPreferenceManager.writeString(
                ChromePreferenceKeys.HOMEPAGE_LOCATION_POLICY_GURL, homepageLocation.serialize());
    }

    @Test
    @SmallTest
    public void testEmptyInstance_GetFromSharedPreference() {
        // Create a new empty instance
        HomepagePolicyManager manager = new HomepagePolicyManager();

        // Test if policy reflects the setting of shared preference
        Assert.assertFalse(
                "HomepagePolicyManager should be not initialized yet", manager.isInitialized());
        Assert.assertFalse(
                "#isHomepageManagedByPolicy not consistent with test setting",
                manager.isHomepageLocationPolicyEnabled());
    }

    @Test
    @SmallTest
    public void testEmptyInstance_EnabledFromSharedPreference() {
        setHomepageInSharedPreference(new GURL(CHROME_NTP));

        // Create a new empty instance
        HomepagePolicyManager manager = new HomepagePolicyManager();

        Assert.assertFalse(
                "HomepagePolicyManager should be not initialized yet", manager.isInitialized());
        Assert.assertTrue(
                "#isHomepageManagedByPolicy not consistent with test setting",
                manager.isHomepageLocationPolicyEnabled());
        Assert.assertEquals(
                "#getHomepageUrl not consistent with test setting",
                CHROME_NTP,
                manager.getHomepagePreference().getSpec());
    }

    @Test
    @SmallTest
    public void testEmptyInstance_InitializeAfterwards() {
        // Create a new empty instance
        HomepagePolicyManager manager = new HomepagePolicyManager();

        manager.initializeWithNative(mMockRegistrar);
        Assert.assertTrue("HomepagePolicyManager should be initialized", manager.isInitialized());
        Mockito.verify(mMockRegistrar, Mockito.times(1)).addObserver(Pref.HOME_PAGE, manager);
    }

    @Test
    @SmallTest
    public void testInitialization() {
        setupNewHomepagePolicyManagerForTests(true, TEST_URL, null);

        Assert.assertTrue(
                "#isHomepageManagedByPolicy not consistent with test setting",
                HomepagePolicyManager.isHomepageManagedByPolicy());
        Assert.assertEquals(
                "#getHomepageUrl not consistent with test setting",
                TEST_URL,
                HomepagePolicyManager.getHomepageUrl().getSpec());

        String homepageGurlSerialized =
                mSharedPreferenceManager.readString(
                        ChromePreferenceKeys.HOMEPAGE_LOCATION_POLICY_GURL, "");
        GURL homepageGurl = GURL.deserialize(homepageGurlSerialized);
        Assert.assertEquals(
                "Updated HomepageLocation should be stored in shared preference",
                TEST_URL,
                homepageGurl.getSpec());
    }

    @Test
    @SmallTest
    public void testInitialization_NTP() {
        setupNewHomepagePolicyManagerForTests(true, CHROME_NTP, null);

        Assert.assertTrue(
                "#isHomepageManagedByPolicy not consistent with test setting",
                HomepagePolicyManager.isHomepageManagedByPolicy());
        Assert.assertEquals(
                "#getHomepageUrl not consistent with test setting",
                CHROME_NTP,
                HomepagePolicyManager.getHomepageUrl().getSpec());

        String homepageGurlSerialized =
                mSharedPreferenceManager.readString(
                        ChromePreferenceKeys.HOMEPAGE_LOCATION_POLICY_GURL, "");
        GURL homepageGurl = GURL.deserialize(homepageGurlSerialized);
        Assert.assertEquals(
                "Updated HomepageLocation should be stored in shared preference",
                CHROME_NTP,
                homepageGurl.getSpec());
    }

    @Test
    @SmallTest
    public void testDestroy() {
        HomepagePolicyManager.destroy();
        Mockito.verify(mMockRegistrar).destroy();
        Assert.assertTrue(
                "Listeners are not remove completely",
                mHomepagePolicyManager.getListenersForTesting().isEmpty());
    }

    @Test
    @SmallTest
    public void testPrefRefreshToEnablePolicy() {
        Assert.assertFalse(
                "Policy should be disabled after set up",
                mHomepagePolicyManager.isHomepageLocationPolicyEnabled());

        // Add listener
        mHomepagePolicyManager.addListener(mListener);

        // A new policy URL is set, which triggers the refresh of native manager.
        final String newUrl = JUnitTestGURLs.URL_1.getSpec();
        Mockito.when(mMockPrefService.isManagedPreference(Pref.HOME_PAGE)).thenReturn(true);
        Mockito.when(mMockPrefService.getString(Pref.HOME_PAGE)).thenReturn(newUrl);

        // Update the preference, so that the policy will be enabled.
        mHomepagePolicyManager.onPreferenceChange();

        // The homepage retrieved from homepage manager should be in sync with pref setting
        Assert.assertTrue(
                "Policy should be enabled after refresh",
                mHomepagePolicyManager.isHomepageLocationPolicyEnabled());
        Assert.assertEquals(
                "#getHomepageUrl not consistent with test setting",
                newUrl,
                mHomepagePolicyManager.getHomepagePreference().getSpec());
        Mockito.verify(mListener, Mockito.times(1)).onHomepagePolicyUpdate();
    }

    @Test
    @SmallTest
    public void testPrefRefreshToDisablePolicy() {
        // Set a new HomepagePolicyManager with policy enabled.
        setupNewHomepagePolicyManagerForTests(true, TEST_URL, null);
        mHomepagePolicyManager.addListener(mListener);

        // The verify policyEnabled
        Assert.assertTrue(
                "Policy should be enabled after set up",
                mHomepagePolicyManager.isHomepageLocationPolicyEnabled());

        // Update the preference, so that the policy will be disabled.
        Mockito.when(mMockPrefService.isManagedPreference(Pref.HOME_PAGE)).thenReturn(false);
        Mockito.when(mMockPrefService.getString(Pref.HOME_PAGE)).thenReturn("");
        mHomepagePolicyManager.onPreferenceChange();

        // The homepage retrieved from homepage manager should be in sync with pref setting.
        Assert.assertFalse(
                "Policy should be disabled after refresh",
                mHomepagePolicyManager.isHomepageLocationPolicyEnabled());
        Mockito.verify(mListener, Mockito.times(1)).onHomepagePolicyUpdate();
    }

    @Test
    @SmallTest
    public void testPrefRefreshWithoutChanges() {
        // Set a new HomepagePolicyManager with policy enabled.
        setupNewHomepagePolicyManagerForTests(true, TEST_URL, null);

        // The verify policyEnabled
        Assert.assertTrue(
                "Policy should be enabled after set up",
                mHomepagePolicyManager.isHomepageLocationPolicyEnabled());

        // Perform an debounce - creating a new homepage manager with same setting, and add the
        // listener.
        setupNewHomepagePolicyManagerForTests(true, TEST_URL, mListener);

        // Verify listeners should not receive updates.
        Assert.assertTrue(
                "Policy should be enabled after refresh",
                mHomepagePolicyManager.isHomepageLocationPolicyEnabled());
        Assert.assertEquals(
                "#getHomepageUrl not consistent with test setting",
                TEST_URL,
                mHomepagePolicyManager.getHomepagePreference().getSpec());

        // However, because the native setting is consistent with cached value in SharedPreference,
        // listeners will not receive update.
        Mockito.verify(mListener, Mockito.never()).onHomepagePolicyUpdate();
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testIllegal_GetHomepageUrl() {
        setupNewHomepagePolicyManagerForTests(false, "", null);
        HomepagePolicyManager.getHomepageUrl();
    }

    @Test
    @SmallTest
    public void testGurlPreferenceKeysMigrationInConstructor() {
        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.DEPRECATED_HOMEPAGE_LOCATION_POLICY, null);
        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.HOMEPAGE_LOCATION_POLICY_GURL, null);
        mHomepagePolicyManager = new HomepagePolicyManager();
        Assert.assertFalse(mHomepagePolicyManager.isHomepageLocationPolicyEnabled());

        final String url1 = JUnitTestGURLs.URL_1.getSpec();
        final String url2 = JUnitTestGURLs.URL_2.getSpec();
        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.DEPRECATED_HOMEPAGE_LOCATION_POLICY, url1);
        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.HOMEPAGE_LOCATION_POLICY_GURL, null);
        mHomepagePolicyManager = new HomepagePolicyManager();
        Assert.assertEquals(url1, mHomepagePolicyManager.getHomepagePreference().getSpec());

        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.DEPRECATED_HOMEPAGE_LOCATION_POLICY, null);
        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.HOMEPAGE_LOCATION_POLICY_GURL,
                        new GURL(url1).serialize());
        mHomepagePolicyManager = new HomepagePolicyManager();
        Assert.assertEquals(url1, mHomepagePolicyManager.getHomepagePreference().getSpec());

        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.DEPRECATED_HOMEPAGE_LOCATION_POLICY, url1);
        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.HOMEPAGE_LOCATION_POLICY_GURL,
                        new GURL(url2).serialize());
        mHomepagePolicyManager = new HomepagePolicyManager();
        Assert.assertEquals(url2, mHomepagePolicyManager.getHomepagePreference().getSpec());
    }
}
