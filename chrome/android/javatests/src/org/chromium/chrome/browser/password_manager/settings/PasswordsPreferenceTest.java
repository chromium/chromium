// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static org.mockito.Mockito.any;
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

import org.chromium.base.test.transit.EntryPointSentinelStation;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.settings.AutofillAndPasswordsFragment;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.LoginDbDeprecationUtilBridge;
import org.chromium.chrome.browser.password_manager.LoginDbDeprecationUtilBridgeJni;
import org.chromium.chrome.browser.password_manager.PasswordManagerTestHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.settings.SettingsTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.settings.PreferenceFacility;
import org.chromium.chrome.test.transit.settings.SettingsStation;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.File;
import java.io.IOException;

/** Public Transit tests for the passwords preference item. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(
        reason =
                "The tests can't be batched because the functionality under test is set up during"
                        + " Chrome start up.")
public class PasswordsPreferenceTest {
    private static final int RENDER_TEST_REVISION = 2;

    @ClassRule
    public static SettingsTestRule<AutofillAndPasswordsFragment> mAutofillSettingsActivityTestRule =
            new SettingsTestRule<>(AutofillAndPasswordsFragment.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE_SETTINGS)
                    .setRevision(RENDER_TEST_REVISION)
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;
    @Mock private LoginDbDeprecationUtilBridge.Natives mLoginDbDeprecationUtilBridgeJniMock;

    @Before
    public void setUp() {
        PasswordManagerUtilBridgeJni.setInstanceForTesting(mPasswordManagerUtilBridgeJniMock);
        LoginDbDeprecationUtilBridgeJni.setInstanceForTesting(mLoginDbDeprecationUtilBridgeJniMock);
        PasswordManagerTestHelper.setUpGmsCoreFakeBackends();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPasswordManagerAvailableNoSubtitle_savedInfoEnabled() throws IOException {
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(true)).thenReturn(true);
        when(mLoginDbDeprecationUtilBridgeJniMock.getAutoExportCsvFilePath(any()))
                .thenReturn("random/file/path");

        SettingsStation<AutofillAndPasswordsFragment> autofillAndPasswordsPage =
                startAutofillAndPasswordsSettings();

        PreferenceFacility passwordsPref =
                autofillAndPasswordsPage.scrollToPref("autofill_and_passwords_gpm");

        mRenderTestRule.render(
                passwordsPref.prefViewElement.value(),
                "passwords_preference_no_subtitle_saved_info");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPwmStoppedWorkingSubtitle_savedInfoEnabled() throws IOException {
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(true)).thenReturn(false);
        when(mLoginDbDeprecationUtilBridgeJniMock.getAutoExportCsvFilePath(any()))
                .thenReturn("random/file/path");

        SettingsStation<AutofillAndPasswordsFragment> autofillAndPasswordsPage =
                startAutofillAndPasswordsSettings();

        PreferenceFacility passwordsPref =
                autofillAndPasswordsPage.scrollToPref("autofill_and_passwords_gpm");

        mRenderTestRule.render(
                passwordsPref.prefViewElement.value(),
                "passwords_preference_gpm_stopped_working_saved_info");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testSomePasswordsNotAccessibleSubtitle_savedInfoEnabled() throws IOException {
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(true)).thenReturn(true);
        File fakeCsv = File.createTempFile("passwords", null, null);
        fakeCsv.deleteOnExit();
        when(mLoginDbDeprecationUtilBridgeJniMock.getAutoExportCsvFilePath(any()))
                .thenReturn(fakeCsv.getAbsolutePath());

        SettingsStation<AutofillAndPasswordsFragment> autofillAndPasswordsPage =
                startAutofillAndPasswordsSettings();

        PreferenceFacility passwordsPref =
                autofillAndPasswordsPage.scrollToPref("autofill_and_passwords_gpm");

        mRenderTestRule.render(
                passwordsPref.prefViewElement.value(),
                "passwords_preference_pwds_not_accessible_saved_info");
    }

    private SettingsStation<AutofillAndPasswordsFragment> startAutofillAndPasswordsSettings() {
        EntryPointSentinelStation sentinel = new EntryPointSentinelStation();
        sentinel.setAsEntryPoint();

        SettingsStation<AutofillAndPasswordsFragment> entryPageStation =
                new SettingsStation<>(AutofillAndPasswordsFragment.class);
        return sentinel.runTo(mAutofillSettingsActivityTestRule::startSettingsActivity)
                .arriveAt(entryPageStation);
    }
}
