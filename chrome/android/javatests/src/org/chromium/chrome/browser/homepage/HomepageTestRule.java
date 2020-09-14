// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage;

import android.support.test.InstrumentationRegistry;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.policy.AbstractAppRestrictionsProvider;
import org.chromium.components.policy.AppRestrictionsProvider;
import org.chromium.components.policy.CombinedPolicyProvider;
import org.chromium.components.policy.test.PolicyData;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;

/**
 * Test rule for homepage related tests. It fetches the latest values from shared preference
 * manager, copy them before test starts, and restores those values inside SharedPreferenceManager
 * when test finished.
 */
public class HomepageTestRule implements TestRule {
    private boolean mIsHomepageEnabled;
    private boolean mIsUsingChromeNTP;
    private boolean mIsUsingDefaultHomepage;
    private String mCustomizedHomepage;

    private final SharedPreferencesManager mManager;
    private @Nullable AppRestrictionsProvider mTestProvider;

    public HomepageTestRule() {
        mManager = SharedPreferencesManager.getInstance();
    }

    @Override
    public Statement apply(final Statement base, Description desc) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                try {
                    copyInitialValueBeforeTest();
                    base.evaluate();
                } finally {
                    restoreHomepageRelatedPreferenceAfterTest();
                    // Reset preference if a test policy override is provided in test.
                    if (mTestProvider != null) setHomepagePolicyForTest(null);
                }
            }
        };
    }

    private void copyInitialValueBeforeTest() {
        mIsHomepageEnabled = mManager.readBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        mIsUsingChromeNTP =
                mManager.readBoolean(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, false);
        mIsUsingDefaultHomepage =
                mManager.readBoolean(ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, true);
        mCustomizedHomepage = mManager.readString(ChromePreferenceKeys.HOMEPAGE_CUSTOM_URI, "");
    }

    private void restoreHomepageRelatedPreferenceAfterTest() {
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, mIsHomepageEnabled);
        mManager.writeBoolean(
                ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, mIsUsingDefaultHomepage);
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, mIsUsingChromeNTP);
        mManager.writeString(ChromePreferenceKeys.HOMEPAGE_CUSTOM_URI, mCustomizedHomepage);
    }

    /**
     * Setup or disable HomepageLocation policy for test.
     * @param homepage String value of HomepageLocation. If the input string is empty, policy for
     *         test will be disabled.
     */
    public void setHomepagePolicyForTest(String homepage) {
        if (TextUtils.isEmpty(homepage)) {
            AbstractAppRestrictionsProvider.setTestRestrictions(null);
        } else {
            final PolicyData[] policies = {
                    new PolicyData.Str("HomepageLocation", homepage),
            };
            AbstractAppRestrictionsProvider.setTestRestrictions(
                    PolicyData.asBundle(Arrays.asList(policies)));
        }

        if (mTestProvider == null) {
            mTestProvider = new AppRestrictionsProvider(
                    InstrumentationRegistry.getInstrumentation().getContext());
            CombinedPolicyProvider.get().registerProvider(mTestProvider);
        }
        TestThreadUtils.runOnUiThreadBlocking(mTestProvider::refresh);

        // To avoid race conditions
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    // Utility functions that help setting up homepage related shared preference.

    /**
     * Set homepage disabled for this test cases.
     *
     * HOMEPAGE_ENABLED -> false
     */
    public void disableHomepageForTest() {
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, false);
    }

    /**
     * Set up shared preferences to use default Homepage in the testcase.
     *
     * HOMEPAGE_ENABLED -> true;
     * HOMEPAGE_USE_DEFAULT_URI -> true;
     * HOMEPAGE_USE_CHROME_NTP -> false;
     */
    public void useDefaultHomepageForTest() {
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, true);
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, false);
    }

    /**
     * Set up shared preferences to use Chrome NTP as homepage. This is to select chrome NTP in the
     * home settings page, rather than setting the address of Chrome NTP as customized homepage.
     *
     * HOMEPAGE_ENABLED -> true;
     * HOMEPAGE_USE_DEFAULT_URI -> false;
     * HOMEPAGE_USE_CHROME_NTP -> true;
     */
    public void useChromeNTPForTest() {
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, false);
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, true);
    }

    /**
     * Set up shared preferences to use customized homepage.
     *
     * HOMEPAGE_ENABLED -> true;
     * HOMEPAGE_USE_DEFAULT_URI -> false;
     * HOMEPAGE_USE_CHROME_NTP -> false;
     * HOMEPAGE_CUSTOM_URI -> <b>homepage</b>
     *
     * @param homepage The customized homepage that will be used in this testcase.
     */
    public void useCustomizedHomepageForTest(String homepage) {
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_DEFAULT_URI, false);
        mManager.writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, false);
        mManager.writeString(ChromePreferenceKeys.HOMEPAGE_CUSTOM_URI, homepage);
    }
}
