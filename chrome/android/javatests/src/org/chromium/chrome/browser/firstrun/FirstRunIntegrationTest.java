// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.mockito.ArgumentMatchers.any;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.widget.Button;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.CollectionUtil;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.locale.DefaultSearchEngineDialogHelperUtils;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManager.SearchEnginePromoType;
import org.chromium.chrome.browser.policy.EnterpriseInfo;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.MultiActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.policy.AbstractAppRestrictionsProvider;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Integration test suite for the first run experience.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Features.EnableFeatures(ChromeFeatureList.SHARE_BY_DEFAULT_IN_CCT)
public class FirstRunIntegrationTest {
    private static final long DEFERRED_START_UP_POLL_TIME = 10000L;
    @Rule
    public MultiActivityTestRule mTestRule = new MultiActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    public FirstRunAppRestrictionInfo mMockAppRestrictionInfo;
    @Mock
    public EnterpriseInfo mEnterpriseInfo;

    private final Set<Class> mSupportedActivities =
            CollectionUtil.newHashSet(ChromeLauncherActivity.class, FirstRunActivity.class,
                    ChromeTabbedActivity.class, CustomTabActivity.class);
    private final Map<Class, ActivityMonitor> mMonitorMap = new HashMap<>();
    private Instrumentation mInstrumentation;
    private Context mContext;

    private FirstRunActivityTestObserver mTestObserver = new FirstRunActivityTestObserver();
    private Activity mLastActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        FirstRunStatus.setFirstRunSkippedByPolicy(false);
        FirstRunUtils.setDisableDelayOnExitFreForTest(true);
        FirstRunActivity.setObserverForTest(mTestObserver);
        ToSAndUMAFirstRunFragment.setShowUmaCheckBoxForTesting(true);

        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mContext = mInstrumentation.getTargetContext();
        for (Class clazz : mSupportedActivities) {
            ActivityMonitor monitor = new ActivityMonitor(clazz.getName(), null, false);
            mMonitorMap.put(clazz, monitor);
            mInstrumentation.addMonitor(monitor);
        }
    }

    @After
    public void tearDown() {
        FirstRunStatus.setFirstRunSkippedByPolicy(false);
        FirstRunUtils.setDisableDelayOnExitFreForTest(false);
        FirstRunAppRestrictionInfo.setInitializedInstanceForTest(null);
        ToSAndUMAFirstRunFragment.setShowUmaCheckBoxForTesting(false);
        EnterpriseInfo.setInstanceForTest(null);
        if (mLastActivity != null) mLastActivity.finish();
    }

    private ActivityMonitor getMonitor(Class activityClass) {
        Assert.assertTrue(mSupportedActivities.contains(activityClass));
        return mMonitorMap.get(activityClass);
    }

    private <T extends Activity> T waitForActivity(Class<T> activityClass) {
        Assert.assertTrue(mSupportedActivities.contains(activityClass));
        ActivityMonitor monitor = getMonitor(activityClass);
        mLastActivity = mInstrumentation.waitForMonitorWithTimeout(
                monitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertNotNull(mLastActivity);
        return (T) mLastActivity;
    }

    private void setHasAppRestrictionForMock() {
        Mockito.doAnswer(invocation -> {
                   Callback<Boolean> callback = invocation.getArgument(0);
                   callback.onResult(true);
                   return null;
               })
                .when(mMockAppRestrictionInfo)
                .getHasAppRestriction(any());
        FirstRunAppRestrictionInfo.setInitializedInstanceForTest(mMockAppRestrictionInfo);
    }

    private void setDeviceOwnedForMock() {
        Mockito.doAnswer(invocation -> {
                   Callback<EnterpriseInfo.OwnedState> callback = invocation.getArgument(0);
                   callback.onResult(new EnterpriseInfo.OwnedState(true, false));
                   return null;
               })
                .when(mEnterpriseInfo)
                .getDeviceEnterpriseInfo(any());
        EnterpriseInfo.setInstanceForTest(mEnterpriseInfo);
    }

    private void skipTosDialogViaPolicy() {
        setHasAppRestrictionForMock();
        Bundle restrictions = new Bundle();
        restrictions.putInt("TosDialogBehavior", TosDialogBehavior.SKIP);
        AbstractAppRestrictionsProvider.setTestRestrictions(restrictions);
        setDeviceOwnedForMock();
    }

    @Test
    @SmallTest
    public void testHelpPageSkipsFirstRun() {
        // Fire an Intent to load a generic URL.
        CustomTabActivity.showInfoPage(mContext, "http://google.com");

        // The original activity should be started because it's a "help page".
        waitForActivity(CustomTabActivity.class);
        Assert.assertFalse(mLastActivity.isFinishing());

        // First run should be skipped for this Activity.
        Assert.assertEquals(0, getMonitor(FirstRunActivity.class).getHits());
    }

    @Test
    @SmallTest
    public void testAbortFirstRun() throws Exception {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://test.com"));
        intent.setPackage(mContext.getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);

        Activity chromeLauncherActivity = waitForActivity(ChromeLauncherActivity.class);

        // Because the ChromeLauncherActivity notices that the FRE hasn't been run yet, it
        // redirects to it.
        waitForActivity(FirstRunActivity.class);

        // Once the user closes the FRE, the user should be kicked back into the
        // startup flow where they were interrupted.
        Assert.assertEquals(0, mTestObserver.abortFirstRunExperienceCallback.getCallCount());
        mLastActivity.onBackPressed();
        mTestObserver.abortFirstRunExperienceCallback.waitForCallback(
                "FirstRunActivity didn't abort", 0);

        CriteriaHelper.pollInstrumentationThread(() -> mLastActivity.isFinishing());

        // ChromeLauncherActivity should finish if FRE was aborted.
        CriteriaHelper.pollInstrumentationThread(() -> chromeLauncherActivity.isFinishing());
    }

    @Test
    @MediumTest
    public void testDefaultSearchEngine_DontShow() throws Exception {
        runSearchEnginePromptTest(LocaleManager.SearchEnginePromoType.DONT_SHOW);
    }

    @Test
    @MediumTest
    public void testDefaultSearchEngine_ShowExisting() throws Exception {
        runSearchEnginePromptTest(LocaleManager.SearchEnginePromoType.SHOW_EXISTING);
    }

    @Test
    @MediumTest
    public void testDefaultSearchEngine_WithCctPolicy() throws Exception {
        skipTosDialogViaPolicy();

        runSearchEnginePromptTest(LocaleManager.SearchEnginePromoType.SHOW_EXISTING);
    }

    private void runSearchEnginePromptTest(@SearchEnginePromoType final int searchPromoType)
            throws Exception {
        // Force the LocaleManager into a specific state.
        LocaleManager mockManager = new LocaleManager() {
            @Override
            public int getSearchEnginePromoShowType() {
                return searchPromoType;
            }

            @Override
            public List<TemplateUrl> getSearchEnginesForPromoDialog(int promoType) {
                return TemplateUrlServiceFactory.get().getTemplateUrls();
            }
        };
        LocaleManager.setInstanceForTest(mockManager);

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://test.com"));
        intent.setPackage(mContext.getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);

        // Because the AsyncInitializationActivity notices that the FRE hasn't been run yet, it
        // redirects to it.  Once the user closes the FRE, the user should be kicked back into the
        // startup flow where they were interrupted.
        waitForActivity(FirstRunActivity.class);

        mTestObserver.flowIsKnownCallback.waitForCallback("Failed to finalize the flow", 0);
        Bundle freProperties = mTestObserver.freProperties;
        Assert.assertEquals(0, mTestObserver.updateCachedEngineCallback.getCallCount());

        // Accept the ToS.
        clickButton(mLastActivity, R.id.terms_accept, "Failed to accept ToS");
        mTestObserver.jumpToPageCallback.waitForCallback(
                "Failed to try moving to the next screen", 0);
        mTestObserver.acceptTermsOfServiceCallback.waitForCallback("Failed to accept the ToS", 0);

        // Acknowledge that Data Saver will be enabled.
        if (freProperties.getBoolean(FirstRunActivityBase.SHOW_DATA_REDUCTION_PAGE)) {
            int jumpCallCount = mTestObserver.jumpToPageCallback.getCallCount();
            clickButton(mLastActivity, R.id.next_button, "Failed to skip data saver");
            mTestObserver.jumpToPageCallback.waitForCallback(
                    "Failed to try moving to next screen", jumpCallCount);
        }

        // Select a default search engine.
        if (searchPromoType == LocaleManager.SearchEnginePromoType.DONT_SHOW) {
            Assert.assertFalse("Search engine page was shown.",
                    freProperties.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        } else {
            Assert.assertTrue("Search engine page wasn't shown.",
                    freProperties.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
            int jumpCallCount = mTestObserver.jumpToPageCallback.getCallCount();
            DefaultSearchEngineDialogHelperUtils.clickOnFirstEngine(
                    mLastActivity.findViewById(android.R.id.content));

            mTestObserver.jumpToPageCallback.waitForCallback(
                    "Failed to try moving to next screen", jumpCallCount);
        }

        // Don't sign in the user.
        if (freProperties.getBoolean(FirstRunActivityBase.SHOW_SIGNIN_PAGE)) {
            int jumpCallCount = mTestObserver.jumpToPageCallback.getCallCount();
            clickButton(mLastActivity, R.id.negative_button, "Failed to skip signing-in");
            mTestObserver.jumpToPageCallback.waitForCallback(
                    "Failed to try moving to next screen", jumpCallCount);
        }

        // FRE should be completed now, which will kick the user back into the interrupted flow.
        // In this case, the user gets sent to the ChromeTabbedActivity after a View Intent is
        // processed by ChromeLauncherActivity.
        mTestObserver.updateCachedEngineCallback.waitForCallback(
                "Failed to alert search widgets that an update is necessary", 0);
        waitForActivity(ChromeTabbedActivity.class);
    }

    @Test
    @MediumTest
    public void testExitFirstRunWithPolicy() {
        skipTosDialogViaPolicy();

        Intent intent =
                CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, "https://test.com");
        mContext.startActivity(intent);

        FirstRunActivity freActivity = waitForActivity(FirstRunActivity.class);
        CriteriaHelper.pollUiThread(
                () -> freActivity.getSupportFragmentManager().getFragments().size() > 0);
        // Make sure native is initialized so that the subseuqent transition doesn't get blocked.
        CriteriaHelper.pollUiThread((() -> freActivity.isNativeSideIsInitializedForTest()),
                "native never initialized.");

        waitForActivity(CustomTabActivity.class);
        Assert.assertFalse("Usage and crash reporting pref was set to true after skip",
                PrivacyPreferencesManagerImpl.getInstance()
                        .isUsageAndCrashReportingPermittedByUser());
        Assert.assertTrue(
                "FRE should be skipped for CCT.", FirstRunStatus.isFirstRunSkippedByPolicy());
    }

    @Test
    @MediumTest
    public void testFirstRunSkippedSharedPreferenceRefresh() {
        // Set that the first run was previous skipped by policy in shared preference, then
        // refreshing shared preference should cause its value to become false, since there's no
        // policy set in this test case.
        FirstRunStatus.setFirstRunSkippedByPolicy(true);

        DeferredStartupHandler.setExpectingActivityStartupForTesting();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, "about:blank");
        mContext.startActivity(intent);
        CustomTabActivity activity = waitForActivity(CustomTabActivity.class);
        CriteriaHelper.pollUiThread(() -> activity.didFinishNativeInitialization());

        // DeferredStartupHandler could not finish with CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL.
        // Use longer timeout here to avoid flakiness. See https://crbug.com/1157611.
        Assert.assertTrue("Deferred startup never completed",
                DeferredStartupHandler.waitForDeferredStartupCompleteForTesting(
                        ScalableTimeout.scaleTimeout(DEFERRED_START_UP_POLL_TIME)));

        // FirstRun status should be refreshed by TosDialogBehaviorSharedPrefInvalidator in deferred
        // start up task.
        CriteriaHelper.pollUiThread(() -> !FirstRunStatus.isFirstRunSkippedByPolicy());
    }

    @Test
    @MediumTest
    // TODO(https://crbug.com/1111490): Change this test case when policy can handle cases when ToS
    // is accepted in Browser App.
    public void testSkipTosPage_WithCctPolicy() throws Exception {
        skipTosDialogViaPolicy();
        FirstRunStatus.setSkipWelcomePage(true);

        Intent intent =
                CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, "https://test.com");
        mContext.startActivity(intent);

        FirstRunActivity freActivity = waitForActivity(FirstRunActivity.class);
        CriteriaHelper.pollUiThread(
                () -> freActivity.getSupportFragmentManager().getFragments().size() > 0);

        // A page skip should happen, while we are still staying at FRE.
        mTestObserver.jumpToPageCallback.waitForCallback("Welcome page should be skipped.", 0);
        Assert.assertFalse(
                "FRE should not be skipped for CCT.", FirstRunStatus.isFirstRunSkippedByPolicy());
        Assert.assertFalse(
                "FreActivity should still be alive.", freActivity.isActivityFinishingOrDestroyed());
    }

    @Test
    @MediumTest
    public void testFastDestroy() {
        // Inspired by crbug.com/1119548, where onDestroy() before triggerLayoutInflation() caused
        // a crash.
        Intent intent =
                CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, "https://test.com");
        mContext.startActivity(intent);
    }

    private void clickButton(final Activity activity, final int id, final String message) {
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(activity.findViewById(id), Matchers.notNullValue()));

        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            Button button = (Button) activity.findViewById(id);
            Assert.assertNotNull(message, button);
            button.performClick();
        });
    }
}
