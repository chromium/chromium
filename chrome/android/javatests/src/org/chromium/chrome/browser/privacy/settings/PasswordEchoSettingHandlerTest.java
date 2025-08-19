// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import android.provider.Settings;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.uiautomator.UiDevice;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.user_prefs.UserPrefs;

import java.io.IOException;
import java.util.concurrent.ExecutionException;

@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PasswordEchoSettingHandlerTest {
    @Rule public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    private Profile mProfile;
    private PasswordEchoSettingHandler mPasswordEchoSettingHandler;
    private UiDevice mDevice;
    private String mInitialShowPasswordValue;

    @Before
    public void setUp() throws ExecutionException, IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile = ProfileManager.getLastUsedRegularProfile();
                    mPasswordEchoSettingHandler =
                            PasswordEchoSettingHandlerFactory.getForProfile(mProfile);
                });
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        mInitialShowPasswordValue =
                mDevice.executeShellCommand("settings get system show_password").trim();
    }

    @After
    public void tearDown() throws IOException {
        if (!mInitialShowPasswordValue.equals("null")) {
            mDevice.executeShellCommand(
                    "settings put system show_password " + mInitialShowPasswordValue);
        }
    }

    private boolean isPasswordEchoEnabledInPrefService() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> UserPrefs.get(mProfile).getBoolean(Pref.WEB_KIT_PASSWORD_ECHO_ENABLED));
    }

    private boolean isPasswordEchoEnabledInSystemSettings() {
        return Settings.System.getInt(
                        ContextUtils.getApplicationContext().getContentResolver(),
                        Settings.System.TEXT_SHOW_PASSWORD,
                        1)
                == 1;
    }

    @Test
    @SmallTest
    public void testInvokingUpdateMethodSyncsInitialState() throws ExecutionException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PasswordEchoSettingHandlerFactory.getForProfile(mProfile)
                            .updatePasswordEchoState();
                });
        Assert.assertEquals(
                isPasswordEchoEnabledInPrefService(), isPasswordEchoEnabledInSystemSettings());
    }

    private void setSystemPasswordEchoAndAssertState(boolean enabled)
            throws ExecutionException, IOException {
        mDevice.executeShellCommand("settings put system show_password " + (enabled ? "1" : "0"));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPasswordEchoSettingHandler.getPasswordEchoSettingObserver().onChange(true);
                });
        Assert.assertEquals(
                isPasswordEchoEnabledInPrefService(), isPasswordEchoEnabledInSystemSettings());
    }

    @Test
    @SmallTest
    public void testSettingChangeIsObserved() throws ExecutionException, IOException {
        setSystemPasswordEchoAndAssertState(false);

        setSystemPasswordEchoAndAssertState(true);

        setSystemPasswordEchoAndAssertState(false);
    }
}
