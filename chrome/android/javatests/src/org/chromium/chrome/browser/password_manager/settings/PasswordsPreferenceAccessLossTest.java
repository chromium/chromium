// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static org.mockito.ArgumentMatchers.any;
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

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.transit.TransitAsserts;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.access_loss.PasswordAccessLossWarningType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.PasswordManagerTestHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.transit.settings.PreferenceFacility;
import org.chromium.chrome.test.transit.settings.SettingsActivityPublicTransitEntryPoints;
import org.chromium.chrome.test.transit.settings.SettingsStation;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/**
 * Public Transit tests for the passwords preference item, parameterized by the passwords access
 * loss warning type.
 */
@RunWith(ParameterizedRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(
        reason =
                "The tests can't be batched because the functionality under test is set up during"
                        + " Chrome start up.")
@DisableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
public class PasswordsPreferenceAccessLossTest {
    @ClassRule
    public static SettingsActivityTestRule<MainSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(MainSettings.class);

    private final String mName;

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE_SETTINGS)
                    .setRevision(1)
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;

    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet()
                            .value(PasswordAccessLossWarningType.NO_GMS_CORE, "NoGmsCore")
                            .name("NoGmsCore"),
                    new ParameterSet()
                            .value(PasswordAccessLossWarningType.NO_UPM, "NoUpm")
                            .name("NoUpm"),
                    new ParameterSet()
                            .value(
                                    PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED,
                                    "NewGmsCoreMigrationFailed")
                            .name("NewGmsCoreMigrationFailed"),
                    new ParameterSet()
                            .value(PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM, "OnlyAccountGms")
                            .name("OnlyAccountGms"));

    SettingsActivityPublicTransitEntryPoints mEntryPoints =
            new SettingsActivityPublicTransitEntryPoints(mSettingsActivityTestRule);

    private final @PasswordAccessLossWarningType int mWarningType;

    public PasswordsPreferenceAccessLossTest(
            @PasswordAccessLossWarningType int warningType, String name) {
        mWarningType = warningType;
        mName = name;
    }

    @Before
    public void setUp() {
        PasswordManagerUtilBridgeJni.setInstanceForTesting(mPasswordManagerUtilBridgeJniMock);
        PasswordManagerTestHelper.setUpGmsCoreFakeBackends();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testAccessLossWarningPasswordsPreference() throws IOException {
        when(mPasswordManagerUtilBridgeJniMock.getPasswordAccessLossWarningType(any()))
                .thenReturn(mWarningType);

        SettingsStation<MainSettings> page = mEntryPoints.startMainSettingsNonBatched();
        PreferenceFacility passwordsPref = page.scrollToPref(MainSettings.PREF_PASSWORDS);

        mRenderTestRule.render(
                passwordsPref.prefViewElement.get(), "passwords_preference_" + mName);
        TransitAsserts.assertFinalDestination(page);
    }
}
