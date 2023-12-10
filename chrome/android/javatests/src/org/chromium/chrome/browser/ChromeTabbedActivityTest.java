// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Intent;
import android.net.Uri;
import android.os.Build.VERSION_CODES;
import android.provider.Browser;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.runner.lifecycle.Stage;

import com.google.common.collect.Lists;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.IntentUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;

/** Instrumentation tests for ChromeTabbedActivity. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ChromeTabbedActivityTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static final String FILE_PATH = "/chrome/test/data/android/test.html";

    private ChromeTabbedActivity mActivity;

    @Before
    public void setUp() {
        mActivity = sActivityTestRule.getActivity();
    }

    /**
     * Verifies that the front tab receives the hide() call when the activity is stopped (hidden);
     * and that it receives the show() call when the activity is started again. This is a regression
     * test for http://crbug.com/319804 .
     */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1347506")
    public void testTabVisibility() {
        // Create two tabs - tab[0] in the foreground and tab[1] in the background.
        final Tab[] tabs = new Tab[2];
        sActivityTestRule.getTestServer(); // Triggers the lazy initialization of the test server.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Foreground tab.
                    ChromeTabCreator tabCreator = mActivity.getCurrentTabCreator();
                    tabs[0] =
                            tabCreator.createNewTab(
                                    new LoadUrlParams(
                                            sActivityTestRule.getTestServer().getURL(FILE_PATH)),
                                    TabLaunchType.FROM_CHROME_UI,
                                    null);
                    // Background tab.
                    tabs[1] =
                            tabCreator.createNewTab(
                                    new LoadUrlParams(
                                            sActivityTestRule.getTestServer().getURL(FILE_PATH)),
                                    TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                                    null);
                });

        // Verify that the front tab is in the 'visible' state.
        Assert.assertFalse(tabs[0].isHidden());
        Assert.assertTrue(tabs[1].isHidden());

        // Fake sending the activity to background.
        TestThreadUtils.runOnUiThreadBlocking(() -> mActivity.onPause());
        TestThreadUtils.runOnUiThreadBlocking(() -> mActivity.onStop());
        TestThreadUtils.runOnUiThreadBlocking(() -> mActivity.onWindowFocusChanged(false));
        // Verify that both Tabs are hidden.
        Assert.assertTrue(tabs[0].isHidden());
        Assert.assertTrue(tabs[1].isHidden());

        // Fake bringing the activity back to foreground.
        TestThreadUtils.runOnUiThreadBlocking(() -> mActivity.onWindowFocusChanged(true));
        TestThreadUtils.runOnUiThreadBlocking(() -> mActivity.onStart());
        TestThreadUtils.runOnUiThreadBlocking(() -> mActivity.onResume());
        // Verify that the front tab is in the 'visible' state.
        Assert.assertFalse(tabs[0].isHidden());
        Assert.assertTrue(tabs[1].isHidden());
    }

    @Test
    @SmallTest
    public void testTabAnimationsCorrectlyEnabled() {
        boolean animationsEnabled =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> mActivity.getLayoutManager().animationsEnabled());
        Assert.assertEquals(animationsEnabled, DeviceClassManager.enableAnimations());
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    public void testTabModelSelectorObserverOnTabStateInitialized() {
        // Get the original value of |mCreatedTabOnStartup|.
        boolean createdTabOnStartup = mActivity.getCreatedTabOnStartupForTesting();

        // Reset the values of |mCreatedTabOnStartup| and |MultiInstanceManager.mTabModelObserver|.
        // This tab model selector observer should be registered in MultiInstanceManager on tab
        // state initialization irrespective of the value of |mCreatedTabOnStartup|.
        mActivity.setCreatedTabOnStartupForTesting(false);
        mActivity.getMultiInstanceMangerForTesting().setTabModelObserverForTesting(null);

        var tabModelSelectorObserver = mActivity.getTabModelSelectorObserverForTesting();
        TestThreadUtils.runOnUiThreadBlocking(tabModelSelectorObserver::onTabStateInitialized);
        Assert.assertTrue(
                "Regular tab count should be written to SharedPreferences after tab state"
                        + " initialization.",
                ChromeSharedPreferences.getInstance()
                                .readIntsWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_TAB_COUNT)
                                .size()
                        > 0);
        Assert.assertTrue(
                "Incognito tab count should be written to SharedPreferences after tab state"
                        + " initialization.",
                ChromeSharedPreferences.getInstance()
                                .readIntsWithPrefix(
                                        ChromePreferenceKeys.MULTI_INSTANCE_INCOGNITO_TAB_COUNT)
                                .size()
                        > 0);

        // Restore the original value of |mCreatedTabOnStartup|.
        mActivity.setCreatedTabOnStartupForTesting(createdTabOnStartup);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1347506")
    public void testMultiUrlIntent() {
        Intent viewIntent =
                new Intent(
                        Intent.ACTION_VIEW,
                        Uri.parse(sActivityTestRule.getTestServer().getURL("/first")));
        viewIntent.putExtra(
                Browser.EXTRA_APPLICATION_ID, mActivity.getApplicationContext().getPackageName());
        viewIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        viewIntent.putExtra(IntentHandler.EXTRA_PAGE_TRANSITION_TYPE, PageTransition.AUTO_BOOKMARK);
        viewIntent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        viewIntent.setClass(mActivity, ChromeLauncherActivity.class);
        ArrayList<String> extraUrls =
                Lists.newArrayList(
                        sActivityTestRule.getTestServer().getURL("/second"),
                        sActivityTestRule.getTestServer().getURL("/third"));
        viewIntent.putExtra(IntentHandler.EXTRA_ADDITIONAL_URLS, extraUrls);
        IntentUtils.addTrustedIntentExtras(viewIntent);

        mActivity.getApplicationContext().startActivity(viewIntent);
        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel tabModel = mActivity.getCurrentTabModel();
                    Criteria.checkThat(tabModel.getCount(), Matchers.is(4));
                    Criteria.checkThat(
                            tabModel.getTabAt(1).getUrl().getSpec(), Matchers.endsWith("first"));
                    Criteria.checkThat(
                            tabModel.getTabAt(2).getUrl().getSpec(), Matchers.endsWith("second"));
                    Criteria.checkThat(
                            tabModel.getTabAt(3).getUrl().getSpec(), Matchers.endsWith("third"));
                });

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getCurrentTabModel().closeAllTabs(false));

        viewIntent.putExtra(IntentHandler.EXTRA_OPEN_ADDITIONAL_URLS_IN_TAB_GROUP, true);
        mActivity.getApplicationContext().startActivity(viewIntent);
        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel tabModel = mActivity.getCurrentTabModel();
                    Criteria.checkThat(tabModel.getCount(), Matchers.is(3));
                    Criteria.checkThat(
                            tabModel.getTabAt(0).getUrl().getSpec(), Matchers.endsWith("first"));
                    int parentId = tabModel.getTabAt(0).getId();
                    Criteria.checkThat(
                            tabModel.getTabAt(1).getUrl().getSpec(), Matchers.endsWith("second"));
                    Criteria.checkThat(tabModel.getTabAt(1).getParentId(), Matchers.is(parentId));
                    Criteria.checkThat(
                            tabModel.getTabAt(2).getUrl().getSpec(), Matchers.endsWith("third"));
                    Criteria.checkThat(tabModel.getTabAt(2).getParentId(), Matchers.is(parentId));
                });

        viewIntent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        mActivity.getApplicationContext().startActivity(viewIntent);
        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel tabModel = mActivity.getCurrentTabModel();
                    Criteria.checkThat(tabModel.isIncognito(), Matchers.is(true));
                    Criteria.checkThat(tabModel.getCount(), Matchers.is(3));
                    Criteria.checkThat(
                            tabModel.getTabAt(0).getUrl().getSpec(), Matchers.endsWith("first"));
                    Criteria.checkThat(
                            tabModel.getTabAt(1).getUrl().getSpec(), Matchers.endsWith("second"));
                    Criteria.checkThat(
                            tabModel.getTabAt(2).getUrl().getSpec(), Matchers.endsWith("third"));
                });
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.REDIRECT_EXPLICIT_CTA_INTENTS_TO_EXISTING_ACTIVITY)
    @MinAndroidSdkLevel(VERSION_CODES.S)
    public void testExplicitViewIntent_OpensInExistingLiveActivity() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                ChromeTabbedActivity
                                        .HISTOGRAM_EXPLICIT_VIEW_INTENT_FINISHED_NEW_ACTIVITY,
                                true,
                                1)
                        .build();
        int initialWindowCount = MultiWindowUtils.getInstanceCount();
        Intent intent =
                new Intent(Intent.ACTION_VIEW, Uri.parse(JUnitTestGURLs.EXAMPLE_URL.getSpec()));
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        intent.setClass(mActivity, ChromeTabbedActivity.class);

        // The newly created ChromeTabbedActivity (created via #startActivity()) should be
        // destroyed, and the intent should be launched in the existing ChromeTabbedActivity.
        ApplicationTestUtils.waitForActivityWithClass(
                ChromeTabbedActivity.class,
                Stage.DESTROYED,
                () -> mActivity.getApplicationContext().startActivity(intent));

        Assert.assertEquals(
                "No new window should be opened.",
                initialWindowCount,
                MultiWindowUtils.getInstanceCount());
        // A new tab should be opened in the existing ChromeTabbedActivity.
        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel tabModel = mActivity.getCurrentTabModel();
                    Criteria.checkThat(tabModel.getCount(), Matchers.is(2));
                });
        histogramWatcher.assertExpected();
    }
}
