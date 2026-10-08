// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.CustomizationProviderDelegateType.G_SERVICE;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.CustomizationProviderDelegateType.PRELOAD_APK;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.PartnerCustomizationsHomepageEnum.NTP_CORRECTLY;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.PartnerCustomizationsHomepageEnum.NTP_INCORRECTLY;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.PartnerCustomizationsHomepageEnum.NTP_UNKNOWN;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.PartnerCustomizationsHomepageEnum.OTHER_CUSTOM_HOMEPAGE;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.PartnerCustomizationsHomepageEnum.PARTNER_CUSTOM_HOMEPAGE;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.TaskCompletion.CANCELLED;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.TaskCompletion.COMPLETED_IN_TIME;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.TaskCompletion.COMPLETED_TOO_LATE;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.TaskCompletion.EXCEPTION;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.TaskCompletion.TASK_SKIPPED;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.delegateName;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;

import android.os.SystemClock;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.shadows.ShadowLog.LogItem;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.Log;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsTestUtils.HomepageCharacterizationHelperStub;
import org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.CustomizationProviderDelegateType;
import org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.PartnerCustomizationsHomepageEnum;
import org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.TaskCompletion;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.List;
import java.util.Locale;
import java.util.function.Supplier;

/** Unit tests for {@link PartnerCustomizationsUma}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PartnerCustomizationsUmaUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcherMock;

    @Captor private ArgumentCaptor<LifecycleObserver> mLifeCycleObserverCaptor;

    private static final String LOG_TAG = Log.normalizeTag("PartnerCustUma");
    private static final @CustomizationProviderDelegateType int SOME_DELEGATE = G_SERVICE;

    private static final int START_TIME = 700;

    private static final boolean NOT_CACHED = false;
    private static final boolean CACHED = true;

    private static final String NTP_URL = getOriginalNativeNtpUrl();
    private static final String NON_NTP_URL = "https://www.google.com/";

    private static final Supplier<HomepageCharacterizationHelper> HELPER_FOR_NTP =
            HomepageCharacterizationHelperStub::ntpHelper;

    private static final Supplier<HomepageCharacterizationHelper> HELPER_FOR_PARTNER_NON_NTP =
            HomepageCharacterizationHelperStub::nonNtpHelper;

    private PartnerCustomizationsUma mPartnerCustomizationsUma;

    private boolean mDidCall;

    @Before
    public void setUp() {
        ShadowLog.reset();
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL);
        PartnerCustomizationsUma.resetStaticsForTesting();
        mPartnerCustomizationsUma = new PartnerCustomizationsUma();
    }

    @After
    public void tearDown() {
        ShadowLog.reset();
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL);
        PartnerCustomizationsUma.resetStaticsForTesting();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.PARTNER_CUSTOMIZATIONS_UMA)
    public void testIsEnabled() {
        Assert.assertTrue(PartnerCustomizationsUma.isEnabled());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.PARTNER_CUSTOMIZATIONS_UMA)
    public void testIsEnabled_false() {
        Assert.assertFalse(PartnerCustomizationsUma.isEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.PARTNER_CUSTOMIZATIONS_UMA)
    public void testOnFinishNativeInitializationEnabled_alreadyEnabled() {
        mPartnerCustomizationsUma.onFinishNativeInitializationOrEnabled(
                mActivityLifecycleDispatcherMock, () -> mDidCall = true);
        verifyNoInteractions(mActivityLifecycleDispatcherMock);
        Assert.assertTrue(mDidCall);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.PARTNER_CUSTOMIZATIONS_UMA)
    public void testOnFinishNativeInitializationEnabled_alreadyDisabled() {
        mPartnerCustomizationsUma.onFinishNativeInitializationOrEnabled(
                mActivityLifecycleDispatcherMock, () -> mDidCall = true);
        verifyNoInteractions(mActivityLifecycleDispatcherMock);
        Assert.assertFalse(mDidCall);
    }

    private NativeInitObserver captureObserverFromLifecycleMock() {
        // Set up the dispatcher mock to capture the observer
        verify(mActivityLifecycleDispatcherMock, times(1))
                .register(mLifeCycleObserverCaptor.capture());
        NativeInitObserver observer = (NativeInitObserver) mLifeCycleObserverCaptor.getValue();
        return observer;
    }

    @Test
    public void testOnFinishNativeInitializationEnabled_beforeNativeInit() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureOverrides.removeAllIncludingAnnotations();
        mPartnerCustomizationsUma.onFinishNativeInitializationOrEnabled(
                mActivityLifecycleDispatcherMock, () -> mDidCall = true);
        NativeInitObserver observer = captureObserverFromLifecycleMock();
        FeatureOverrides.enable(ChromeFeatureList.PARTNER_CUSTOMIZATIONS_UMA);
        Assert.assertFalse(
                "Expected onFinishNativeInitializationOrEnabled to not have called the Runnable!",
                mDidCall);
        observer.onFinishNativeInitialization();
        Assert.assertTrue(
                "Something went wrong: "
                        + "onFinishNativeInitializationOrEnabled should have called the Runnable",
                mDidCall);
    }

    @Test
    public void testOnFinishNativeInitializationEnabled_beforeNativeInitDisabled() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureOverrides.removeAllIncludingAnnotations();
        mPartnerCustomizationsUma.onFinishNativeInitializationOrEnabled(
                mActivityLifecycleDispatcherMock, () -> mDidCall = true);
        NativeInitObserver observer = captureObserverFromLifecycleMock();
        FeatureOverrides.disable(ChromeFeatureList.PARTNER_CUSTOMIZATIONS_UMA);
        Assert.assertFalse(
                "Expected onFinishNativeInitializationOrEnabled to not have called the Runnable!",
                mDidCall);
        observer.onFinishNativeInitialization();
        Assert.assertFalse(
                "Checking of the Feature appears to be broken "
                        + "since onFinishNativeInitializationOrEnabled called the runnable!",
                mDidCall);
    }

    @Test
    public void testLogPartnerCustomizationDelegate() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.PartnerHomepageCustomization.Delegate2", G_SERVICE)
                        .build();
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(G_SERVICE);
        histogramWatcher.assertExpected();
        assertLogMessage(Log.INFO, "Partner customization delegate: GService.");
    }

    @Test
    public void testLogPartnerCustomizationUsage() {
        PartnerCustomizationsUma.logPartnerCustomizationUsage(
                PartnerCustomizationsUma.CustomizationUsage.HOMEPAGE);
        assertLogMessage(Log.INFO, "Partner customization usage: Homepage.");

        ShadowLog.reset();
        PartnerCustomizationsUma.logPartnerCustomizationUsage(
                PartnerCustomizationsUma.CustomizationUsage.BOOKMARKS);
        assertLogMessage(Log.INFO, "Partner customization usage: Bookmarks.");

        ShadowLog.reset();
        PartnerCustomizationsUma.logPartnerCustomizationUsage(
                PartnerCustomizationsUma.CustomizationUsage.INCOGNITO);
        assertLogMessage(Log.INFO, "Partner customization usage: Incognito.");
    }

    @Test
    public void testLogDelegateTryCreateDuration() {
        @CustomizationProviderDelegateType int delegate = G_SERVICE;
        long startTime = SystemClock.elapsedRealtime();
        long endTime = startTime + 7;
        PartnerCustomizationsUma.logDelegateTryCreateDuration(
                delegate, startTime, endTime, /* didTryCreateSucceed= */ true);

        assertLogMessage(Log.INFO, "Try create customization delegate GService succeeded in 7 ms.");
    }

    @Test
    public void testLogDelegateTryCreateDuration_failed() {
        @CustomizationProviderDelegateType int delegate = PRELOAD_APK;
        long startTime = SystemClock.elapsedRealtime();
        long endTime = startTime + 9;
        PartnerCustomizationsUma.logDelegateTryCreateDuration(
                delegate, startTime, endTime, /* didTryCreateSucceed= */ false);

        assertLogMessage(Log.WARN, "Try create customization delegate PreloadApk failed in 9 ms.");
    }

    // ==============================================================================================
    // Helpers.
    // ==============================================================================================

    /**
     * Captures the NativeInitObserver used by the {@link #mActivityLifecycleDispatcherMock}, and
     * returns it. Also sets the Feature to be enabled so calling onFinishNativeInitialization will
     * execute the UMA capturing functions.
     *
     * @return The {@link NativeInitObserver} used by the mock and captured, ready to be used.
     */
    private NativeInitObserver captureObserverFromLifecycleMockForEnabledFeature() {
        // We'll need to be enabled to go past the native init.
        FeatureOverrides.enable(ChromeFeatureList.PARTNER_CUSTOMIZATIONS_UMA);
        return captureObserverFromLifecycleMock();
    }

    private HistogramWatcher expectDelegate(@CustomizationProviderDelegateType int whichDelegate) {
        return HistogramWatcher.newBuilder()
                .expectIntRecord("Android.PartnerHomepageCustomization.Delegate2", whichDelegate)
                .build();
    }

    private void setHomepageCachedForTesting() {
        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL, NON_NTP_URL);
        mPartnerCustomizationsUma = new PartnerCustomizationsUma();
    }

    private void assertLogMessage(int expectedLogLevel, String expectedMessage) {
        List<LogItem> logs = ShadowLog.getLogsForTag(LOG_TAG);
        for (LogItem item : logs) {
            if (item.type == expectedLogLevel && expectedMessage.equals(item.msg)) {
                return;
            }
        }
        Assert.fail(
                String.format(
                        Locale.US,
                        "Expected log [level=%d, msg=%s] not found in %s",
                        expectedLogLevel,
                        expectedMessage,
                        logs));
    }

    private void assertNoLogContaining(String substring) {
        List<LogItem> logs = ShadowLog.getLogsForTag(LOG_TAG);
        for (LogItem item : logs) {
            if (item.msg != null && item.msg.contains(substring)) {
                Assert.fail("Unexpected log found containing '" + substring + "': " + item.msg);
            }
        }
    }

    private void assertCustomizationOutcomeLogged(
            @PartnerCustomizationsHomepageEnum int expectedEnum,
            boolean wasHomepageCached,
            @CustomizationProviderDelegateType int whichDelegate) {
        String delegate = delegateName(whichDelegate);
        int expectedLevel;
        String prefix;
        switch (expectedEnum) {
            case NTP_UNKNOWN:
                expectedLevel = Log.WARN;
                prefix =
                        "Initial tab homepage outcome: NTP (unknown if correct, customization"
                                + " incomplete).";
                break;
            case NTP_INCORRECTLY:
                expectedLevel = Log.WARN;
                prefix =
                        "Initial tab homepage outcome: NTP incorrectly (should have been partner"
                                + " homepage).";
                break;
            case NTP_CORRECTLY:
                expectedLevel = Log.INFO;
                prefix = "Initial tab homepage outcome: NTP correctly.";
                break;
            case PARTNER_CUSTOM_HOMEPAGE:
                expectedLevel = Log.INFO;
                prefix = "Initial tab homepage outcome: Partner custom homepage.";
                break;
            case OTHER_CUSTOM_HOMEPAGE:
                expectedLevel = Log.INFO;
                prefix = "Initial tab homepage outcome: Other custom homepage.";
                break;
            default:
                throw new IllegalArgumentException("Unexpected outcome: " + expectedEnum);
        }
        assertLogMessage(
                expectedLevel,
                String.format(
                        Locale.US,
                        "%s delegate=%s, cached=%b",
                        prefix,
                        delegate,
                        wasHomepageCached));
    }

    private void assertTaskCompletionLogged(
            @TaskCompletion int taskCompletion,
            boolean wasHomepageCached,
            @CustomizationProviderDelegateType int whichDelegate) {
        String delegate = delegateName(whichDelegate);
        int expectedLevel;
        String prefix;
        switch (taskCompletion) {
            case COMPLETED_IN_TIME:
                expectedLevel = Log.INFO;
                prefix = "Async init completed in time for tab creation.";
                break;
            case COMPLETED_TOO_LATE:
                expectedLevel = Log.WARN;
                prefix = "Async init completed too late for tab creation.";
                break;
            case CANCELLED:
                expectedLevel = Log.WARN;
                prefix = "Async init cancelled (timeout).";
                break;
            case EXCEPTION:
                expectedLevel = Log.WARN;
                prefix = "Async init failed with exception.";
                break;
            case TASK_SKIPPED:
                expectedLevel = Log.WARN;
                prefix = "Async init task skipped.";
                break;
            default:
                throw new IllegalArgumentException("Unexpected task completion: " + taskCompletion);
        }
        assertLogMessage(
                expectedLevel,
                String.format(
                        Locale.US,
                        "%s delegate=%s, cached=%b",
                        prefix,
                        delegate,
                        wasHomepageCached));
    }

    // ==============================================================================================
    // Key core sequences: not cached and creating the initial Tab before customization finishes.
    // ==============================================================================================

    @Test
    public void testCreateNtpIncorrectlyBeforeCustomization() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureOverrides.removeAllIncludingAnnotations();
        HistogramWatcher histograms = expectDelegate(SOME_DELEGATE);

        mPartnerCustomizationsUma.logAsyncInitStarted(START_TIME);
        mPartnerCustomizationsUma.onCreateInitialTab(
                false, NTP_URL, mActivityLifecycleDispatcherMock, HELPER_FOR_PARTNER_NON_NTP);
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(SOME_DELEGATE);
        mPartnerCustomizationsUma.logAsyncInitCompleted();
        mPartnerCustomizationsUma.logAsyncInitFinalized(true);

        captureObserverFromLifecycleMockForEnabledFeature().onFinishNativeInitialization();
        histograms.assertExpected();
        assertLogMessage(Log.WARN, "Initial tab created before partner customization finished.");
        assertTaskCompletionLogged(COMPLETED_TOO_LATE, NOT_CACHED, SOME_DELEGATE);
        assertCustomizationOutcomeLogged(NTP_INCORRECTLY, NOT_CACHED, SOME_DELEGATE);
    }

    @Test
    public void testCreateNtpCorrectlyBeforeCustomization() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureOverrides.removeAllIncludingAnnotations();
        HistogramWatcher histograms = expectDelegate(SOME_DELEGATE);

        mPartnerCustomizationsUma.logAsyncInitStarted(START_TIME);
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(SOME_DELEGATE);
        mPartnerCustomizationsUma.onCreateInitialTab(
                false, NTP_URL, mActivityLifecycleDispatcherMock, HELPER_FOR_NTP);
        mPartnerCustomizationsUma.logAsyncInitCompleted();
        mPartnerCustomizationsUma.logAsyncInitFinalized(false);

        captureObserverFromLifecycleMockForEnabledFeature().onFinishNativeInitialization();
        histograms.assertExpected();
        assertTaskCompletionLogged(COMPLETED_TOO_LATE, NOT_CACHED, SOME_DELEGATE);
        assertCustomizationOutcomeLogged(NTP_CORRECTLY, NOT_CACHED, SOME_DELEGATE);
    }

    @Test
    public void testCreateNtpUnknownBeforeCustomization() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureOverrides.removeAllIncludingAnnotations();
        HistogramWatcher histograms = expectDelegate(SOME_DELEGATE);

        mPartnerCustomizationsUma.logAsyncInitStarted(START_TIME);
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(SOME_DELEGATE);
        mPartnerCustomizationsUma.onCreateInitialTab(
                false, NTP_URL, mActivityLifecycleDispatcherMock, HELPER_FOR_NTP);
        mPartnerCustomizationsUma.logAsyncInitCancelled();
        mPartnerCustomizationsUma.logAsyncInitFinalized(false);
        // Customization never completes probably due to the async task timing out due to a slow
        // delegate

        captureObserverFromLifecycleMockForEnabledFeature().onFinishNativeInitialization();
        histograms.assertExpected();
        assertTaskCompletionLogged(CANCELLED, NOT_CACHED, SOME_DELEGATE);
        assertCustomizationOutcomeLogged(NTP_UNKNOWN, NOT_CACHED, SOME_DELEGATE);
    }

    @Test
    public void testCreatePartnerHomepageBeforeCustomization() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureOverrides.removeAllIncludingAnnotations();
        HistogramWatcher histograms = expectDelegate(SOME_DELEGATE);

        mPartnerCustomizationsUma.logAsyncInitStarted(START_TIME);
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(SOME_DELEGATE);
        mPartnerCustomizationsUma.onCreateInitialTab(
                false, NON_NTP_URL, mActivityLifecycleDispatcherMock, HELPER_FOR_PARTNER_NON_NTP);
        mPartnerCustomizationsUma.logAsyncInitCompleted();
        mPartnerCustomizationsUma.logAsyncInitFinalized(true);

        captureObserverFromLifecycleMockForEnabledFeature().onFinishNativeInitialization();
        histograms.assertExpected();
        assertTaskCompletionLogged(COMPLETED_TOO_LATE, NOT_CACHED, SOME_DELEGATE);
        assertCustomizationOutcomeLogged(PARTNER_CUSTOM_HOMEPAGE, NOT_CACHED, SOME_DELEGATE);
    }

    @Test
    public void testCreateOtherHomepageBeforeCustomization() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureOverrides.removeAllIncludingAnnotations();
        HistogramWatcher histograms = expectDelegate(SOME_DELEGATE);

        mPartnerCustomizationsUma.logAsyncInitStarted(START_TIME);
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(SOME_DELEGATE);
        mPartnerCustomizationsUma.onCreateInitialTab(
                false,
                NON_NTP_URL,
                mActivityLifecycleDispatcherMock,
                HomepageCharacterizationHelperStub::nonPartnerHelper);
        mPartnerCustomizationsUma.logAsyncInitCompleted();
        mPartnerCustomizationsUma.logAsyncInitFinalized(true);

        captureObserverFromLifecycleMockForEnabledFeature().onFinishNativeInitialization();
        histograms.assertExpected();
        assertTaskCompletionLogged(COMPLETED_TOO_LATE, NOT_CACHED, SOME_DELEGATE);
        assertCustomizationOutcomeLogged(OTHER_CUSTOM_HOMEPAGE, NOT_CACHED, SOME_DELEGATE);
    }

    // ==============================================================================================
    // Additional sequences that are not problematic: Homepage is cached, or creation of the
    // initial Tab is done after customization finishes.
    // ==============================================================================================

    @Test
    public void testCreateNtpCorrectlyCached() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureOverrides.removeAllIncludingAnnotations();
        setHomepageCachedForTesting();
        HistogramWatcher histograms = expectDelegate(SOME_DELEGATE);

        mPartnerCustomizationsUma.logAsyncInitStarted(START_TIME);
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(SOME_DELEGATE);
        mPartnerCustomizationsUma.onCreateInitialTab(
                false, NTP_URL, mActivityLifecycleDispatcherMock, HELPER_FOR_NTP);
        mPartnerCustomizationsUma.logAsyncInitCompleted();
        mPartnerCustomizationsUma.logAsyncInitFinalized(false);

        captureObserverFromLifecycleMockForEnabledFeature().onFinishNativeInitialization();
        histograms.assertExpected();
        assertTaskCompletionLogged(COMPLETED_TOO_LATE, CACHED, SOME_DELEGATE);
        assertCustomizationOutcomeLogged(NTP_CORRECTLY, CACHED, SOME_DELEGATE);
    }

    @Test
    public void testCreatePartnerHomepageCached() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureOverrides.removeAllIncludingAnnotations();
        setHomepageCachedForTesting();
        HistogramWatcher histograms = expectDelegate(SOME_DELEGATE);

        mPartnerCustomizationsUma.logAsyncInitStarted(START_TIME);
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(SOME_DELEGATE);
        mPartnerCustomizationsUma.onCreateInitialTab(
                false, NON_NTP_URL, mActivityLifecycleDispatcherMock, HELPER_FOR_PARTNER_NON_NTP);
        mPartnerCustomizationsUma.logAsyncInitCompleted();
        mPartnerCustomizationsUma.logAsyncInitFinalized(false);

        captureObserverFromLifecycleMockForEnabledFeature().onFinishNativeInitialization();
        histograms.assertExpected();
        assertTaskCompletionLogged(COMPLETED_TOO_LATE, CACHED, SOME_DELEGATE);
        assertCustomizationOutcomeLogged(PARTNER_CUSTOM_HOMEPAGE, CACHED, SOME_DELEGATE);
    }

    @Test
    public void testCreateNtpCorrectlyAfterCustomization() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureOverrides.removeAllIncludingAnnotations();
        HistogramWatcher histograms = expectDelegate(SOME_DELEGATE);

        mPartnerCustomizationsUma.logAsyncInitStarted(START_TIME);
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(SOME_DELEGATE);
        mPartnerCustomizationsUma.logAsyncInitCompleted();
        mPartnerCustomizationsUma.logAsyncInitFinalized(false);

        mPartnerCustomizationsUma.onCreateInitialTab(
                false, NTP_URL, mActivityLifecycleDispatcherMock, HELPER_FOR_NTP);

        captureObserverFromLifecycleMockForEnabledFeature().onFinishNativeInitialization();
        histograms.assertExpected();
        assertTaskCompletionLogged(COMPLETED_IN_TIME, NOT_CACHED, SOME_DELEGATE);
        assertCustomizationOutcomeLogged(NTP_CORRECTLY, NOT_CACHED, SOME_DELEGATE);
    }

    @Test
    public void testCreatePartnerHomepageAfterCustomization() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureOverrides.removeAllIncludingAnnotations();
        HistogramWatcher histograms = expectDelegate(SOME_DELEGATE);

        mPartnerCustomizationsUma.logAsyncInitStarted(START_TIME);
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(SOME_DELEGATE);
        mPartnerCustomizationsUma.logAsyncInitCompleted();
        mPartnerCustomizationsUma.logAsyncInitFinalized(true);

        mPartnerCustomizationsUma.onCreateInitialTab(
                true, NON_NTP_URL, mActivityLifecycleDispatcherMock, HELPER_FOR_PARTNER_NON_NTP);

        captureObserverFromLifecycleMockForEnabledFeature().onFinishNativeInitialization();
        histograms.assertExpected();
        assertNoLogContaining("Initial tab created before partner customization finished.");
        assertTaskCompletionLogged(COMPLETED_IN_TIME, NOT_CACHED, SOME_DELEGATE);
        assertCustomizationOutcomeLogged(PARTNER_CUSTOM_HOMEPAGE, NOT_CACHED, SOME_DELEGATE);
    }

    // ==============================================================================================
    // Problematic or unexpected sequences.
    // ==============================================================================================

    @Test
    public void testCreateInitialTabCalledBeforeCustomizationStarts() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureOverrides.removeAllIncludingAnnotations();

        mPartnerCustomizationsUma.onCreateInitialTab(
                false, NON_NTP_URL, mActivityLifecycleDispatcherMock, HELPER_FOR_PARTNER_NON_NTP);
        assertNoLogContaining("Initial tab homepage outcome:");

        // Outcome and delegate histogram should be emitted once the Async task finishes.
        HistogramWatcher histograms = expectDelegate(SOME_DELEGATE);

        mPartnerCustomizationsUma.logAsyncInitStarted(START_TIME);
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(SOME_DELEGATE);
        mPartnerCustomizationsUma.logAsyncInitCompleted();
        mPartnerCustomizationsUma.logAsyncInitFinalized(true);

        captureObserverFromLifecycleMockForEnabledFeature().onFinishNativeInitialization();
        histograms.assertExpected();
        assertTaskCompletionLogged(COMPLETED_TOO_LATE, NOT_CACHED, SOME_DELEGATE);
        assertCustomizationOutcomeLogged(PARTNER_CUSTOM_HOMEPAGE, NOT_CACHED, SOME_DELEGATE);
    }

    /**
     * Tests that multi-instance causing multiple onCreateInitialTab calls ignores all but the
     * first.
     */
    @Test
    public void testCreateInitialTabCalledMultipleTimes() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureOverrides.removeAllIncludingAnnotations();
        HistogramWatcher histograms = expectDelegate(SOME_DELEGATE);

        mPartnerCustomizationsUma.logAsyncInitStarted(START_TIME);
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(SOME_DELEGATE);
        mPartnerCustomizationsUma.onCreateInitialTab(
                false, NON_NTP_URL, mActivityLifecycleDispatcherMock, HELPER_FOR_PARTNER_NON_NTP);
        // Starting two activities right away, e.g. on multi window. The second should be ignored.
        mPartnerCustomizationsUma.onCreateInitialTab(
                false, NON_NTP_URL, mActivityLifecycleDispatcherMock, HELPER_FOR_PARTNER_NON_NTP);
        mPartnerCustomizationsUma.logAsyncInitCompleted();
        mPartnerCustomizationsUma.logAsyncInitFinalized(true);

        captureObserverFromLifecycleMockForEnabledFeature().onFinishNativeInitialization();
        histograms.assertExpected();
        assertLogMessage(Log.WARN, "Multiple initial Tabs being created, e.g. multi-instance.");
        assertTaskCompletionLogged(COMPLETED_TOO_LATE, NOT_CACHED, SOME_DELEGATE);
        assertCustomizationOutcomeLogged(PARTNER_CUSTOM_HOMEPAGE, NOT_CACHED, SOME_DELEGATE);
    }

    @Test
    public void testAsyncInitTaskSkipped() {
        mPartnerCustomizationsUma.logAsyncInitCompleted();
        assertTaskCompletionLogged(
                TASK_SKIPPED, NOT_CACHED, CustomizationProviderDelegateType.NONE_VALID);
    }

    @Test
    public void testAsyncInitException() {
        mPartnerCustomizationsUma.logAsyncInitStarted(START_TIME);
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(SOME_DELEGATE);
        mPartnerCustomizationsUma.logAsyncInitException();
        assertTaskCompletionLogged(EXCEPTION, NOT_CACHED, SOME_DELEGATE);
    }
}
