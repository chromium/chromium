// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.containsString;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.view.View;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;

import org.chromium.base.CollectionUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;
import java.util.HashSet;

/** Integration tests for ClearBrowsingDataFragmentBasic. */
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

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.PRIVACY)
                    .setRevision(1)
                    .build();

    @Mock private SyncService mMockSyncService;

    @Mock public TemplateUrlService mMockTemplateUrlService;
    @Mock public TemplateUrl mMockSearchEngine;

    @Before
    public void setUp() throws InterruptedException {
        initMocks(this);
        SyncServiceFactory.setInstanceForTesting(mMockSyncService);
        setSyncable(false);
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    private void setSyncable(boolean syncable) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(mMockSyncService.isSyncFeatureEnabled()).thenReturn(syncable);
                    when(mMockSyncService.getActiveDataTypes())
                            .thenReturn(
                                    syncable
                                            ? CollectionUtil.newHashSet(
                                                    DataType.HISTORY_DELETE_DIRECTIVES)
                                            : new HashSet<Integer>());
                });
    }

    private void configureMockSearchEngine() {
        TemplateUrlServiceFactory.setInstanceForTesting(mMockTemplateUrlService);
        Mockito.doReturn(mMockSearchEngine)
                .when(mMockTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();
    }

    private void waitForOptionsMenu() {
        CriteriaHelper.pollUiThread(
                () -> {
                    return mSettingsActivityTestRule
                                    .getActivity()
                                    .findViewById(R.id.menu_id_targeted_help)
                            != null;
                });
    }

    private void waitForCacheCounter() {
        // The cache counter is populated asynchronusly.
        ViewUtils.waitForVisibleView(withText(containsString("Frees up")));
    }

    private SettingsActivity startPreferences() {
        SettingsActivity settingsActivity =
                mSettingsActivityTestRule.startSettingsActivity(
                        ClearBrowsingDataFragment.createFragmentArgs(
                                mActivityTestRule.getActivity().getClass().getName(),
                                /* isFetcherSuppliedFromOutside= */ false));
        return settingsActivity;
    }

    @Test
    @LargeTest
    public void testSignOutLinkNotOfferedToSupervisedAccounts() {
        mSigninTestRule.addChildTestAccountThenWaitForSignin();
        setSyncable(false);
        final SettingsActivity settingsActivity = startPreferences();
        waitForOptionsMenu();

        final ClearBrowsingDataFragmentBasic clearBrowsingDataFragmentBasic =
                (ClearBrowsingDataFragmentBasic) settingsActivity.getMainFragment();
        onView(withText(clearBrowsingDataFragmentBasic.buildSignOutOfChromeText().toString()))
                .check(doesNotExist());
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderSignedInAndSyncing() throws IOException {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        setSyncable(true);
        final SettingsActivity settingsActivity = startPreferences();
        waitForOptionsMenu();
        View view = settingsActivity.findViewById(android.R.id.content).getRootView();
        waitForCacheCounter();
        mRenderTestRule.render(view, "clear_browsing_data_basic_signed_in_sync");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderSearchHistoryLinkSignedOutGoogleDSE() throws IOException {
        final SettingsActivity settingsActivity = startPreferences();
        waitForOptionsMenu();
        View view = settingsActivity.findViewById(android.R.id.content).getRootView();
        waitForCacheCounter();
        mRenderTestRule.render(view, "clear_browsing_data_basic_shl_google_signed_out");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderSearchHistoryLinkSignedInGoogleDSE() throws IOException {
        mSigninTestRule.addTestAccountThenSignin();
        setSyncable(false);
        final SettingsActivity settingsActivity = startPreferences();
        waitForOptionsMenu();
        View view = settingsActivity.findViewById(android.R.id.content).getRootView();
        waitForCacheCounter();
        mRenderTestRule.render(view, "clear_browsing_data_basic_shl_google_signed_in");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderSearchHistoryLinkSignedInKnownNonGoogleDSE() throws IOException {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        setSyncable(false);
        configureMockSearchEngine();
        Mockito.doReturn(false).when(mMockTemplateUrlService).isDefaultSearchEngineGoogle();
        Mockito.doReturn(true).when(mMockSearchEngine).getIsPrepopulated();

        final SettingsActivity settingsActivity = startPreferences();
        waitForOptionsMenu();
        View view = settingsActivity.findViewById(android.R.id.content).getRootView();
        waitForCacheCounter();
        mRenderTestRule.render(view, "clear_browsing_data_basic_shl_known_signed_in");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderSearchHistoryLinkSignedInUnknownNonGoogleDSE() throws IOException {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        setSyncable(false);
        configureMockSearchEngine();
        Mockito.doReturn(false).when(mMockTemplateUrlService).isDefaultSearchEngineGoogle();
        Mockito.doReturn(false).when(mMockSearchEngine).getIsPrepopulated();

        final SettingsActivity settingsActivity = startPreferences();
        waitForOptionsMenu();
        View view = settingsActivity.findViewById(android.R.id.content).getRootView();
        waitForCacheCounter();
        mRenderTestRule.render(view, "clear_browsing_data_basic_shl_unknown_signed_in");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderSearchHistoryLinkSignedOutKnownNonGoogleDSE() throws IOException {
        configureMockSearchEngine();
        Mockito.doReturn(false).when(mMockTemplateUrlService).isDefaultSearchEngineGoogle();
        Mockito.doReturn(true).when(mMockSearchEngine).getIsPrepopulated();

        final SettingsActivity settingsActivity = startPreferences();
        waitForOptionsMenu();
        View view = settingsActivity.findViewById(android.R.id.content).getRootView();
        waitForCacheCounter();
        mRenderTestRule.render(view, "clear_browsing_data_basic_shl_known_signed_out");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderSearchHistoryLinkSignedOutUnknownNonGoogleDSE() throws IOException {
        configureMockSearchEngine();
        Mockito.doReturn(false).when(mMockTemplateUrlService).isDefaultSearchEngineGoogle();
        Mockito.doReturn(false).when(mMockSearchEngine).getIsPrepopulated();

        final SettingsActivity settingsActivity = startPreferences();
        waitForOptionsMenu();
        View view = settingsActivity.findViewById(android.R.id.content).getRootView();
        waitForCacheCounter();
        mRenderTestRule.render(view, "clear_browsing_data_basic_shl_unknown_signed_out");
    }
}
