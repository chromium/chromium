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
import org.junit.BeforeClass;
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
import org.chromium.components.embedder_support.util.PasswordEchoSettingState;
import org.chromium.components.user_prefs.UserPrefs;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ExecutionException;

@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PasswordEchoSplitSettingHandlerTest {
    private static class SettingType {
        public final String key;
        public final String pref;
        public final int defaultVal;

        private SettingType(String key, String pref, int defaultVal) {
            this.key = key;
            this.pref = pref;
            this.defaultVal = defaultVal;
        }

        private static final SettingType PHYSICAL =
                new SettingType(
                        "show_password_physical", Pref.WEB_KIT_PASSWORD_ECHO_ENABLED_PHYSICAL, 0);
        private static final SettingType TOUCH =
                new SettingType("show_password_touch", Pref.WEB_KIT_PASSWORD_ECHO_ENABLED_TOUCH, 1);

        public static SettingType[] values() {
            return new SettingType[] {PHYSICAL, TOUCH};
        }
    }

    @Rule public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    private Profile mProfile;
    private PasswordEchoSettingHandler mPasswordEchoSettingHandler;
    private UiDevice mDevice;
    private final Map<SettingType, String> mInitialShowPasswordValue = new HashMap<>();

    @BeforeClass
    public static void setUpClass() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Enable setting split feature.
                    PasswordEchoSettingState.setInstanceForTests(true);
                });
    }

    @Before
    public void setUp() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile = ProfileManager.getLastUsedRegularProfile();
                    mPasswordEchoSettingHandler =
                            PasswordEchoSettingHandlerFactory.getForProfile(mProfile);
                });

        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());

        for (SettingType type : SettingType.values()) {
            final String shellCommand = String.format("settings get secure %s", type.key);
            final String value = mDevice.executeShellCommand(shellCommand).trim();
            mInitialShowPasswordValue.put(type, value);
        }
    }

    @After
    public void tearDown() throws IOException {
        for (SettingType type : SettingType.values()) {
            final String initialValue = mInitialShowPasswordValue.getOrDefault(type, "null");
            final String shellCommand;

            if (initialValue.equals("null")) {
                shellCommand = String.format("settings delete secure %s", type.key);
            } else {
                shellCommand = String.format("settings put secure %s %s", type.key, initialValue);
            }
            mDevice.executeShellCommand(shellCommand);
        }
    }

    private boolean isPasswordEchoEnabledInPrefService(SettingType type) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> UserPrefs.get(mProfile).getBoolean(type.pref));
    }

    private boolean isPasswordEchoEnabledInSystemSettings(SettingType type) {
        return Settings.Secure.getInt(
                        ContextUtils.getApplicationContext().getContentResolver(),
                        type.key,
                        type.defaultVal)
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

        for (SettingType type : SettingType.values()) {
            Assert.assertEquals(
                    isPasswordEchoEnabledInPrefService(type),
                    isPasswordEchoEnabledInSystemSettings(type));
        }
    }

    private void setSystemPasswordEchoAndAssertState(SettingType type, boolean enabled)
            throws ExecutionException, IOException {
        final String shellCommand =
                String.format("settings put secure %s %s", type.key, enabled ? "1" : "0");
        mDevice.executeShellCommand(shellCommand);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PasswordEchoSettingState.getInstance()
                            .getSettingObserver()
                            .onChange(true, Settings.Secure.getUriFor(type.key));
                });
        Assert.assertEquals(
                isPasswordEchoEnabledInPrefService(type),
                isPasswordEchoEnabledInSystemSettings(type));
    }

    @Test
    @SmallTest
    public void testSettingChangeIsObserved() throws ExecutionException, IOException {
        for (SettingType type : SettingType.values()) {
            setSystemPasswordEchoAndAssertState(type, false);
            setSystemPasswordEchoAndAssertState(type, true);
            setSystemPasswordEchoAndAssertState(type, false);
        }
    }

    // If the setting was never set by the user, the system should have the appropriate default
    // value according to the setting type.
    // The default for the touch setting is 'enabled'.
    // The default for the physical setting is 'disabled'.
    @Test
    @SmallTest
    public void testDefaultStateIfNeverSet() throws ExecutionException, IOException {
        for (SettingType type : SettingType.values()) {
            // Clear the setting from the device to test default behavior.
            final String initialValue = mInitialShowPasswordValue.getOrDefault(type, "null");
            if (!initialValue.equals("null")) {
                final String shellCommand = String.format("settings delete secure %s", type.key);
                mDevice.executeShellCommand(shellCommand);
            }

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        PasswordEchoSettingState.getInstance()
                                .getSettingObserver()
                                .onChange(true, Settings.Secure.getUriFor(type.key));
                    });

            Assert.assertEquals(type.defaultVal == 1, isPasswordEchoEnabledInPrefService(type));
            Assert.assertEquals(type.defaultVal == 1, isPasswordEchoEnabledInSystemSettings(type));
        }
    }
}
