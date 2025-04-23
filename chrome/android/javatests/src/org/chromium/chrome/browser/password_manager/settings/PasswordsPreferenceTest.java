// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.transit.TransitAsserts;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.LoginDbDeprecationUtilBridge;
import org.chromium.chrome.browser.password_manager.LoginDbDeprecationUtilBridgeJni;
import org.chromium.chrome.browser.password_manager.PasswordManagerTestHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.settings.PreferenceFacility;
import org.chromium.chrome.test.transit.settings.SettingsActivityPublicTransitEntryPoints;
import org.chromium.chrome.test.transit.settings.SettingsStation;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.File;
import java.io.IOException;

/**
 * Public Transit tests for the passwords preference item. Tests checking the subtitle for the
 * access loss warning can be found in {@link PasswordsPreferenceAccessLossTest}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(
        reason =
                "The tests can't be batched because the functionality under test is set up during"
                        + " Chrome start up.")
@EnableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
public class PasswordsPreferenceTest {
    @ClassRule
    public static SettingsActivityTestRule<MainSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(MainSettings.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE_SETTINGS)
                    .setRevision(1)
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock PrefService mPrefService;

    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;
    @Mock private LoginDbDeprecationUtilBridge.Natives mLoginDbDeprecationUtilBridgeJniMock;

    SettingsActivityPublicTransitEntryPoints mEntryPoints =
            new SettingsActivityPublicTransitEntryPoints(mSettingsActivityTestRule);

    @Before
    public void setUp() {
        PasswordManagerUtilBridgeJni.setInstanceForTesting(mPasswordManagerUtilBridgeJniMock);
        LoginDbDeprecationUtilBridgeJni.setInstanceForTesting(mLoginDbDeprecationUtilBridgeJniMock);
        PasswordManagerTestHelper.setUpGmsCoreFakeBackends();

        PasswordsPreference.setPrefServiceForTesting(mPrefService);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPwmStoppedWorkingSubtitle() throws IOException {
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(any(), eq(true)))
                .thenReturn(false);
        when(mPrefService.getBoolean(Pref.UPM_UNMIGRATED_PASSWORDS_EXPORTED)).thenReturn(true);
        when(mLoginDbDeprecationUtilBridgeJniMock.getAutoExportCsvFilePath(any()))
                .thenReturn("random/file/path");

        SettingsStation<MainSettings> page = mEntryPoints.startMainSettingsNonBatched();
        PreferenceFacility passwordsPref = page.scrollToPref(MainSettings.PREF_PASSWORDS);

        mRenderTestRule.render(
                passwordsPref.prefViewElement.get(), "passwords_preference_gpm_stopped_working");
        TransitAsserts.assertFinalDestination(page);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void testSomePasswordsNotAccessibleSubtitle() throws IOException {
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(any(), eq(true)))
                .thenReturn(true);
        when(mPrefService.getBoolean(Pref.UPM_UNMIGRATED_PASSWORDS_EXPORTED)).thenReturn(true);
        File fakeCsv = File.createTempFile("passwords", null, null);
        fakeCsv.deleteOnExit();
        when(mLoginDbDeprecationUtilBridgeJniMock.getAutoExportCsvFilePath(any()))
                .thenReturn(fakeCsv.getAbsolutePath());

        SettingsStation<MainSettings> page = mEntryPoints.startMainSettingsNonBatched();
        PreferenceFacility passwordsPref = page.scrollToPref(MainSettings.PREF_PASSWORDS);

        mRenderTestRule.render(
                passwordsPref.prefViewElement.get(), "passwords_preference_pwds_not_accessible");
        TransitAsserts.assertFinalDestination(page);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_AUTO})
    public void testSomePasswordsNotAccessibleSubtitleNotDisplayedOnAuto() throws IOException {
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(any(), eq(true)))
                .thenReturn(true);
        when(mPrefService.getBoolean(Pref.UPM_UNMIGRATED_PASSWORDS_EXPORTED)).thenReturn(true);
        File fakeCsv = File.createTempFile("passwords", null, null);
        fakeCsv.deleteOnExit();
        when(mLoginDbDeprecationUtilBridgeJniMock.getAutoExportCsvFilePath(any()))
                .thenReturn(fakeCsv.getAbsolutePath());

        SettingsStation<MainSettings> page = mEntryPoints.startMainSettingsNonBatched();
        PreferenceFacility passwordsPref = page.scrollToPref(MainSettings.PREF_PASSWORDS);

        mRenderTestRule.render(
                passwordsPref.prefViewElement.get(),
                "passwords_preference_pwds_not_accessible_auto");
        TransitAsserts.assertFinalDestination(page);
    }
}
