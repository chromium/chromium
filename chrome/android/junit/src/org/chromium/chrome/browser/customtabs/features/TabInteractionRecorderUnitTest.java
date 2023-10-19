// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;

/** Unit test for {@link TabInteractionRecorder} on the java side. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabInteractionRecorderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock Tab mTab;

    private TestNativeInteractionRecorder mTestNative;
    private TabInteractionRecorder mTestRecorder;
    private SharedPreferencesManager mPref;

    @Before
    public void setup() {
        mTestNative = new TestNativeInteractionRecorder();
        mJniMocker.mock(TabInteractionRecorderJni.TEST_HOOKS, mTestNative);
        mPref = ChromeSharedPreferences.getInstance();

        TabInteractionRecorder.createForTab(mTab);
        TabInteractionRecorder recorder = TabInteractionRecorder.getFromTab(mTab);
        Assert.assertNotNull("Recorder not created.", recorder);
    }

    @After
    public void tearDown() {
        mPref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION);
        mPref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TIMESTAMP);
    }

    @Test
    public void hadFormInteractionOnTabClosing() {
        mTestNative.paramHadFormInteractionInSession = true;
        mTestNative.paramHadFormInteractionInActivePage = true;
        TabInteractionRecorder.getFromTab(mTab).onTabClosing();

        Assert.assertTrue(
                "Shared pref <CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION> is not recorded.",
                mPref.contains(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION));
        Assert.assertTrue(
                "Value of shared pref <CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION> is wrong.",
                mPref.readBoolean(
                        ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION, false));
        Assert.assertTrue(
                "Shared pref <CUSTOM_TABS_LAST_CLOSE_TIMESTAMP> is not recorded.",
                mPref.contains(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TIMESTAMP));
    }

    @Test
    public void hadNavigationInteractionOnTabClosing() {
        mTestNative.paramHadNavigationInteraction = true;
        TabInteractionRecorder.getFromTab(mTab).onTabClosing();

        Assert.assertTrue(
                "Shared pref <CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION> is not recorded.",
                mPref.contains(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION));
        Assert.assertTrue(
                "Value of shared pref <CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION> is wrong.",
                mPref.readBoolean(
                        ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION, false));
        Assert.assertTrue(
                "Shared pref <CUSTOM_TABS_LAST_CLOSE_TIMESTAMP> is not recorded.",
                mPref.contains(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TIMESTAMP));
    }

    @Test
    public void noInteractionOnTabClosing() {
        mTestNative.paramHadFormInteractionInSession = false;
        mTestNative.paramHadFormInteractionInActivePage = false;
        mTestNative.paramHadNavigationInteraction = false;
        TabInteractionRecorder.getFromTab(mTab).onTabClosing();

        Assert.assertTrue(
                "Shared pref <CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION> is not recorded.",
                mPref.contains(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION));
        Assert.assertFalse(
                "Value of shared pref <CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION> is wrong.",
                mPref.readBoolean(
                        ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION, false));
        Assert.assertTrue(
                "Shared pref <CUSTOM_TABS_LAST_CLOSE_TIMESTAMP> is not recorded.",
                mPref.contains(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TIMESTAMP));
    }

    class TestNativeInteractionRecorder implements TabInteractionRecorder.Natives {
        public boolean paramDidGetUserInteraction;
        public boolean paramHadFormInteractionInSession;
        public boolean paramHadFormInteractionInActivePage;
        public boolean paramHadNavigationInteraction;

        @Override
        public TabInteractionRecorder getFromTab(Tab tab) {
            return mTestRecorder;
        }

        @Override
        public TabInteractionRecorder createForTab(Tab tab) {
            mTestRecorder = new TabInteractionRecorder();
            return mTestRecorder;
        }

        @Override
        public boolean didGetUserInteraction(long nativeTabInteractionRecorderAndroid) {
            return paramDidGetUserInteraction;
        }

        @Override
        public boolean hadFormInteractionInSession(long nativeTabInteractionRecorderAndroid) {
            return paramHadFormInteractionInSession;
        }

        @Override
        public boolean hadFormInteractionInActivePage(long nativeTabInteractionRecorderAndroid) {
            return paramHadFormInteractionInActivePage;
        }

        @Override
        public boolean hadNavigationInteraction(long nativeTabInteractionRecorderAndroid) {
            return paramHadNavigationInteraction;
        }

        @Override
        public void reset(long nativeTabInteractionRecorderAndroid) {
            paramHadFormInteractionInSession = false;
            paramHadFormInteractionInActivePage = false;
            paramHadNavigationInteraction = false;
            paramDidGetUserInteraction = false;
        }
    }
}
