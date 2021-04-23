// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.hamcrest.Matchers.is;
import static org.mockito.ArgumentMatchers.any;

import android.accounts.Account;
import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.view.View;
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
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.CollectionUtil;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.firstrun.FirstRunActivityTestObserver.ScopedObserverData;
import org.chromium.chrome.browser.locale.DefaultSearchEngineDialogHelperUtils;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManager.SearchEnginePromoType;
import org.chromium.chrome.browser.policy.EnterpriseInfo;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.MultiActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.policy.AbstractAppRestrictionsProvider;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentUrlConstants;

import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Integration test suite for the first run experience.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class FirstRunIntegrationTest {
    private static final String TEST_URL = "https://test.com";
    private static final String FOO_URL = "https://foo.com";
    private static final long ACTIVITY_WAIT_LONG_MS = TimeUnit.SECONDS.toMillis(10);

    @Rule
    public MultiActivityTestRule mTestRule = new MultiActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    public FirstRunAppRestrictionInfo mMockAppRestrictionInfo;
    @Mock
    public EnterpriseInfo mEnterpriseInfo;
    @Mock
    private AccountManagerFacade mAccountManagerFacade;

    @Captor
    private ArgumentCaptor<Callback<List<Account>>> mGetGoogleAccountsCaptor;

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
        // Tear down the last activity first, otherwise the other cleanup, in particular skipped by
        // policy pref, might trigger an assert in activity initialization because of the statics
        // we reset below. Run it on UI so there are no threading issues.
        if (mLastActivity != null) {
            TestThreadUtils.runOnUiThreadBlocking(() -> mLastActivity.finish());
        }

        FirstRunStatus.setFirstRunSkippedByPolicy(false);
        FirstRunUtils.setDisableDelayOnExitFreForTest(false);
        FirstRunAppRestrictionInfo.setInitializedInstanceForTest(null);
        ToSAndUMAFirstRunFragment.setShowUmaCheckBoxForTesting(false);
        EnterpriseInfo.setInstanceForTest(null);
    }

    private ActivityMonitor getMonitor(Class activityClass) {
        Assert.assertTrue(mSupportedActivities.contains(activityClass));
        return mMonitorMap.get(activityClass);
    }

    private FirstRunActivity launchFirstRunActivity() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(TEST_URL));
        intent.setPackage(mContext.getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);

        // Because the AsyncInitializationActivity notices that the FRE hasn't been run yet, it
        // redirects to it.  Once the user closes the FRE, the user should be kicked back into the
        // startup flow where they were interrupted.
        return waitForActivity(FirstRunActivity.class);
    }

    private <T extends Activity> T waitForActivity(Class<T> activityClass) {
        Assert.assertTrue(mSupportedActivities.contains(activityClass));
        ActivityMonitor monitor = getMonitor(activityClass);
        mLastActivity = mInstrumentation.waitForMonitorWithTimeout(monitor, ACTIVITY_WAIT_LONG_MS);
        Assert.assertNotNull("Could not find " + activityClass.getName(), mLastActivity);
        return (T) mLastActivity;
    }

    private void setHasAppRestrictionForMock(boolean hasAppRestriction) {
        Mockito.doAnswer(invocation -> {
                   Callback<Boolean> callback = invocation.getArgument(0);
                   callback.onResult(hasAppRestriction);
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
        setHasAppRestrictionForMock(true);
        Bundle restrictions = new Bundle();
        restrictions.putInt("TosDialogBehavior", TosDialogBehavior.SKIP);
        AbstractAppRestrictionsProvider.setTestRestrictions(restrictions);
        setDeviceOwnedForMock();
    }

    private void launchCustomTabs(String url) {
        mContext.startActivity(CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, url));
    }

    private void launchViewIntent(String url) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.setPackage(mContext.getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);
    }

    private void clickThroughFirstRun(FirstRunActivity firstRunActivity,
            @SearchEnginePromoType final int searchPromoType) throws Exception {
        ScopedObserverData scopedObserverData = getObserverData(firstRunActivity);
        scopedObserverData.createPostNativeAndPoliciesPageSequenceCallback.waitForCallback(
                "Failed to finalize the flow and create subsequent pages", 0);
        Bundle freProperties = scopedObserverData.freProperties;
        Assert.assertEquals("Search engine name should not have been set yet", 0,
                scopedObserverData.updateCachedEngineCallback.getCallCount());

        // Accept the ToS.
        clickButton(firstRunActivity, R.id.terms_accept, "Failed to accept ToS");
        scopedObserverData.jumpToPageCallback.waitForCallback(
                "Failed to try moving to the next screen", 0);
        scopedObserverData.acceptTermsOfServiceCallback.waitForCallback(
                "Failed to accept the ToS", 0);

        // Acknowledge that Data Saver will be enabled.
        if (freProperties.getBoolean(FirstRunActivityBase.SHOW_DATA_REDUCTION_PAGE)) {
            int jumpCallCount = scopedObserverData.jumpToPageCallback.getCallCount();
            clickButton(firstRunActivity, R.id.next_button, "Failed to skip data saver");
            scopedObserverData.jumpToPageCallback.waitForCallback(
                    "Failed try to move past the data saver fragment", jumpCallCount);
        }

        // Select a default search engine.
        if (searchPromoType == LocaleManager.SearchEnginePromoType.DONT_SHOW) {
            Assert.assertFalse("Search engine page was shown.",
                    freProperties.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        } else {
            Assert.assertTrue("Search engine page wasn't shown.",
                    freProperties.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
            int jumpCallCount = scopedObserverData.jumpToPageCallback.getCallCount();
            DefaultSearchEngineDialogHelperUtils.clickOnFirstEngine(
                    firstRunActivity.findViewById(android.R.id.content));

            scopedObserverData.jumpToPageCallback.waitForCallback(
                    "Failed trying to move past the search engine fragment", jumpCallCount);
        }

        // Don't sign in the user.
        if (freProperties.getBoolean(FirstRunActivityBase.SHOW_SIGNIN_PAGE)) {
            int jumpCallCount = scopedObserverData.jumpToPageCallback.getCallCount();
            clickButton(firstRunActivity, R.id.negative_button, "Failed to skip signing-in");
            scopedObserverData.jumpToPageCallback.waitForCallback(
                    "Failed trying to move past the sign in fragment", jumpCallCount);
        }
    }

    private void verifyUrlEquals(String expected, Uri actual) {
        Assert.assertEquals("Expected " + expected + " did not match actual " + actual,
                Uri.parse(expected), actual);
    }

    /**
     * When launching a second Chrome, the new FRE should replace the old FRE. In order to know when
     * the second FirstRunActivity is ready, use object inequality with old one.
     * @param previousFreActivity The previous activity.
     */
    private FirstRunActivity waitForDifferentFirstRunActivity(
            FirstRunActivity previousFreActivity) {
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> {
                    for (Activity runningActivity : ApplicationStatus.getRunningActivities()) {
                        @ActivityState
                        int state = ApplicationStatus.getStateForActivity(runningActivity);
                        if (runningActivity.getClass() == FirstRunActivity.class
                                && runningActivity != previousFreActivity
                                && (state == ActivityState.STARTED
                                        || state == ActivityState.RESUMED)) {
                            mLastActivity = runningActivity;
                            return true;
                        }
                    }
                    return false;
                },
                "Did not find a different FirstRunActivity from " + previousFreActivity,
                /*maxTimeoutMs*/ ACTIVITY_WAIT_LONG_MS,
                /*checkIntervalMs*/ CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        CriteriaHelper.pollInstrumentationThread(()
                                                         -> previousFreActivity.isFinishing(),
                "The original FirstRunActivity should be finished, instead "
                        + ApplicationStatus.getStateForActivity(previousFreActivity));
        return (FirstRunActivity) mLastActivity;
    }

    private <T extends ChromeActivity> Uri waitAndGetUriFromChromeActivity(Class<T> activityClass)
            throws Exception {
        ChromeActivity chromeActivity = waitForActivity(activityClass);
        return chromeActivity.getIntent().getData();
    }

    private ScopedObserverData getObserverData(FirstRunActivity freActivity) {
        return mTestObserver.getScopedObserverData(freActivity);
    }

    @Test
    @SmallTest
    public void testHelpPageSkipsFirstRun() {
        // Fire an Intent to load a generic URL.
        CustomTabActivity.showInfoPage(mContext, TEST_URL);

        // The original activity should be started because it's a "help page".
        waitForActivity(CustomTabActivity.class);
        Assert.assertFalse(mLastActivity.isFinishing());

        // First run should be skipped for this Activity.
        Assert.assertEquals(0, getMonitor(FirstRunActivity.class).getHits());
    }

    @Test
    @SmallTest
    public void testAbortFirstRun() throws Exception {
        launchViewIntent(TEST_URL);
        Activity chromeLauncherActivity = waitForActivity(ChromeLauncherActivity.class);

        // Because the ChromeLauncherActivity notices that the FRE hasn't been run yet, it
        // redirects to it.
        FirstRunActivity firstRunActivity = waitForActivity(FirstRunActivity.class);

        // Once the user closes the FRE, the user should be kicked back into the
        // startup flow where they were interrupted.
        ScopedObserverData scopedObserverData = getObserverData(firstRunActivity);
        Assert.assertEquals(0, scopedObserverData.abortFirstRunExperienceCallback.getCallCount());
        mLastActivity.onBackPressed();
        scopedObserverData.abortFirstRunExperienceCallback.waitForCallback(
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

        FirstRunActivity firstRunActivity = launchFirstRunActivity();

        clickThroughFirstRun(firstRunActivity, searchPromoType);

        // FRE should be completed now, which will kick the user back into the interrupted flow.
        // In this case, the user gets sent to the ChromeTabbedActivity after a View Intent is
        // processed by ChromeLauncherActivity.
        getObserverData(firstRunActivity)
                .updateCachedEngineCallback.waitForCallback(
                        "Failed to alert search widgets that an update is necessary", 0);
        waitForActivity(ChromeTabbedActivity.class);
    }

    @Test
    @MediumTest
    public void testExitFirstRunWithPolicy() {
        skipTosDialogViaPolicy();

        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, TEST_URL);
        mContext.startActivity(intent);

        FirstRunActivity freActivity = waitForActivity(FirstRunActivity.class);
        CriteriaHelper.pollUiThread(
                () -> freActivity.getSupportFragmentManager().getFragments().size() > 0);
        // Make sure native is initialized so that the subsequent transition is not blocked.
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

        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                mContext, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        mContext.startActivity(intent);
        CustomTabActivity activity = waitForActivity(CustomTabActivity.class);
        CriteriaHelper.pollUiThread(() -> activity.didFinishNativeInitialization());

        // DeferredStartupHandler could not finish with CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL.
        // Use longer timeout here to avoid flakiness. See https://crbug.com/1157611.
        CriteriaHelper.pollUiThread(() -> activity.deferredStartupPostedForTesting());
        Assert.assertTrue("Deferred startup never completed",
                DeferredStartupHandler.waitForDeferredStartupCompleteForTesting(
                        ScalableTimeout.scaleTimeout(ACTIVITY_WAIT_LONG_MS)));

        // FirstRun status should be refreshed by TosDialogBehaviorSharedPrefInvalidator in deferred
        // start up task.
        CriteriaHelper.pollUiThread(() -> !FirstRunStatus.isFirstRunSkippedByPolicy());
    }

    @Test
    @MediumTest
    public void testSkipTosPage() throws TimeoutException {
        // Test case that verifies when the ToS Page was previously accepted, launching the FRE
        // should transition to the next page.
        FirstRunStatus.setSkipWelcomePage(true);

        FirstRunActivity freActivity = launchFirstRunActivity();
        CriteriaHelper.pollUiThread(
                () -> freActivity.getSupportFragmentManager().getFragments().size() > 0);

        getObserverData(freActivity)
                .jumpToPageCallback.waitForCallback("Welcome page should be skipped.", 0);
    }

    @Test
    @MediumTest
    // TODO(https://crbug.com/1111490): Change this test case when policy can handle cases when ToS
    // is accepted in Browser App.
    public void testSkipTosPage_WithCctPolicy() throws Exception {
        skipTosDialogViaPolicy();
        FirstRunStatus.setSkipWelcomePage(true);

        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, TEST_URL);
        mContext.startActivity(intent);

        FirstRunActivity freActivity = waitForActivity(FirstRunActivity.class);
        CriteriaHelper.pollUiThread(
                () -> freActivity.getSupportFragmentManager().getFragments().size() > 0);

        // A page skip should happen, while we are still staying at FRE.
        getObserverData(freActivity)
                .jumpToPageCallback.waitForCallback("Welcome page should be skipped.", 0);
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
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, TEST_URL);
        mContext.startActivity(intent);
    }

    @Test
    @MediumTest
    public void testResetOnBackPress() throws Exception {
        // Inspired by crbug.com/1192854.
        // When the policy initialization is finishing after ToS accepted, the small loading circle
        // will be shown on the screen. If user decide to go back with backpress, the UI should be
        // reset with ToS UI visible.
        FirstRunStatus.setSkipWelcomePage(true);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED, true);
        setHasAppRestrictionForMock(false);
        FirstRunActivity freActivity = launchFirstRunActivity();

        // ToS page should be skipped and jumped to the next page, since ToS is marked accepted in
        // shared preference.
        ScopedObserverData scopedObserverData = getObserverData(freActivity);
        scopedObserverData.createPostNativeAndPoliciesPageSequenceCallback.waitForCallback(
                "Failed to finalize the flow.", 0);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(freActivity.isNativeSideIsInitializedForTest(), is(true)));
        scopedObserverData.jumpToPageCallback.waitForCallback(
                "ToS should be skipped after native initialized.", 0);

        // Press back button.
        TestThreadUtils.runOnUiThreadBlocking(() -> mLastActivity.onBackPressed());

        View tosAndPrivacy = mLastActivity.findViewById(R.id.tos_and_privacy);
        View umaCheckbox = mLastActivity.findViewById(R.id.send_report_checkbox);
        Assert.assertNotNull(tosAndPrivacy);
        Assert.assertNotNull(umaCheckbox);
        Assert.assertEquals(View.VISIBLE, tosAndPrivacy.getVisibility());
        Assert.assertEquals(View.VISIBLE, umaCheckbox.getVisibility());
    }

    @Test
    @MediumTest
    public void testMultipleFresCustomIntoView() throws Exception {
        launchCustomTabs(TEST_URL);
        FirstRunActivity firstFreActivity = waitForActivity(FirstRunActivity.class);

        launchViewIntent(FOO_URL);
        FirstRunActivity secondFreActivity = waitForDifferentFirstRunActivity(firstFreActivity);

        clickThroughFirstRun(secondFreActivity, LocaleManager.SearchEnginePromoType.DONT_SHOW);
        verifyUrlEquals(FOO_URL, waitAndGetUriFromChromeActivity(ChromeTabbedActivity.class));
    }

    @Test
    @MediumTest
    public void testMultipleFresViewIntoCustom() throws Exception {
        launchViewIntent(TEST_URL);
        FirstRunActivity firstFreActivity = waitForActivity(FirstRunActivity.class);

        launchCustomTabs(FOO_URL);
        FirstRunActivity secondFreActivity = waitForDifferentFirstRunActivity(firstFreActivity);

        clickThroughFirstRun(secondFreActivity, LocaleManager.SearchEnginePromoType.DONT_SHOW);
        verifyUrlEquals(FOO_URL, waitAndGetUriFromChromeActivity(CustomTabActivity.class));
    }

    @Test
    @MediumTest
    public void testMultipleFresBothView() throws Exception {
        launchViewIntent(TEST_URL);
        FirstRunActivity firstFreActivity = waitForActivity(FirstRunActivity.class);

        launchViewIntent(FOO_URL);
        FirstRunActivity secondFreActivity = waitForDifferentFirstRunActivity(firstFreActivity);

        clickThroughFirstRun(secondFreActivity, LocaleManager.SearchEnginePromoType.DONT_SHOW);
        verifyUrlEquals(FOO_URL, waitAndGetUriFromChromeActivity(ChromeTabbedActivity.class));
    }

    @Test
    @MediumTest
    public void testMultipleFresBackButton() throws Exception {
        launchViewIntent(TEST_URL);
        FirstRunActivity firstFreActivity = waitForActivity(FirstRunActivity.class);

        launchViewIntent(TEST_URL);
        FirstRunActivity secondFreActivity = waitForDifferentFirstRunActivity(firstFreActivity);

        ScopedObserverData secondFreData = getObserverData(secondFreActivity);
        Assert.assertEquals("Second FRE should not have aborted before back button is pressed.", 0,
                secondFreData.abortFirstRunExperienceCallback.getCallCount());

        secondFreActivity.onBackPressed();
        secondFreData.abortFirstRunExperienceCallback.waitForCallback(
                "Second FirstRunActivity didn't abort", 0);
        CriteriaHelper.pollInstrumentationThread(
                () -> secondFreActivity.isFinishing(), "Second FRE should be finishing now.");
    }

    @Test
    @MediumTest
    public void testInitialDrawBlocked() throws Exception {
        // This should block the FRE from showing any UI, as #onFlowIsKnown will not be called.
        AccountManagerFacadeProvider.setInstanceForTests(mAccountManagerFacade);

        launchViewIntent(TEST_URL);
        FirstRunActivity firstRunActivity = waitForActivity(FirstRunActivity.class);

        // Wait for the activity to initialize views or else later find call will NPE.
        CriteriaHelper.pollUiThread(() -> firstRunActivity.findViewById(R.id.fre_pager) != null);

        CallbackHelper onDrawCallbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            View frePager = firstRunActivity.findViewById(R.id.fre_pager);
            Assert.assertNotNull(frePager);
            frePager.getViewTreeObserver().addOnDrawListener(onDrawCallbackHelper::notifyCalled);
        });

        // Wait for a second to ensure Android would have had time to do a draw pass if it was ever
        // going to.
        Thread.sleep(1000);
        Assert.assertEquals(0, onDrawCallbackHelper.getCallCount());

        // Now return account status which should result in both the first fragment being generated,
        // and the first draw call being let happen.
        Mockito.verify(mAccountManagerFacade)
                .tryGetGoogleAccounts(mGetGoogleAccountsCaptor.capture());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mGetGoogleAccountsCaptor.getValue().onResult(Collections.emptyList()));
        onDrawCallbackHelper.waitForCallback(0);
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
