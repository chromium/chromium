// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.when;

import android.accounts.Account;
import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.ColorDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.ProgressBar;

import androidx.annotation.IdRes;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.CollectionUtil;
import org.chromium.base.Promise;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.metrics.HistogramTestRule;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.enterprise.util.EnterpriseInfo;
import org.chromium.chrome.browser.enterprise.util.FakeEnterpriseInfo;
import org.chromium.chrome.browser.firstrun.FirstRunActivityTestObserver.ScopedObserverData;
import org.chromium.chrome.browser.firstrun.ToSAndUMAFirstRunFragment.Observer;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManagerDelegate;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.search_engines.DefaultSearchEngineDialogHelperUtils;
import org.chromium.chrome.browser.search_engines.SearchEnginePromoType;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.SigninFirstRunFragment;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.policy.AbstractAppRestrictionsProvider;
import org.chromium.components.policy.PolicyService;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentUrlConstants;

import java.util.ArrayList;
import java.util.Arrays;
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
@DoNotBatch(reason = "This test interacts with startup, native initialization, and first run.")
@CommandLineFlags.Add({ChromeSwitches.FORCE_DISABLE_SIGNIN_FRE})
public class FirstRunIntegrationTest {
    private static final String TEST_URL = "https://test.com";
    private static final String FOO_URL = "https://foo.com";
    private static final long ACTIVITY_WAIT_LONG_MS = TimeUnit.SECONDS.toMillis(10);
    private static final String TEST_ENROLLMENT_TOKEN = "enrollment-token";
    private static final String FRE_PROGRESS_VIEW_INTENT_HISTOGRAM =
            "MobileFre.Progress.ViewIntent";

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Rule
    public TestRule mCommandLineFlagsRule = CommandLineFlags.getTestRule();

    @Rule
    public HistogramTestRule mHistogramTestRule = new HistogramTestRule();

    @Mock
    private ExternalAuthUtils mExternalAuthUtilsMock;
    @Mock
    public FirstRunAppRestrictionInfo mMockAppRestrictionInfo;
    @Mock
    private AccountManagerFacade mAccountManagerFacade;
    @Mock
    private TemplateUrlService mTemplateUrlService;

    private List<Runnable> mTemplateUrlServiceWhenLoadedRunnables = new ArrayList<>();

    private TestFirstRunFlowSequencerDelegate mDelegate;

    private Promise<List<Account>> mAccountsPromise;

    private final Set<Class> mSupportedActivities =
            CollectionUtil.newHashSet(ChromeLauncherActivity.class, FirstRunActivity.class,
                    ChromeTabbedActivity.class, CustomTabActivity.class);
    private final Map<Class, ActivityMonitor> mMonitorMap = new HashMap<>();
    private Instrumentation mInstrumentation;
    private Context mContext;

    private FirstRunActivityTestObserver mTestObserver = new FirstRunActivityTestObserver();
    private Activity mLastActivity;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        // Only needs to be loaded once and needs to be loaded before HistogramTestRule.
        // TODO(https://crbug.com/1211884): Revise after HistogramTestRule is revised to not require
        // native loading.
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(false);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);
        FirstRunStatus.setFirstRunSkippedByPolicy(false);
        FirstRunUtils.setDisableDelayOnExitFreForTest(true);
        FirstRunActivity.setObserverForTest(mTestObserver);
        FirstRunActivityBase.setPolicyLoadListenerFactoryForTesting(null);
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
        AccountManagerFacadeProvider.resetInstanceForTests();
        FirstRunFlowSequencer.setDelegateForTesting(null);
        ToSAndUMAFirstRunFragment.setObserverForTesting(null);
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

    private void setTemplateUrlServiceForMock() {
        Mockito.doAnswer(invocation -> {
                   mTemplateUrlServiceWhenLoadedRunnables.add(invocation.getArgument(0));
                   return null;
               })
                .when(mTemplateUrlService)
                .runWhenLoaded(any());
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
    }

    private void replaceMockTemplateUrlServiceWithInitReal() {
        CriteriaHelper.pollUiThread(() -> {
            Assert.assertEquals(TemplateUrlServiceFactory.get(), mTemplateUrlService);
            TemplateUrlServiceFactory.setInstanceForTesting(null);
            TemplateUrlServiceFactory.get().runWhenLoaded(() -> {
                for (Runnable runnable : mTemplateUrlServiceWhenLoadedRunnables) {
                    runnable.run();
                }
            });
        });
    }

    private void skipTosDialogViaPolicy() {
        setHasAppRestrictionForMock(true);
        Bundle restrictions = new Bundle();
        restrictions.putInt("TosDialogBehavior", TosDialogBehavior.SKIP);
        AbstractAppRestrictionsProvider.setTestRestrictions(restrictions);

        FakeEnterpriseInfo fakeEnterpriseInfo = new FakeEnterpriseInfo();
        fakeEnterpriseInfo.initialize(new EnterpriseInfo.OwnedState(true, false));
        EnterpriseInfo.setInstanceForTest(fakeEnterpriseInfo);
    }

    private void setTosAccepted(boolean allowCrashUpload) {
        FirstRunStatus.setSkipWelcomePage(true);
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        sharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED, true);
        sharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER, allowCrashUpload);
    }

    private void enableCloudManagementViaPolicy() {
        setHasAppRestrictionForMock(true);
        Bundle restrictions = new Bundle();
        restrictions.putString("CloudManagementEnrollmentToken", TEST_ENROLLMENT_TOKEN);
        AbstractAppRestrictionsProvider.setTestRestrictions(restrictions);
    }

    private void launchCustomTabs(String url) {
        mContext.startActivity(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(mContext, url));
    }

    private void launchViewIntent(String url) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.setPackage(mContext.getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);
    }

    private void clickThroughFirstRun(
            FirstRunActivity firstRunActivity, FirstRunPagesTestCase testCase) throws Exception {
        // Start FRE.
        FirstRunNavigationHelper navigationHelper = new FirstRunNavigationHelper(firstRunActivity);
        navigationHelper.ensurePagesCreationSucceeded().acceptTermsOfService();

        if (testCase.searchPromoType() == SearchEnginePromoType.DONT_SHOW) {
            navigationHelper.ensureDefaultSearchEnginePromoNotCurrentPage();
        } else {
            navigationHelper.selectDefaultSearchEngine();
        }

        if (testCase.showSigninPromo()) {
            navigationHelper.skipSigninPromo();
        } else {
            navigationHelper.ensureSigninPromoNotCurrentPage();
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

    private void blockOnFlowIsKnown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull("mAccountsPromise is already initialized!", mAccountsPromise);
            mAccountsPromise = new Promise<>();
            // getCoreAccountInfos() is called by AccountTrackerService.seedAccounts();
            // TODO(https://crbug.com/1336704): Remove when account manager facade initiates
            //  seeding.
            Mockito.when(mAccountManagerFacade.getCoreAccountInfos()).thenReturn(new Promise<>());
        });
        Mockito.when(mAccountManagerFacade.getAccounts()).thenReturn(mAccountsPromise);
        AccountManagerFacadeProvider.setInstanceForTests(mAccountManagerFacade);
    }

    private void unblockOnFlowIsKnown() {
        Mockito.verify(mAccountManagerFacade, atLeastOnce()).getAccounts();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mAccountsPromise.fulfill(Collections.emptyList()));
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
    @DisabledTest(message = "https://crbug.com/1295396")
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
        TestThreadUtils.runOnUiThreadBlocking(mLastActivity::onBackPressed);
        scopedObserverData.abortFirstRunExperienceCallback.waitForCallback(
                "FirstRunActivity didn't abort", 0);

        CriteriaHelper.pollInstrumentationThread(() -> mLastActivity.isFinishing());

        // ChromeLauncherActivity should finish if FRE was aborted.
        CriteriaHelper.pollInstrumentationThread(() -> chromeLauncherActivity.isFinishing());
    }

    // TODO(https://crbug.com/1240516): Add test cases for the new Welcome screen that includes the
    // Sign-in promo once the sign-in components can be disabled by policy.

    // TODO(https://crbug.com/1254470): Add test cases for ToS page disabled by policy after the
    // user accepted ToS and aborted first run.

    @Test
    @MediumTest
    public void testFirstRunPages_NoCctPolicy_AbsenceOfPromos() throws Exception {
        runFirstRunPagesTest(new FirstRunPagesTestCase());
    }

    @Test
    @MediumTest
    public void testFirstRunPages_NoCctPolicy_SearchPromo() throws Exception {
        runFirstRunPagesTest(new FirstRunPagesTestCase().withSearchPromo());
    }

    @Test
    @MediumTest
    public void testFirstRunPages_NoCctPolicy_SearchPromo_SigninPromo() throws Exception {
        runFirstRunPagesTest(new FirstRunPagesTestCase().withSearchPromo().withSigninPromo());
    }

    @Test
    @MediumTest
    public void testFirstRunPages_NoCctPolicy_SigninPromo() throws Exception {
        runFirstRunPagesTest(new FirstRunPagesTestCase().withSigninPromo());
    }

    @Test
    @MediumTest
    public void testFirstRunPages_NoCctPolicy_OnBackPressed() throws Exception {
        initializePreferences(new FirstRunPagesTestCase().withSearchPromo().withSigninPromo());

        FirstRunActivity firstRunActivity = launchFirstRunActivity();

        // Go until the last page without skipping the last one, go back until initial page, and
        // then complete first run.
        new FirstRunNavigationHelper(firstRunActivity)
                .ensurePagesCreationSucceeded()
                .acceptTermsOfService()
                .selectDefaultSearchEngine()
                .ensureSigninPromoIsCurrentPage()
                .goBackToPreviousPage()
                .ensureDefaultSearchEnginePromoIsCurrentPage()
                .goBackToPreviousPage()
                .ensureTermsOfServiceIsCurrentPage()
                .acceptTermsOfService()
                .selectDefaultSearchEngine()
                .skipSigninPromo();

        waitForActivity(ChromeTabbedActivity.class);
    }

    @Test
    @MediumTest
    public void testFirstRunPages_WithCctPolicy_AbsenceOfPromos() throws Exception {
        runFirstRunPagesTest(new FirstRunPagesTestCase().withCctTosDisabled());
    }

    @Test
    @MediumTest
    public void testFirstRunPages_WithCctPolicy_SearchPromo() throws Exception {
        runFirstRunPagesTest(new FirstRunPagesTestCase().withCctTosDisabled().withSearchPromo());
    }

    @Test
    @MediumTest
    public void testFirstRunPages_WithCctPolicy_SearchPromo_SigninPromo() throws Exception {
        runFirstRunPagesTest(new FirstRunPagesTestCase()
                                     .withCctTosDisabled()
                                     .withSearchPromo()
                                     .withSigninPromo());
    }

    @Test
    @MediumTest
    public void testFirstRunPages_WithCctPolicy_SigninPromo() throws Exception {
        runFirstRunPagesTest(new FirstRunPagesTestCase().withCctTosDisabled().withSigninPromo());
    }

    @Test
    @MediumTest
    public void testFirstRunPages_WithCctPolicy_OnBackPressed() throws Exception {
        initializePreferences(new FirstRunPagesTestCase()
                                      .withCctTosDisabled()
                                      .withSearchPromo()
                                      .withSigninPromo());

        FirstRunActivity firstRunActivity = launchFirstRunActivity();

        // Go until the last page without skipping the last one, go back until initial page, and
        // then complete first run.
        new FirstRunNavigationHelper(firstRunActivity)
                .ensurePagesCreationSucceeded()
                .acceptTermsOfService()
                .selectDefaultSearchEngine()
                .ensureSigninPromoIsCurrentPage()
                .goBackToPreviousPage()
                .ensureDefaultSearchEnginePromoIsCurrentPage()
                .goBackToPreviousPage()
                .ensureTermsOfServiceIsCurrentPage()
                .acceptTermsOfService()
                .selectDefaultSearchEngine()
                .skipSigninPromo();

        waitForActivity(ChromeTabbedActivity.class);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Remove({ChromeSwitches.FORCE_DISABLE_SIGNIN_FRE})
    @CommandLineFlags.Add({ChromeSwitches.FORCE_ENABLE_SIGNIN_FRE})
    public void testSigninFirstRunPages_WithCctPolicy_AbsenceOfPromos() throws Exception {
        runFirstRunPagesTest(new FirstRunPagesTestCase().withCctTosDisabled());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Remove({ChromeSwitches.FORCE_DISABLE_SIGNIN_FRE})
    @CommandLineFlags.Add({ChromeSwitches.FORCE_ENABLE_SIGNIN_FRE})
    public void testSigninFirstRunPages_WithCctPolicy_SearchPromo() throws Exception {
        runFirstRunPagesTest(new FirstRunPagesTestCase().withCctTosDisabled().withSearchPromo());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Remove({ChromeSwitches.FORCE_DISABLE_SIGNIN_FRE})
    @CommandLineFlags.Add({ChromeSwitches.FORCE_ENABLE_SIGNIN_FRE})
    public void testSigninFirstRunPages_WithCctPolicy_SearchPromo_SigninPromo() throws Exception {
        runFirstRunPagesTest(new FirstRunPagesTestCase()
                                     .withCctTosDisabled()
                                     .withSearchPromo()
                                     .withSigninPromo());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Remove({ChromeSwitches.FORCE_DISABLE_SIGNIN_FRE})
    @CommandLineFlags.Add({ChromeSwitches.FORCE_ENABLE_SIGNIN_FRE})
    public void testSigninFirstRunPages_WithCctPolicy_SigninPromo() throws Exception {
        runFirstRunPagesTest(new FirstRunPagesTestCase().withCctTosDisabled().withSigninPromo());
    }

    private void runFirstRunPagesTest(FirstRunPagesTestCase testCase) throws Exception {
        initializePreferences(testCase);

        FirstRunActivity firstRunActivity = launchFirstRunActivity();
        clickThroughFirstRun(firstRunActivity, testCase);

        // FRE should be completed now, which will kick the user back into the interrupted flow.
        // In this case, the user gets sent to the ChromeTabbedActivity after a View Intent is
        // processed by ChromeLauncherActivity.
        getObserverData(firstRunActivity)
                .updateCachedEngineCallback.waitForCallback(
                        "Failed to alert search widgets that an update is necessary", 0);
        waitForActivity(ChromeTabbedActivity.class);
    }

    private void initializePreferences(FirstRunPagesTestCase testCase) throws Exception {
        if (testCase.cctTosDisabled()) skipTosDialogViaPolicy();
        if (testCase.isTosAccepted()) setTosAccepted(testCase.isUmaUploadAccepted());

        mDelegate = new TestFirstRunFlowSequencerDelegate(testCase);
        FirstRunFlowSequencer.setDelegateForTesting(mDelegate);

        setUpLocaleManagerDelegate(testCase.searchPromoType());
    }

    @Test
    @MediumTest
    public void testFirstRunPages_ProgressHistogramRecordedOnlyOnce() throws Exception {
        initializePreferences(new FirstRunPagesTestCase().withSearchPromo().withSigninPromo());

        FirstRunActivity firstRunActivity = launchFirstRunActivity();

        // Go until the last page without skipping the last one, go back until initial page, and
        // then complete first run.
        new FirstRunNavigationHelper(firstRunActivity)
                .ensurePagesCreationSucceeded()
                .acceptTermsOfService()
                .selectDefaultSearchEngine()
                .ensureSigninPromoIsCurrentPage()
                .goBackToPreviousPage()
                .ensureDefaultSearchEnginePromoIsCurrentPage()
                .goBackToPreviousPage()
                .ensureTermsOfServiceIsCurrentPage()
                .acceptTermsOfService()
                .selectDefaultSearchEngine()
                .skipSigninPromo();

        waitForActivity(ChromeTabbedActivity.class);

        checkRecordedProgressSteps(Arrays.asList(new Integer[] {MobileFreProgress.STARTED,
                MobileFreProgress.WELCOME_SHOWN, MobileFreProgress.SYNC_CONSENT_SHOWN,
                MobileFreProgress.SYNC_CONSENT_DISMISSED,
                MobileFreProgress.DEFAULT_SEARCH_ENGINE_SHOWN}));
    }

    @Test
    @MediumTest
    public void testFirstRunPages_ProgressHistogramRecording_NoPromos() throws Exception {
        initializePreferences(new FirstRunPagesTestCase());

        FirstRunActivity firstRunActivity = launchFirstRunActivity();

        new FirstRunNavigationHelper(firstRunActivity)
                .ensurePagesCreationSucceeded()
                .acceptTermsOfService();

        waitForActivity(ChromeTabbedActivity.class);

        checkRecordedProgressSteps(Arrays.asList(
                new Integer[] {MobileFreProgress.STARTED, MobileFreProgress.WELCOME_SHOWN}));
    }

    private void checkRecordedProgressSteps(List<Integer> bucketsRecorded) {
        for (int bucket = MobileFreProgress.STARTED; bucket < MobileFreProgress.MAX; ++bucket) {
            int recordedValue = RecordHistogram.getHistogramValueCountForTesting(
                    FRE_PROGRESS_VIEW_INTENT_HISTOGRAM, bucket);
            if (bucketsRecorded.contains(bucket)) {
                Assert.assertEquals(
                        String.format(
                                "Histogram <%s>, bucket <%d> should be recorded exactly once.",
                                FRE_PROGRESS_VIEW_INTENT_HISTOGRAM, bucket),
                        1, recordedValue);
            } else {
                Assert.assertEquals(
                        String.format("Histogram <%s>, bucket <%d> should not be recorded.",
                                FRE_PROGRESS_VIEW_INTENT_HISTOGRAM, bucket),
                        0, recordedValue);
            }
        }
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1221647")
    public void testExitFirstRunWithPolicy() throws Exception {
        initializePreferences(new FirstRunPagesTestCase().withCctTosDisabled());

        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(mContext, TEST_URL);
        mContext.startActivity(intent);

        FirstRunActivity freActivity = waitForActivity(FirstRunActivity.class);
        CriteriaHelper.pollUiThread(
                () -> freActivity.getSupportFragmentManager().getFragments().size() > 0);
        // Make sure native is initialized so that the subsequent transition is not blocked.
        CriteriaHelper.pollUiThread(
                (() -> freActivity.getNativeInitializationPromise().isFulfilled()),
                "native never initialized.");

        waitForActivity(CustomTabActivity.class);
        Assert.assertFalse("Usage and crash reporting pref was set to true after skip",
                PrivacyPreferencesManagerImpl.getInstance().isUsageAndCrashReportingPermitted());
        Assert.assertTrue(
                "FRE should be skipped for CCT.", FirstRunStatus.isFirstRunSkippedByPolicy());
    }

    @Test
    @MediumTest
    public void testFirstRunSkippedSharedPreferenceRefresh() throws Exception {
        // Set that the first run was previous skipped by policy in shared preference, then
        // refreshing shared preference should cause its value to become false, since there's no
        // policy set in this test case.
        FirstRunStatus.setFirstRunSkippedByPolicy(true);

        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
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
    @DisabledTest(message = "https://crbug.com/1331277")
    public void testSkipTosPage_WithCctPolicy() throws Exception {
        skipTosDialogViaPolicy();
        FirstRunStatus.setSkipWelcomePage(true);

        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(mContext, TEST_URL);
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
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(mContext, TEST_URL);
        mContext.startActivity(intent);
    }

    @Test
    @MediumTest
    public void testResetOnBackPress() throws Exception {
        testResetOnBackPressImpl(true);
    }

    @Test
    @MediumTest
    public void testResetOnBackPress_NoUmaAccepted() throws Exception {
        testResetOnBackPressImpl(false);
    }

    private void testResetOnBackPressImpl(boolean allowedCrashUpLoad) throws Exception {
        // Inspired by crbug.com/1192854.
        // When the policy initialization is finishing after ToS accepted, the small loading circle
        // will be shown on the screen. If user decide to go back with backpress, the UI should be
        // reset with ToS UI visible.
        FirstRunPagesTestCase testCase = new FirstRunPagesTestCase()
                                                 .withSigninPromo()
                                                 .withTosAlreadyAccepted()
                                                 .setUmaUploadAccepted(allowedCrashUpLoad);
        initializePreferences(testCase);

        // In this specific setup the policy loading call will be notified before ToS fragment is
        // finishing initialization, as FRE might attach to the PolicyLoadListener first. To make
        // sure no race condition happen, use TosAndUmaObserver to make sure the call is invoked.
        CallbackHelper tosPagePolicyLoadingListener = new CallbackHelper();
        ToSAndUMAFirstRunFragment.setObserverForTesting(new Observer() {
            @Override
            public void onNativeInitialized() {}

            @Override
            public void onPolicyServiceInitialized() {
                tosPagePolicyLoadingListener.notifyCalled();
            }

            @Override
            public void onHideLoadingUIComplete() {}
        });

        FirstRunActivity freActivity = launchFirstRunActivity();

        FirstRunNavigationHelper navHelper = new FirstRunNavigationHelper(freActivity)
                                                     .ensurePagesCreationSucceeded()
                                                     .ensureSigninPromoIsCurrentPage();
        tosPagePolicyLoadingListener.waitForFirst();
        navHelper.goBackToPreviousPage().ensureTermsOfServiceIsCurrentPage();

        View tosAndPrivacy = mLastActivity.findViewById(R.id.tos_and_privacy);
        CheckBox umaCheckbox = mLastActivity.findViewById(R.id.send_report_checkbox);
        Assert.assertNotNull("ToS should not be null.", tosAndPrivacy);
        Assert.assertNotNull("UMA Checkbox should not be null.", umaCheckbox);
        Assert.assertEquals("ToS should be visible.", View.VISIBLE, tosAndPrivacy.getVisibility());
        Assert.assertEquals(
                "UMA Checkbox should be visible.", View.VISIBLE, umaCheckbox.getVisibility());
        Assert.assertEquals(
                "UMA Checkbox state is different.", allowedCrashUpLoad, umaCheckbox.isChecked());
    }

    @Test
    @MediumTest
    public void testMultipleFresCustomIntoView() throws Exception {
        FirstRunPagesTestCase testCase = FirstRunPagesTestCase.createWithShowAllPromos();
        initializePreferences(testCase);

        launchCustomTabs(TEST_URL);
        FirstRunActivity firstFreActivity = waitForActivity(FirstRunActivity.class);

        launchViewIntent(FOO_URL);
        FirstRunActivity secondFreActivity = waitForDifferentFirstRunActivity(firstFreActivity);

        clickThroughFirstRun(secondFreActivity, testCase);
        verifyUrlEquals(FOO_URL, waitAndGetUriFromChromeActivity(ChromeTabbedActivity.class));
    }

    @Test
    @MediumTest
    public void testMultipleFresViewIntoCustom() throws Exception {
        FirstRunPagesTestCase testCase = FirstRunPagesTestCase.createWithShowAllPromos();
        initializePreferences(testCase);

        launchViewIntent(TEST_URL);
        FirstRunActivity firstFreActivity = waitForActivity(FirstRunActivity.class);

        launchCustomTabs(FOO_URL);
        FirstRunActivity secondFreActivity = waitForDifferentFirstRunActivity(firstFreActivity);

        clickThroughFirstRun(secondFreActivity, testCase);
        verifyUrlEquals(FOO_URL, waitAndGetUriFromChromeActivity(CustomTabActivity.class));
    }

    @Test
    @MediumTest
    public void testMultipleFresBothView() throws Exception {
        FirstRunPagesTestCase testCase = FirstRunPagesTestCase.createWithShowAllPromos();
        initializePreferences(testCase);

        launchViewIntent(TEST_URL);
        FirstRunActivity firstFreActivity = waitForActivity(FirstRunActivity.class);

        launchViewIntent(FOO_URL);
        FirstRunActivity secondFreActivity = waitForDifferentFirstRunActivity(firstFreActivity);

        clickThroughFirstRun(secondFreActivity, testCase);
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
        // This should block the FRE from showing any UI.
        blockOnFlowIsKnown();

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
        unblockOnFlowIsKnown();
        onDrawCallbackHelper.waitForCallback(0);
    }

    @Test
    @MediumTest
    public void testNativeInitBeforeFragment() throws Exception {
        FirstRunPagesTestCase testCase = new FirstRunPagesTestCase();
        initializePreferences(testCase);

        // Inspired by https://crbug.com/1207683 where a notification was dropped because native
        // initialized before the first fragment was attached to the activity.
        blockOnFlowIsKnown();

        launchViewIntent(TEST_URL);
        FirstRunActivity firstRunActivity = waitForActivity(FirstRunActivity.class);
        CriteriaHelper.pollUiThread(
                (() -> firstRunActivity.getNativeInitializationPromise().isFulfilled()),
                "native never initialized.");

        unblockOnFlowIsKnown();
        clickThroughFirstRun(firstRunActivity, testCase);
        verifyUrlEquals(TEST_URL, waitAndGetUriFromChromeActivity(ChromeTabbedActivity.class));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Remove({ChromeSwitches.FORCE_DISABLE_SIGNIN_FRE})
    @CommandLineFlags.Add({ChromeSwitches.FORCE_ENABLE_SIGNIN_FRE})
    public void testSigninFirstRunPageShownBeforeChildStatusFetch() throws Exception {
        // ChildAccountStatusSupplier uses AppRestrictions to quickly detect non-supervised cases,
        // so pretend there are AppRestrictions set by FamilyLink.
        setHasAppRestrictionForMock(true);
        blockOnFlowIsKnown();
        initializePreferences(new FirstRunPagesTestCase());

        FirstRunActivity firstRunActivity = launchFirstRunActivity();
        new FirstRunNavigationHelper(firstRunActivity).ensureTermsOfServiceIsCurrentPage();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ProgressBar progressBar =
                    ((SigninFirstRunFragment) firstRunActivity.getCurrentFragmentForTesting())
                            .getView()
                            .findViewById(R.id.fre_native_and_policy_load_progress_spinner);
            // Replace the progress bar with a dummy to allow other checks. Currently the
            // progress bar cannot be stopped otherwise due to some espresso issues (crbug/1115067).
            progressBar.setIndeterminateDrawable(
                    new ColorDrawable(SemanticColorUtils.getDefaultBgColor(firstRunActivity)));
        });

        onView(withId(R.id.fre_logo)).check(matches(isDisplayed()));
        onView(withId(R.id.fre_native_and_policy_load_progress_spinner))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Remove({ChromeSwitches.FORCE_DISABLE_SIGNIN_FRE})
    @CommandLineFlags.Add({ChromeSwitches.FORCE_ENABLE_SIGNIN_FRE})
    public void testSigninFirstRunLoadPointHistograms() throws Exception {
        initializePreferences(new FirstRunPagesTestCase());

        FirstRunActivity firstRunActivity = launchFirstRunActivity();
        new FirstRunNavigationHelper(firstRunActivity)
                .ensurePagesCreationSucceeded()
                .ensureTermsOfServiceIsCurrentPage();

        Assert.assertEquals("Child status fetch time not recorded", 1,
                mHistogramTestRule.getHistogramTotalCount(
                        "MobileFre.FromLaunch.ChildStatusAvailable"));
        Assert.assertEquals("Policies fetch time not recorded", 1,
                mHistogramTestRule.getHistogramTotalCount("MobileFre.FromLaunch.PoliciesLoaded"));
    }

    @Test
    @MediumTest
    public void testNativeInitBeforeFragmentSkip() throws Exception {
        skipTosDialogViaPolicy();
        blockOnFlowIsKnown();

        launchCustomTabs(TEST_URL);
        FirstRunActivity firstRunActivity = waitForActivity(FirstRunActivity.class);
        CriteriaHelper.pollUiThread(
                (() -> firstRunActivity.getNativeInitializationPromise().isFulfilled()),
                "native never initialized.");

        unblockOnFlowIsKnown();
        verifyUrlEquals(TEST_URL, waitAndGetUriFromChromeActivity(CustomTabActivity.class));
    }

    @Test
    @MediumTest
    public void testCloudManagementDoesNotBlockFirstRun() throws Exception {
        // Ensures FRE is not blocked if cloud management is enabled.
        FirstRunPagesTestCase testCase = FirstRunPagesTestCase.createWithShowAllPromos();
        initializePreferences(testCase);
        enableCloudManagementViaPolicy();

        launchViewIntent(TEST_URL);
        FirstRunActivity firstRunActivity = waitForActivity(FirstRunActivity.class);
        clickThroughFirstRun(firstRunActivity, testCase);
        verifyUrlEquals(TEST_URL, waitAndGetUriFromChromeActivity(ChromeTabbedActivity.class));
    }

    private void setUpLocaleManagerDelegate(@SearchEnginePromoType final int searchPromoType)
            throws Exception {
        // Force the LocaleManager into a specific state.
        LocaleManagerDelegate mockDelegate = new LocaleManagerDelegate() {
            @Override
            public int getSearchEnginePromoShowType() {
                return searchPromoType;
            }

            @Override
            public List<TemplateUrl> getSearchEnginesForPromoDialog(int promoType) {
                return TemplateUrlServiceFactory.get().getTemplateUrls();
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(
                () -> LocaleManager.getInstance().setDelegateForTest(mockDelegate));
    }

    @Test
    @MediumTest
    public void testPrefsUpdated_allPagesAlreadyShown() throws Exception {
        FirstRunPagesTestCase testCase =
                new FirstRunPagesTestCase().withSearchPromo().withSigninPromo();
        initializePreferences(testCase);

        FirstRunActivity firstRunActivity = launchFirstRunActivity();

        // Go until the last page without skipping the last one.
        FirstRunNavigationHelper navigationHelper = new FirstRunNavigationHelper(firstRunActivity)
                                                            .ensurePagesCreationSucceeded()
                                                            .acceptTermsOfService()
                                                            .selectDefaultSearchEngine()
                                                            .ensureSigninPromoIsCurrentPage();

        // Change preferences to disable all promos.
        testCase.setSearchPromoType(SearchEnginePromoType.DONT_SHOW);
        testCase.setSigninPromo(false);

        // Go back should skip all the promo pages and reach the terms of service page. Accepting
        // terms of service completes first run.
        navigationHelper.goBackToPreviousPage()
                .ensureTermsOfServiceIsCurrentPage()
                .acceptTermsOfService();

        waitForActivity(ChromeTabbedActivity.class);
    }

    @Test
    @MediumTest
    public void testPrefsUpdated_noPagesShown() throws Exception {
        FirstRunPagesTestCase testCase =
                new FirstRunPagesTestCase().withSearchPromo().withSigninPromo();
        initializePreferences(testCase);

        FirstRunActivity firstRunActivity = launchFirstRunActivity();

        // Show terms of services.
        FirstRunNavigationHelper navigationHelper = new FirstRunNavigationHelper(firstRunActivity)
                                                            .ensurePagesCreationSucceeded()
                                                            .ensureTermsOfServiceIsCurrentPage();

        // Change preferences before any promo page is shown.
        testCase.setSearchPromoType(SearchEnginePromoType.DONT_SHOW);
        testCase.setSigninPromo(false);

        // Accepting terms of services should complete first run, since all the promos are disabled.
        navigationHelper.acceptTermsOfService()
                .ensureDefaultSearchEnginePromoNotCurrentPage()
                .ensureSigninPromoNotCurrentPage();

        waitForActivity(ChromeTabbedActivity.class);
    }

    @Test
    @MediumTest
    public void testPrefsUpdated_searchEnginePromoDisableAfterPromoShown() throws Exception {
        FirstRunPagesTestCase testCase =
                new FirstRunPagesTestCase().withSearchPromo().withSigninPromo();
        initializePreferences(testCase);

        FirstRunActivity firstRunActivity = launchFirstRunActivity();

        // Go until the last page without skipping the last one.
        FirstRunNavigationHelper navigationHelper = new FirstRunNavigationHelper(firstRunActivity)
                                                            .ensurePagesCreationSucceeded()
                                                            .acceptTermsOfService()
                                                            .selectDefaultSearchEngine()
                                                            .ensureSigninPromoIsCurrentPage();

        // Disable search engine prompt after the next page is shown.
        testCase.setSearchPromoType(SearchEnginePromoType.DONT_SHOW);
        setUpLocaleManagerDelegate(SearchEnginePromoType.DONT_SHOW);

        // Go back until initial page, and
        // then complete first run. The search engine prompt shouldn't be shown again in either
        // direction.
        navigationHelper.goBackToPreviousPage()
                .ensureDefaultSearchEnginePromoNotCurrentPage()
                .acceptTermsOfService()
                .ensureDefaultSearchEnginePromoNotCurrentPage()
                .skipSigninPromo();

        waitForActivity(ChromeTabbedActivity.class);
    }

    @Test
    @MediumTest
    public void testPrefsUpdated_searchEnginePromoDisableWhilePromoShown() throws Exception {
        FirstRunPagesTestCase testCase =
                new FirstRunPagesTestCase().withSearchPromo().withSigninPromo();
        initializePreferences(testCase);

        FirstRunActivity firstRunActivity = launchFirstRunActivity();

        // Go over first run prompts and stop at the search engine page.
        FirstRunNavigationHelper navigationHelper =
                new FirstRunNavigationHelper(firstRunActivity)
                        .ensurePagesCreationSucceeded()
                        .acceptTermsOfService()
                        .ensureDefaultSearchEnginePromoIsCurrentPage();

        // Disable search engine prompt while it's shown. This will not hide the page.
        testCase.setSearchPromoType(SearchEnginePromoType.DONT_SHOW);
        setUpLocaleManagerDelegate(SearchEnginePromoType.DONT_SHOW);

        // Pass the search engine prompt, and move to the last page without skipping it.
        // Go back until initial page, and then complete first run. The search engine prompt
        // shouldn't be shown again in either direction.
        navigationHelper.selectDefaultSearchEngine()
                .ensureSigninPromoIsCurrentPage()
                .goBackToPreviousPage()
                .ensureDefaultSearchEnginePromoNotCurrentPage()
                .acceptTermsOfService()
                .ensureDefaultSearchEnginePromoNotCurrentPage()
                .skipSigninPromo();

        waitForActivity(ChromeTabbedActivity.class);
    }

    @Test
    @MediumTest
    public void testPrefsUpdated_signinPromoPromoDisableAfterPromoShown() throws Exception {
        FirstRunPagesTestCase testCase =
                new FirstRunPagesTestCase().withSearchPromo().withSigninPromo();
        initializePreferences(testCase);

        FirstRunActivity firstRunActivity = launchFirstRunActivity();

        // Go until the last page without skipping the last one.
        FirstRunNavigationHelper navigationHelper = new FirstRunNavigationHelper(firstRunActivity)
                                                            .ensurePagesCreationSucceeded()
                                                            .acceptTermsOfService()
                                                            .selectDefaultSearchEngine()
                                                            .ensureSigninPromoIsCurrentPage();

        // Disable sign-in prompt while it's shown. This will not hide the page.
        testCase.setSigninPromo(false);

        // Go back until initial page, and then complete first run. The sign-in prompt shouldn't be
        // shown again.
        navigationHelper.goBackToPreviousPage()
                .ensureDefaultSearchEnginePromoIsCurrentPage()
                .goBackToPreviousPage()
                .acceptTermsOfService()
                .selectDefaultSearchEngine();

        waitForActivity(ChromeTabbedActivity.class);
    }

    @Test
    @MediumTest
    public void testPrefsUpdated_signinPromoPromoDisableWhilePromoShown() throws Exception {
        FirstRunPagesTestCase testCase =
                new FirstRunPagesTestCase().withSearchPromo().withSigninPromo();
        initializePreferences(testCase);

        FirstRunActivity firstRunActivity = launchFirstRunActivity();

        // Go until the last page without skipping the last one.
        FirstRunNavigationHelper navigationHelper = new FirstRunNavigationHelper(firstRunActivity)
                                                            .ensurePagesCreationSucceeded()
                                                            .acceptTermsOfService()
                                                            .selectDefaultSearchEngine()
                                                            .ensureSigninPromoIsCurrentPage();

        // Disable sign-in prompt while it's shown. This will not hide the page.
        testCase.setSearchPromoType(SearchEnginePromoType.DONT_SHOW);

        // User should be able to interact with sign-in promo page and complete first run.
        navigationHelper.ensureSigninPromoIsCurrentPage().skipSigninPromo();

        waitForActivity(ChromeTabbedActivity.class);
    }

    @Test
    @MediumTest
    public void testSlowToInitTemplateUrlService() throws Exception {
        setTemplateUrlServiceForMock();
        initializePreferences(new FirstRunPagesTestCase().withSearchPromo());

        FirstRunActivity firstRunActivity = launchFirstRunActivity();
        FirstRunNavigationHelper navigationHelper = new FirstRunNavigationHelper(firstRunActivity)
                                                            .ensureHasPages()
                                                            .ensureTermsOfServiceIsCurrentPage();

        navigationHelper.clickBlockedTermsOfService();
        navigationHelper.ensureTermsOfServiceIsCurrentPage();

        replaceMockTemplateUrlServiceWithInitReal();
        navigationHelper.ensureDefaultSearchEnginePromoIsCurrentPage();

        navigationHelper.selectDefaultSearchEngine();
        waitForActivity(ChromeTabbedActivity.class);
    }

    /**
     * Inspired by http://crbug.com/1320171, covers the case when the user interacted with the UMA
     * checkbox before the policy service became available.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Remove({ChromeSwitches.FORCE_ENABLE_SIGNIN_FRE})
    @CommandLineFlags.Add({ChromeSwitches.FORCE_DISABLE_SIGNIN_FRE})
    public void testDelayedPolicyInitializationRespectsMetricsAndCrashReportingSelection()
            throws Exception {
        initializePreferences(new FirstRunPagesTestCase());

        DelayedPolicyLoadListenerFactory delayedPolicyLoadListenerFactory =
                new DelayedPolicyLoadListenerFactory();
        FirstRunActivityBase.setPolicyLoadListenerFactoryForTesting(
                delayedPolicyLoadListenerFactory);

        CallbackHelper onPolicyServiceInitializedCallback = new CallbackHelper();
        ToSAndUMAFirstRunFragment.setObserverForTesting(new Observer() {
            @Override
            public void onNativeInitialized() {}

            @Override
            public void onPolicyServiceInitialized() {
                onPolicyServiceInitializedCallback.notifyCalled();
            }

            @Override
            public void onHideLoadingUIComplete() {}
        });

        FirstRunActivity firstRunActivity = launchFirstRunActivity();
        FirstRunNavigationHelper helper =
                new FirstRunNavigationHelper(firstRunActivity).ensurePagesCreationSucceeded();

        helper.clickOnMetricsAndCrashReportingCheckbox();
        helper.ensureMetricsAndCrashReportingDisabled();

        int onPolicyServiceInitializedCallCount = onPolicyServiceInitializedCallback.getCallCount();
        ((DelayedPolicyLoadListener) delayedPolicyLoadListenerFactory.get()).runSavedCallback();
        onPolicyServiceInitializedCallback.waitForCallback(
                "onPolicyServiceInitialized expected to be called.",
                onPolicyServiceInitializedCallCount);
        helper.ensureMetricsAndCrashReportingDisabled();
    }

    private void clickButton(final Activity activity, final int id, final String message) {
        CriteriaHelper.pollUiThread(() -> {
            View view = activity.findViewById(id);
            Criteria.checkThat(view, Matchers.notNullValue());
            Criteria.checkThat(view.getVisibility(), Matchers.is(View.VISIBLE));
            Criteria.checkThat(view.isEnabled(), Matchers.is(true));
        });

        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            Button button = (Button) activity.findViewById(id);
            Assert.assertNotNull(message, button);
            button.performClick();
        });
    }

    /** Configuration for tests that depend on showing First Run pages. */
    static class FirstRunPagesTestCase {
        private boolean mCctTosDisabled;
        private @SearchEnginePromoType int mSearchPromoType = SearchEnginePromoType.DONT_SHOW;
        private boolean mShowSigninPromo;

        private boolean mIsTosAccepted;
        private boolean mIsUmaUploadAccepted;

        boolean cctTosDisabled() {
            return mCctTosDisabled;
        }

        @SearchEnginePromoType
        int searchPromoType() {
            return mSearchPromoType;
        }

        boolean showSearchPromo() {
            return mSearchPromoType == SearchEnginePromoType.SHOW_NEW
                    || mSearchPromoType == SearchEnginePromoType.SHOW_EXISTING;
        }

        boolean showSigninPromo() {
            return mShowSigninPromo;
        }

        boolean isTosAccepted() {
            return mIsTosAccepted;
        }

        boolean isUmaUploadAccepted() {
            return mIsUmaUploadAccepted;
        }

        FirstRunPagesTestCase setCctTosDisabled(boolean cctTosDisabled) {
            mCctTosDisabled = cctTosDisabled;
            return this;
        }

        FirstRunPagesTestCase setSearchPromoType(@SearchEnginePromoType int searchPromoType) {
            mSearchPromoType = searchPromoType;
            return this;
        }

        FirstRunPagesTestCase setSigninPromo(boolean showSigninPromo) {
            mShowSigninPromo = showSigninPromo;
            return this;
        }

        FirstRunPagesTestCase setTosAccepted(boolean tosAccepted) {
            mIsTosAccepted = tosAccepted;
            return this;
        }

        FirstRunPagesTestCase setUmaUploadAccepted(boolean umaUploadAccepted) {
            mIsUmaUploadAccepted = umaUploadAccepted;
            return this;
        }

        FirstRunPagesTestCase withCctTosDisabled() {
            return setCctTosDisabled(true);
        }

        FirstRunPagesTestCase withSearchPromo() {
            return setSearchPromoType(SearchEnginePromoType.SHOW_EXISTING);
        }

        FirstRunPagesTestCase withSigninPromo() {
            return setSigninPromo(true);
        }

        // Used assuming user has previously accepted ToS and move to the following pages.
        FirstRunPagesTestCase withTosAlreadyAccepted() {
            return setTosAccepted(true);
        }

        FirstRunPagesTestCase withUmaUploadAccepted() {
            return setUmaUploadAccepted(true);
        }

        static FirstRunPagesTestCase createWithShowAllPromos() {
            return new FirstRunPagesTestCase().withSearchPromo().withSigninPromo();
        }
    }

    /**
     * Performs basic navigation operations on First Run pages, such as checking if a given promo
     * is current shown, moving to the next page, or going back to the previous page.
     */
    class FirstRunNavigationHelper {
        private FirstRunActivity mFirstRunActivity;
        private ScopedObserverData mScopedObserverData;

        protected FirstRunNavigationHelper(FirstRunActivity firstRunActivity) {
            mFirstRunActivity = firstRunActivity;
            mScopedObserverData = getObserverData(mFirstRunActivity);
        }

        protected FirstRunNavigationHelper ensureNativeInitialized() throws Exception {
            CriteriaHelper.pollUiThread(()
                                                -> mFirstRunActivity.getLifecycleDispatcher()
                                                           .isNativeInitializationFinished());
            return this;
        }

        protected FirstRunNavigationHelper ensureHasPages() throws Exception {
            CriteriaHelper.pollUiThread(() -> mFirstRunActivity.hasPages());
            return this;
        }

        protected FirstRunNavigationHelper ensurePagesCreationSucceeded() throws Exception {
            mScopedObserverData.createPostNativeAndPoliciesPageSequenceCallback.waitForCallback(
                    "Failed to finalize the flow and create subsequent pages", 0);
            Assert.assertEquals("Search engine name should not have been set yet", 0,
                    mScopedObserverData.updateCachedEngineCallback.getCallCount());

            return this;
        }

        protected FirstRunNavigationHelper ensureTermsOfServiceIsCurrentPage() throws Exception {
            return waitForCurrentFragmentToMatch("Terms of Service should be the current page",
                    Matchers.either(Matchers.instanceOf(ToSAndUMAFirstRunFragment.class))
                            .or(Matchers.instanceOf(
                                    TosAndUmaFirstRunFragmentWithEnterpriseSupport.class))
                            .or(Matchers.instanceOf(SigninFirstRunFragment.class)));
        }

        protected FirstRunNavigationHelper ensureDefaultSearchEnginePromoIsCurrentPage() {
            return waitForCurrentFragmentToMatch("Search engine promo should be the current page",
                    Matchers.instanceOf(DefaultSearchEngineFirstRunFragment.class));
        }

        protected FirstRunNavigationHelper ensureDefaultSearchEnginePromoNotCurrentPage() {
            return waitForCurrentFragmentToMatch(
                    "Search engine promo shouldn't be the current page",
                    Matchers.not(Matchers.instanceOf(DefaultSearchEngineFirstRunFragment.class)));
        }

        protected FirstRunNavigationHelper ensureSigninPromoIsCurrentPage() {
            return waitForCurrentFragmentToMatch("Sign-in promo should be the current page",
                    Matchers.instanceOf(SyncConsentFirstRunFragment.class));
        }

        protected FirstRunNavigationHelper ensureSigninPromoNotCurrentPage() {
            return waitForCurrentFragmentToMatch("Sign-in promo shouldn't be the current page",
                    Matchers.not(Matchers.instanceOf(SyncConsentFirstRunFragment.class)));
        }

        protected FirstRunNavigationHelper acceptTermsOfService() throws Exception {
            ensureTermsOfServiceIsCurrentPage();

            int jumpCallCount = mScopedObserverData.jumpToPageCallback.getCallCount();
            int acceptCallCount = mScopedObserverData.acceptTermsOfServiceCallback.getCallCount();

            clickButton(mFirstRunActivity, getTermsOfServiceButtonIdRes(), "Failed to accept ToS");
            mScopedObserverData.jumpToPageCallback.waitForCallback(
                    "Failed to try moving to the next screen", jumpCallCount);
            mScopedObserverData.acceptTermsOfServiceCallback.waitForCallback(
                    "Failed to accept the ToS", acceptCallCount);

            return this;
        }

        protected FirstRunNavigationHelper clickBlockedTermsOfService() throws Exception {
            ensureTermsOfServiceIsCurrentPage();
            int jumpCallCount = mScopedObserverData.jumpToPageCallback.getCallCount();
            int acceptCallCount = mScopedObserverData.acceptTermsOfServiceCallback.getCallCount();

            clickButton(mFirstRunActivity, getTermsOfServiceButtonIdRes(), "Failed to accept ToS");
            // Cannot wait for a callback to be called, instead we want to verify the absence of
            // callbacks. Verify native is at least initialized, which includes a bounce to the UI
            // thread. This seems to be good enough to give things a chance to go wrong if they're
            // going to.
            ensureNativeInitialized();
            Assert.assertEquals("Unexpected move to next screen", jumpCallCount,
                    mScopedObserverData.jumpToPageCallback.getCallCount());
            Assert.assertEquals("Unexpected accept of the ToS", acceptCallCount,
                    mScopedObserverData.acceptTermsOfServiceCallback.getCallCount());

            return this;
        }

        protected FirstRunNavigationHelper selectDefaultSearchEngine() throws Exception {
            ensureDefaultSearchEnginePromoIsCurrentPage();

            int jumpCallCount = mScopedObserverData.jumpToPageCallback.getCallCount();
            DefaultSearchEngineDialogHelperUtils.clickOnFirstEngine(
                    mFirstRunActivity.findViewById(android.R.id.content));
            mScopedObserverData.jumpToPageCallback.waitForCallback(
                    "Failed trying to move past the search engine fragment", jumpCallCount);

            return this;
        }

        protected FirstRunNavigationHelper skipSigninPromo() throws Exception {
            ensureSigninPromoIsCurrentPage();

            int jumpCallCount = mScopedObserverData.jumpToPageCallback.getCallCount();
            clickButton(mFirstRunActivity, R.id.negative_button, "Failed to skip signing-in");
            mScopedObserverData.jumpToPageCallback.waitForCallback(
                    "Failed trying to move past the sign in fragment", jumpCallCount);

            return this;
        }

        protected FirstRunNavigationHelper goBackToPreviousPage() throws Exception {
            int jumpCallCount = mScopedObserverData.jumpToPageCallback.getCallCount();
            TestThreadUtils.runOnUiThreadBlocking(() -> mFirstRunActivity.onBackPressed());
            mScopedObserverData.jumpToPageCallback.waitForCallback(
                    "Failed go back to previous page", jumpCallCount);

            return this;
        }

        protected FirstRunNavigationHelper clickOnMetricsAndCrashReportingCheckbox()
                throws Exception {
            ensureTermsOfServiceIsCurrentPage();
            clickButton(mFirstRunActivity, R.id.send_report_checkbox,
                    "Failed to click on send report checkbox.");
            return this;
        }

        protected FirstRunNavigationHelper ensureMetricsAndCrashReportingDisabled() {
            CheckBox umaCheckbox = mLastActivity.findViewById(R.id.send_report_checkbox);
            Assert.assertNotNull("UMA checkbox should not be null.", umaCheckbox);
            CriteriaHelper.pollUiThread(
                    () -> !umaCheckbox.isChecked(), "UMA reporting should be disabled.");
            return this;
        }

        protected FirstRunNavigationHelper waitForCurrentFragmentToMatch(
                String failureReason, Matcher<Object> matcher) {
            CriteriaHelper.pollUiThread(
                    ()
                            -> matcher.matches(mFirstRunActivity.getCurrentFragmentForTesting()),
                    failureReason);
            return this;
        }

        private @IdRes int getTermsOfServiceButtonIdRes() {
            return Matchers.instanceOf(SigninFirstRunFragment.class)
                            .matches(mFirstRunActivity.getCurrentFragmentForTesting())
                    ? R.id.signin_fre_continue_button
                    : R.id.terms_accept;
        }
    }

    /**
     * Overrides the default {@link FirstRunFlowSequencer}'s delegate to make decisions on
     * showing/skipping promo pages based on the current {@link FirstRunPagesTestCase}.
     */
    private static class TestFirstRunFlowSequencerDelegate
            extends FirstRunFlowSequencer.FirstRunFlowSequencerDelegate {
        private FirstRunPagesTestCase mTestCase;

        public TestFirstRunFlowSequencerDelegate(FirstRunPagesTestCase testCase) {
            mTestCase = testCase;
        }

        @Override
        public boolean shouldShowSyncConsentPage(
                Activity activity, List<Account> accounts, boolean isChild) {
            return mTestCase.showSigninPromo();
        }

        @Override
        public boolean shouldShowSearchEnginePage() {
            return mTestCase.showSearchPromo();
        }
    }

    /**
     * Fake {@link PolicyLoadListener} that captures invocations of {@code
     * PolicyLoadListener#onAvailable} and delays them to until {@link runSavedCallback} is called.
     */
    private static class DelayedPolicyLoadListener extends PolicyLoadListener {
        private List<Callback<Boolean>> mSavedCallbacks = new ArrayList<>();

        public DelayedPolicyLoadListener(FirstRunAppRestrictionInfo appRestrictionInfo,
                OneshotSupplier<PolicyService> policyServiceSupplier) {
            super(appRestrictionInfo, policyServiceSupplier);
        }

        @Override
        public Boolean onAvailable(Callback<Boolean> callback) {
            mSavedCallbacks.add(callback);
            return null;
        }

        /** Fires all callbacks saved in {@link mSavedCallbacks}. */
        public void runSavedCallback() {
            mSavedCallbacks.forEach(callback
                    -> TestThreadUtils.runOnUiThreadBlocking(() -> callback.onResult(true)));
        }
    }

    /**
     * Allows injection of {@link DelayedPolicyLoadListener} into {@link
     * ToSAndUMAFirstRunFragment}.
     */
    private class DelayedPolicyLoadListenerFactory
            implements FirstRunActivityBase.PolicyLoadListenerFactory {
        private PolicyLoadListener mInjectedPolicyLoadListener;

        @Override
        public PolicyLoadListener inject(FirstRunAppRestrictionInfo appRestrictionInfo,
                OneshotSupplier<PolicyService> policyServiceSupplier) {
            mInjectedPolicyLoadListener =
                    new DelayedPolicyLoadListener(appRestrictionInfo, policyServiceSupplier);
            return mInjectedPolicyLoadListener;
        }

        public PolicyLoadListener get() {
            return mInjectedPolicyLoadListener;
        }
    }
}
