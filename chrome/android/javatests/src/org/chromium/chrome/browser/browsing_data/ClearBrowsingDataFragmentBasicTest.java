// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import androidx.preference.CheckBoxPreference;
import androidx.preference.PreferenceScreen;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.CollectionUtil;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragment.DialogOption;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.sync.ModelType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.HashSet;

/**
 * Integration tests for ClearBrowsingDataFragmentBasic.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ClearBrowsingDataFragmentBasicTest {
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();
    public final SettingsActivityTestRule<ClearBrowsingDataFragmentBasic>
            mSettingsActivityTestRule =
                    new SettingsActivityTestRule<>(ClearBrowsingDataFragmentBasic.class);

    // SettingsActivity has to be finished before the outer CTA can be finished or trying to finish
    // CTA won't work.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mActivityTestRule).around(mSettingsActivityTestRule);

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    private static final String GOOGLE_ACCOUNT = "Google Account";
    private static final String OTHER_ACTIVITY = "other forms of browsing history";
    private static final String SYNCED_DEVICES = "synced devices";

    @Mock
    private SyncService mMockSyncService;

    @Before
    public void setUp() throws InterruptedException {
        initMocks(this);
        TestThreadUtils.runOnUiThreadBlocking(() -> SyncService.overrideForTests(mMockSyncService));
        setSyncable(false);
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> SyncService.resetForTests());
    }

    private void setSyncable(boolean syncable) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            when(mMockSyncService.isSyncRequested()).thenReturn(syncable);
            when(mMockSyncService.getActiveDataTypes())
                    .thenReturn(syncable
                                    ? CollectionUtil.newHashSet(ModelType.HISTORY_DELETE_DIRECTIVES)
                                    : new HashSet<Integer>());
        });
    }

    private String getCheckboxSummary(PreferenceScreen screen, String preference) {
        CheckBoxPreference checkbox = (CheckBoxPreference) screen.findPreference(preference);
        return new StringBuilder(checkbox.getSummary()).toString();
    }

    /**
     * Tests that for users who are not signed in, only the general information is shown.
     */
    @Test
    @SmallTest
    public void testCheckBoxTextNotSignedIn() {
        mSettingsActivityTestRule.startSettingsActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ClearBrowsingDataFragmentBasic fragment = mSettingsActivityTestRule.getFragment();
            PreferenceScreen screen = fragment.getPreferenceScreen();

            String cookiesSummary = getCheckboxSummary(screen,
                    ClearBrowsingDataFragment.getPreferenceKey(
                            DialogOption.CLEAR_COOKIES_AND_SITE_DATA));
            String historySummary = getCheckboxSummary(
                    screen, ClearBrowsingDataFragment.getPreferenceKey(DialogOption.CLEAR_HISTORY));

            assertThat(cookiesSummary, not(containsString(GOOGLE_ACCOUNT)));
            assertThat(historySummary, not(containsString(OTHER_ACTIVITY)));
            assertThat(historySummary, not(containsString(SYNCED_DEVICES)));
        });
    }

    /**
     * Tests that for users who are signed in with a primary account but have
     * sync disabled, only "google account" and "other activity" are shown.
     */
    @Test
    @SmallTest
    public void testCheckBoxTextSignedInButNotSyncing() {
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();
        // Simulate that Sync was stopped but the primary account remained.
        setSyncable(false);

        mSettingsActivityTestRule.startSettingsActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ClearBrowsingDataFragmentBasic fragment = mSettingsActivityTestRule.getFragment();
            PreferenceScreen screen = fragment.getPreferenceScreen();

            String cookiesSummary = getCheckboxSummary(screen,
                    ClearBrowsingDataFragment.getPreferenceKey(
                            DialogOption.CLEAR_COOKIES_AND_SITE_DATA));
            String historySummary = getCheckboxSummary(
                    screen, ClearBrowsingDataFragment.getPreferenceKey(DialogOption.CLEAR_HISTORY));

            assertThat(cookiesSummary, containsString(GOOGLE_ACCOUNT));
            assertThat(historySummary, containsString(OTHER_ACTIVITY));
            assertThat(historySummary, not(containsString(SYNCED_DEVICES)));
        });
    }

    /**
     * Tests that users who are signed in, and have sync enabled see information
     * about their "google account", "other activity" and history on "signed in
     * devices".
     */
    @Test
    @SmallTest
    public void testCheckBoxTextSignedInAndSyncing() {
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();
        setSyncable(true);

        mSettingsActivityTestRule.startSettingsActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ClearBrowsingDataFragmentBasic fragment = mSettingsActivityTestRule.getFragment();
            PreferenceScreen screen = fragment.getPreferenceScreen();

            String cookiesSummary = getCheckboxSummary(screen,
                    ClearBrowsingDataFragment.getPreferenceKey(
                            DialogOption.CLEAR_COOKIES_AND_SITE_DATA));
            String historySummary = getCheckboxSummary(
                    screen, ClearBrowsingDataFragment.getPreferenceKey(DialogOption.CLEAR_HISTORY));

            assertThat(cookiesSummary, containsString(GOOGLE_ACCOUNT));
            assertThat(historySummary, containsString(OTHER_ACTIVITY));
            assertThat(historySummary, containsString(SYNCED_DEVICES));
        });
    }
}
