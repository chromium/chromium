// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;

import android.content.Intent;

import androidx.test.core.app.ActivityScenario;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactoryJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.password_manager.LoginDbDeprecationUtilBridgeJni;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileManagerUtils;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactoryJni;
import org.chromium.chrome.browser.settings.SettingsActivityUnitTest.ShadowProfileManagerUtils;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridgeJni;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.components.variations.VariationsAssociatedDataJni;

import java.io.File;
import java.io.IOException;

@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 30, shadows = ShadowProfileManagerUtils.class)
public class MultiColumnSettingsTest {

    /** Shadow class to bypass the real call to ProfileManagerUtils. */
    @Implements(ProfileManagerUtils.class)
    public static class ShadowProfileManagerUtils {
        @Implementation
        protected static void flushPersistentDataForAllProfiles() {}
    }

    private ActivityScenario<SettingsActivity> mActivityScenario;
    private SettingsActivity mSettingsActivity;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock public ChromeBrowserInitializer mInitializer;
    @Mock public Profile mProfile;
    @Mock private UserPrefsJni mUserPrefsJni;
    @Mock private PrefService mPrefs;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private SigninManager mSigninManager;
    @Mock private SyncService mSyncService;
    @Mock private TemplateUrlServiceFactoryJni mTemplateUrlFactoryJni;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private UnifiedConsentServiceBridgeJni mUnifiedConsentServiceBridgeJni;
    @Mock private VariationsAssociatedDataJni mVariationsAssociatedDataJni;
    @Mock private ShoppingServiceFactoryJni mShoppingServiceFactoryJni;
    @Mock private PasswordManagerUtilBridgeJni mPasswordManagerUtilBridgeJni;
    @Mock private LoginDbDeprecationUtilBridgeJni mLoginDbDeprecationUtilBridgeJni;

    @Before
    public void setup() {
        ChromeBrowserInitializer.setForTesting(mInitializer);
        ProfileManager.setLastUsedProfileForTesting(mProfile);

        doReturn(mIdentityManager).when(mIdentityServicesProvider).getIdentityManager(any());
        doReturn(mSigninManager).when(mIdentityServicesProvider).getSigninManager(any());
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);

        doReturn(mPrefs).when(mUserPrefsJni).get(mProfile);
        doReturn("false").when(mPrefs).getString(Pref.CONTEXTUAL_SEARCH_ENABLED);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);

        SyncServiceFactory.setInstanceForTesting(mSyncService);

        doReturn(mTemplateUrlService).when(mTemplateUrlFactoryJni).getTemplateUrlService(any());
        TemplateUrlServiceFactoryJni.setInstanceForTesting(mTemplateUrlFactoryJni);

        UnifiedConsentServiceBridgeJni.setInstanceForTesting(mUnifiedConsentServiceBridgeJni);
        VariationsAssociatedDataJni.setInstanceForTesting(mVariationsAssociatedDataJni);

        ShoppingServiceFactoryJni.setInstanceForTesting(mShoppingServiceFactoryJni);
        PasswordManagerUtilBridgeJni.setInstanceForTesting(mPasswordManagerUtilBridgeJni);
        LoginDbDeprecationUtilBridgeJni.setInstanceForTesting(mLoginDbDeprecationUtilBridgeJni);

        try {
            File tempFile = File.createTempFile("passwords", "csv");
            tempFile.deleteOnExit();
            doReturn(tempFile.getAbsolutePath())
                    .when(mLoginDbDeprecationUtilBridgeJni)
                    .getAutoExportCsvFilePath(mProfile);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    @After
    public void tearDown() {
        if (mActivityScenario != null) {
            mActivityScenario.close();
            mActivityScenario = null;
        }
    }

    @Test
    @Config(qualifiers = "w400dp-h600dp")
    @EnableFeatures({
        ChromeFeatureList.SETTINGS_MULTI_COLUMN,
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION
    })
    @DisableFeatures({
        ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2,
        ChromeFeatureList.PLUS_ADDRESSES_ENABLED
    })
    public void testSinglePane() {
        // Start the main settings, which is an embeddable fragment.
        startSettings(MainSettings.class.getName());
        MultiColumnSettings settings = mSettingsActivity.getMultiColumnSettings();
        assertTrue(settings.getSlidingPaneLayout().isSlideable());
    }

    @Test
    @Config(qualifiers = "w840dp-h60dp")
    @EnableFeatures({
        ChromeFeatureList.SETTINGS_MULTI_COLUMN,
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION
    })
    @DisableFeatures({
        ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2,
        ChromeFeatureList.PLUS_ADDRESSES_ENABLED
    })
    public void testTwoPane() {
        // Start the main settings, which is an embeddable fragment.
        startSettings(MainSettings.class.getName());
        MultiColumnSettings settings = mSettingsActivity.getMultiColumnSettings();
        assertFalse(settings.getSlidingPaneLayout().isSlideable());
    }

    private void startSettings(String fragmentName) {
        assert mActivityScenario == null : "Should be called once per test.";
        Intent intent =
                SettingsIntentUtil.createIntent(
                        ContextUtils.getApplicationContext(), fragmentName, null);
        mActivityScenario = ActivityScenario.launch(intent);
        mActivityScenario.onActivity(activity -> mSettingsActivity = activity);
    }
}
