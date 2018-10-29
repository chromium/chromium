// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags.Add;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.policy.test.annotations.Policies;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Tests for EnabledStateMonitor.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_BUTTON)
public class EnabledStateMonitorTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private ProfileSyncServiceStub mProfileSyncServiceStub;
    private EnabledStateMonitor mEnabledStateMonitor;

    private String mOriginalSignedInAccountName;

    private static class ProfileSyncServiceStub extends ProfileSyncService {
        public ProfileSyncServiceStub() {
            super();
        }

        @Override
        public boolean isUrlKeyedDataCollectionEnabled(boolean personalized) {
            return true;
        }
    }

    @Before
    public void setUp() throws Exception {
        LocaleManager.setInstanceForTest(new LocaleManager() {
            @Override
            public boolean needToCheckForSearchEnginePromo() {
                return false;
            }
        });

        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mOriginalSignedInAccountName = ChromeSigninController.get().getSignedInAccountName();
            ChromeSigninController.get().setSignedInAccountName("test@gmail.com");
            mProfileSyncServiceStub = new ProfileSyncServiceStub();
            ProfileSyncService.overrideForTests(mProfileSyncServiceStub);
            mEnabledStateMonitor = new EnabledStateMonitorImpl();
        });
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeSigninController.get().setSignedInAccountName(mOriginalSignedInAccountName);
            // Clear ProfileSyncService in case it was mocked.
            ProfileSyncService.resetForTests();
        });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSuggestions"})
    @Policies.Add({ @Policies.Item(key = "ContextualSuggestionsEnabled", string = "false") })
    public void testEnterprisePolicy_Disabled() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(mEnabledStateMonitor.getEnabledState());
            Assert.assertFalse(mEnabledStateMonitor.getSettingsEnabled());
        });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSuggestions"})
    @Policies.Add({ @Policies.Item(key = "ContextualSuggestionsEnabled", string = "true") })
    public void testEnterprisePolicy_Enabled() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(mEnabledStateMonitor.getEnabledState());
            Assert.assertTrue(mEnabledStateMonitor.getSettingsEnabled());
        });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSuggestions"})
    @Policies.Remove({ @Policies.Item(key = "ContextualSuggestionsEnabled") })
    public void testEnterprisePolicy_DefaultEnabled() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(mEnabledStateMonitor.getEnabledState());
            Assert.assertTrue(mEnabledStateMonitor.getSettingsEnabled());
        });
    }
}
