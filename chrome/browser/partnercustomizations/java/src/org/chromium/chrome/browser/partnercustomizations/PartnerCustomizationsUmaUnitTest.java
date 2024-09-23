// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.CustomizationProviderDelegateType.G_SERVICE;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.CustomizationProviderDelegateType.PHENOTYPE;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.CustomizationProviderDelegateType.PRELOAD_APK;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.PartnerCustomizationsHomepageEnum.NTP_CORRECTLY;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.PartnerCustomizationsHomepageEnum.NTP_INCORRECTLY;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.PartnerCustomizationsHomepageEnum.NTP_UNKNOWN;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.PartnerCustomizationsHomepageEnum.OTHER_CUSTOM_HOMEPAGE;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.PartnerCustomizationsHomepageEnum.PARTNER_CUSTOM_HOMEPAGE;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.TaskCompletion.CANCELLED;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.TaskCompletion.COMPLETED_IN_TIME;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.TaskCompletion.COMPLETED_TOO_LATE;
import static org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.delegateName;

import android.os.SystemClock;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.supplier.Supplier;
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
import org.chromium.components.embedder_support.util.UrlConstants;

/** Unit tests for {@link PartnerCustomizationsUma}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PartnerCustomizationsUmaUnitTest {
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcherMock;

    @Captor private ArgumentCaptor<LifecycleObserver> mLifeCycleObserverCaptor;

    private static final @CustomizationProviderDelegateType int SOME_DELEGATE = G_SERVICE;
    private static final @CustomizationProviderDelegateType int UNUSED = PHENOTYPE;

    /** Fake timings with arbitrary ascending values with spacing that is relatively unique. */
    private static final int CREATE_BEFORE_CUSTOMIZATION_TIME = 300;

    private static final int START_TIME = 700;
    private static final int CREATE_DURING_CUSTOMIZATION_TIME = 1500;
    private static final int CREATE_DURING_CUSTOMIZATION_A_BIT_LATER_TIME = 1550;
    private static final int END_TIME = 2700;
    private static final int CREATE_AFTER_CUSTOMIZATION_TIME = 4500;
    private static final int UNUSED_TIME = 0;

    private static final boolean NOT_CACHED = false;
    private static final boolean CACHED = true;

    private static final String NTP_URL = UrlConstants.NTP_URL;
    private static final String NON_NTP_URL = "https://www.google.com/";

    private static final Supplier<HomepageCharacterizationHelper> HELPER_FOR_NTP =
            HomepageCharacterizationHelperStub::ntpHelper;

    private static final Supplier<HomepageCharacterizationHelper> HELPER_FOR_PARTNER_NON_NTP =
            HomepageCharacterizationHelperStub::nonNtpHelper;

    private TestValues mEnabledTestValues;
    private TestValues mDisabledTestValues;

    private PartnerCustomizationsUma mPartnerCustomizationsUma;

    private boolean mDidCall;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mEnabledTestValues = new TestValues();
        mEnabledTestValues.addFeatureFlagOverride(
                ChromeFeatureList.PARTNER_CUSTOMIZATIONS_UMA, true);
        mDisabledTestValues = new TestValues();
        mDisabledTestValues.addFeatureFlagOverride(
                ChromeFeatureList.PARTNER_CUSTOMIZATIONS_UMA, false);
        PartnerCustomizationsUma.resetStaticsForTesting();
        mPartnerCustomizationsUma = new PartnerCustomizationsUma();
    }

    @After
    public void tearDown() {
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
        FeatureList.setTestValues(null);
        mPartnerCustomizationsUma.onFinishNativeInitializationOrEnabled(
                mActivityLifecycleDispatcherMock, () -> mDidCall = true);
        NativeInitObserver observer = captureObserverFromLifecycleMock();
        FeatureList.setTestValues(mEnabledTestValues);
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
        FeatureList.setTestValues(null);
        mPartnerCustomizationsUma.onFinishNativeInitializationOrEnabled(
                mActivityLifecycleDispatcherMock, () -> mDidCall = true);
        NativeInitObserver observer = captureObserverFromLifecycleMock();
        FeatureList.setTestValues(mDisabledTestValues);
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
    }

    @Test
    public void testLogPartnerCustomizationUsage() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.PartnerCustomization.Usage",
                                PartnerCustomizationsUma.CustomizationUsage.HOMEPAGE)
                        .build();
        PartnerCustomizationsUma.logPartnerCustomizationUsage(
                PartnerCustomizationsUma.CustomizationUsage.HOMEPAGE);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testLogDelegateTryCreateDuration() {
        @CustomizationProviderDelegateType int delegate = G_SERVICE;
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.PartnerCustomization.TrySucceededDuration.GService", 7)
                        .build();

        long startTime = SystemClock.elapsedRealtime();
        long endTime = startTime + 7;
        PartnerCustomizationsUma.logDelegateTryCreateDuration(delegate, startTime, endTime, true);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testLogDelegateTryCreateDuration_failed() {
        @CustomizationProviderDelegateType int delegate = PRELOAD_APK;
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.PartnerCustomization.TryFailedDuration." + "PreloadApk", 9)
                        .build();

        long startTime = SystemClock.elapsedRealtime();
        long endTime = startTime + 9;
        PartnerCustomizationsUma.logDelegateTryCreateDuration(delegate, startTime, endTime, false);

        histogramWatcher.assertExpected();
    }

    // ==============================================================================================
    // Tests for the basic logic of "outcomes" from homepage customization and tab creation.
    // We try all permutations of the three controlling booleans.
    // The test names are intended to be readable with the expected outcome on the next line.
    // The naming pattern goes:
    //  testOutcomeMade {Ntp | Homepage} {ForCertain | Uncertain}
    //      + Now {Partner | NonPartner} {Page | Ntp}
    //
    //  Note that "Partner" and "Other" only apply when Ntp is false, and omitted otherwise.
    // ==============================================================================================

    @Test
    public void testOutcomeMadeNtpForCertainNowPartnerNtp() {
        HistogramWatcher histograms = expectOutcome(NTP_CORRECTLY);
        mPartnerCustomizationsUma.logInitialTabCustomizationOutcomeDelayed(
                UNUSED,
                /* isInitialTabNtpOrOverview= */ true,
                /* isCharacterizationCertain= */ true,
                /* isHomepagePartner= */ true,
                /* isHomepageNtp= */ true,
                /* unused= */ true);
        histograms.assertExpected();
    }

    @Test
    public void testOutcomeMadeNtpForCertainNowPartnerPage() {
        HistogramWatcher histograms = expectOutcome(NTP_INCORRECTLY);
        mPartnerCustomizationsUma.logInitialTabCustomizationOutcomeDelayed(
                UNUSED,
                /* isInitialTabNtpOrOverview= */ true,
                /* isCharacterizationCertain= */ true,
                /* isHomepagePartner= */ true,
                /* isHomepageNtp= */ false,
                /* unused= */ true);
        histograms.assertExpected();
    }

    @Test
    public void testOutcomeMadeNtpForCertainNowNonPartnerNtp() {
        HistogramWatcher histograms = expectOutcome(NTP_CORRECTLY);
        mPartnerCustomizationsUma.logInitialTabCustomizationOutcomeDelayed(
                UNUSED,
                /* isInitialTabNtpOrOverview= */ true,
                /* isCharacterizationCertain= */ true,
                /* isHomepagePartner= */ false,
                /* isHomepageNtp= */ true,
                /* unused= */ true);
        histograms.assertExpected();
    }

    @Test
    public void testOutcomeMadeNtpForCertainNowNonPartnerPage() {
        HistogramWatcher histograms = expectOutcome(NTP_INCORRECTLY);
        mPartnerCustomizationsUma.logInitialTabCustomizationOutcomeDelayed(
                UNUSED,
                /* isInitialTabNtpOrOverview= */ true,
                /* isCharacterizationCertain= */ true,
                /* isHomepagePartner= */ false,
                /* isHomepageNtp= */ false,
                /* unused= */ true);
        histograms.assertExpected();
    }

    @Test
    public void testOutcomeMadeNtpUncertainNowPartnerNtp() {
        HistogramWatcher histograms = expectOutcome(NTP_UNKNOWN);
        mPartnerCustomizationsUma.logInitialTabCustomizationOutcomeDelayed(
                UNUSED,
                /* isInitialTabNtpOrOverview= */ true,
                /* isCharacterizationCertain= */ false,
                /* isHomepagePartner= */ true,
                /* isHomepageNtp= */ true,
                /* unused= */ true);
        histograms.assertExpected();
    }

    @Test
    public void testOutcomeMadeNtpUncertainNowPartnerPage() {
        HistogramWatcher histograms = expectOutcome(NTP_UNKNOWN);
        mPartnerCustomizationsUma.logInitialTabCustomizationOutcomeDelayed(
                UNUSED,
                /* isInitialTabNtpOrOverview= */ true,
                /* isCharacterizationCertain= */ false,
                /* isHomepagePartner= */ true,
                /* isHomepageNtp= */ false,
                /* unused= */ true);
        histograms.assertExpected();
    }

    @Test
    public void testOutcomeMadeNtpUncertainNowNonPartnerNtp() {
        HistogramWatcher histograms = expectOutcome(NTP_UNKNOWN);
        mPartnerCustomizationsUma.logInitialTabCustomizationOutcomeDelayed(
                UNUSED,
                /* isInitialTabNtpOrOverview= */ true,
                /* isCharacterizationCertain= */ false,
                /* isHomepagePartner= */ false,
                /* isHomepageNtp= */ true,
                /* unused= */ true);
        histograms.assertExpected();
    }

    @Test
    public void testOutcomeMadeNtpUncertainNowNonPartnerPage() {
        HistogramWatcher histograms = expectOutcome(NTP_UNKNOWN);
        mPartnerCustomizationsUma.logInitialTabCustomizationOutcomeDelayed(
                UNUSED,
                /* isInitialTabNtpOrOverview= */ true,
                /* isCharacterizationCertain= */ false,
                /* isHomepagePartner= */ false,
                /* isHomepageNtp= */ false,
                /* unused= */ true);
        histograms.assertExpected();
    }

    @Test
    public void testOutcomeMadeHomepageForCertainNowPartnerNtp() {
        HistogramWatcher histograms = expectOutcome(PARTNER_CUSTOM_HOMEPAGE);
        mPartnerCustomizationsUma.logInitialTabCustomizationOutcomeDelayed(
                UNUSED,
                /* isInitialTabNtpOrOverview= */ false,
                /* isCharacterizationCertain= */ true,
                /* isHomepagePartner= */ true,
                /* isHomepageNtp= */ true,
                /* unused= */ true);
        histograms.assertExpected();
    }

    @Test
    public void testOutcomeMadeHomepageForCertainNowPartnerPage() {
        HistogramWatcher histograms = expectOutcome(PARTNER_CUSTOM_HOMEPAGE);
        mPartnerCustomizationsUma.logInitialTabCustomizationOutcomeDelayed(
                UNUSED,
                /* isInitialTabNtpOrOverview= */ false,
                /* isCharacterizationCertain= */ true,
                /* isHomepagePartner= */ true,
                /* isHomepageNtp= */ false,
                /* unused= */ true);
        histograms.assertExpected();
    }

    @Test
    public void testOutcomeMadeHomepageForCertainNowNonPartnerNtp() {
        HistogramWatcher histograms = expectOutcome(OTHER_CUSTOM_HOMEPAGE);
        mPartnerCustomizationsUma.logInitialTabCustomizationOutcomeDelayed(
                UNUSED,
                /* isInitialTabNtpOrOverview= */ false,
                /* isCharacterizationCertain= */ true,
                /* isHomepagePartner= */ false,
                /* isHomepageNtp= */ true,
                /* unused= */ true);
        histograms.assertExpected();
    }

    @Test
    public void testOutcomeMadeHomepageForCertainNowNonPartnerPage() {
        HistogramWatcher histograms = expectOutcome(OTHER_CUSTOM_HOMEPAGE);
        mPartnerCustomizationsUma.logInitialTabCustomizationOutcomeDelayed(
                UNUSED,
                /* isInitialTabNtpOrOverview= */ false,
                /* isCharacterizationCertain= */ true,
                /* isHomepagePartner= */ false,
                /* isHomepageNtp= */ false,
                /* unused= */ true);
        histograms.assertExpected();
    }

    @Test
    public void testOutcomeMadeHomepageUncertainNowPartnerNtp() {
        HistogramWatcher histograms = expectOutcome(PARTNER_CUSTOM_HOMEPAGE);
        mPartnerCustomizationsUma.logInitialTabCustomizationOutcomeDelayed(
                UNUSED,
                /* isInitialTabNtpOrOverview= */ false,
                /* isCharacterizationCertain= */ false,
                /* isHomepagePartner= */ true,
                /* isHomepageNtp= */ true,
                /* unused= */ true);
        histograms.assertExpected();
    }

    @Test
    public void testOutcomeMadeHomepageUncertainNowPartnerPage() {
        HistogramWatcher histograms = expectOutcome(PARTNER_CUSTOM_HOMEPAGE);
        mPartnerCustomizationsUma.logInitialTabCustomizationOutcomeDelayed(
                UNUSED,
                /* isInitialTabNtpOrOverview= */ false,
                /* isCharacterizationCertain= */ false,
                /* isHomepagePartner= */ true,
                /* isHomepageNtp= */ false,
                /* unused= */ true);
        histograms.assertExpected();
    }

    @Test
    public void testOutcomeMadeHomepageUncertainNowNonPartnerNtp() {
        HistogramWatcher histograms = expectOutcome(OTHER_CUSTOM_HOMEPAGE);
        mPartnerCustomizationsUma.logInitialTabCustomizationOutcomeDelayed(
                UNUSED,
                /* isInitialTabNtpOrOverview= */ false,
                /* isCharacterizationCertain= */ false,
                /* isHomepagePartner= */ false,
                /* isHomepageNtp= */ true,
                /* unused= */ true);
        histograms.assertExpected();
    }

    @Test
    public void testOutcomeMadeHomepageUncertainNowNonPartnerPage() {
        HistogramWatcher histograms = expectOutcome(OTHER_CUSTOM_HOMEPAGE);
        mPartnerCustomizationsUma.logInitialTabCustomizationOutcomeDelayed(
                UNUSED,
                /* isInitialTabNtpOrOverview= */ false,
                /* isCharacterizationCertain= */ false,
                /* isHomepagePartner= */ false,
                /* isHomepageNtp= */ false,
                /* unused= */ true);
        histograms.assertExpected();
    }

    // ==============================================================================================
    // Helpers.
    // ==============================================================================================

    private HistogramWatcher expectOutcome(@PartnerCustomizationsHomepageEnum int expectedEnum) {
        return HistogramWatcher.newSingleRecordWatcher(
                "Android.PartnerCustomization.HomepageCustomizationOutcome", expectedEnum);
    }

    private HistogramWatcher.Builder expectCustomizationOutcome(
            @PartnerCustomizationsHomepageEnum int expectedEnum,
            boolean wasHomepageCached,
            @CustomizationProviderDelegateType int whichDelegate) {
        var builder =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.PartnerCustomization.HomepageCustomizationOutcome",
                                expectedEnum)
                        .expectIntRecord(
                                "Android.PartnerCustomization.HomepageCustomizationOutcome."
                                        + delegateName(whichDelegate),
                                expectedEnum);
        if (!wasHomepageCached) {
            builder.expectIntRecord(
                    "Android.PartnerCustomization.HomepageCustomizationOutcomeNotCached."
                            + delegateName(whichDelegate),
                    expectedEnum);
        }
        builder.expectIntRecord("Android.PartnerHomepageCustomization.Delegate2", whichDelegate);
        return builder;
    }

    private void expectInitializationCompleted(
            HistogramWatcher.Builder builder,
            boolean wasHomepageCached,
            @CustomizationProviderDelegateType int whichDelegate,
            @TaskCompletion int taskCompletion,
            int durationNeeded) {
        builder.expectIntRecord("Android.PartnerCustomization.TaskCompletion", taskCompletion);
        builder.expectIntRecord(
                "Android.PartnerCustomization.TaskCompletion." + delegateName(whichDelegate),
                taskCompletion);
        if (!wasHomepageCached) {
            builder.expectIntRecord(
                    "Android.PartnerCustomization.TaskCompletionNotCached."
                            + delegateName(whichDelegate),
                    taskCompletion);
        }
    }

    /**
     * Captures the NativeInitObserver used by the {@link #mActivityLifecycleDispatcherMock}, and
     * returns it. Also sets the Feature to be enabled so calling onFinishNativeInitialization will
     * execute the UMA capturing functions.
     *
     * @return The {@link NativeInitObserver} used by the mock and captured, ready to be used.
     */
    private NativeInitObserver captureObserverFromLifecycleMockForEnabledFeature() {
        // We'll need to be enabled to go past the native init.
        FeatureList.setTestValues(mEnabledTestValues);
        return captureObserverFromLifecycleMock();
    }

    // ==============================================================================================
    // Key core sequences: not cached and creating the initial Tab before customization finishes.
    // ==============================================================================================

    @Test
    public void testCreateNtpIncorrectlyBeforeCustomization() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureList.setTestValues(null);
        HistogramWatcher.Builder builder =
                expectCustomizationOutcome(NTP_INCORRECTLY, NOT_CACHED, SOME_DELEGATE);
        expectInitializationCompleted(
                builder,
                NOT_CACHED,
                SOME_DELEGATE,
                COMPLETED_TOO_LATE,
                END_TIME - CREATE_DURING_CUSTOMIZATION_TIME);
        HistogramWatcher histograms = builder.build();

        mPartnerCustomizationsUma.logAsyncInitStarted(START_TIME);
        mPartnerCustomizationsUma.onCreateInitialTab(
                false, NTP_URL, mActivityLifecycleDispatcherMock, HELPER_FOR_PARTNER_NON_NTP);
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(SOME_DELEGATE);
        mPartnerCustomizationsUma.logAsyncInitCompleted();
        mPartnerCustomizationsUma.logAsyncInitFinalized(true);

        captureObserverFromLifecycleMockForEnabledFeature().onFinishNativeInitialization();
        histograms.assertExpected();
    }

    @Test
    public void testCreateNtpCorrectlyBeforeCustomization() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureList.setTestValues(null);
        HistogramWatcher.Builder builder =
                expectCustomizationOutcome(NTP_CORRECTLY, NOT_CACHED, SOME_DELEGATE);
        expectInitializationCompleted(
                builder,
                NOT_CACHED,
                SOME_DELEGATE,
                COMPLETED_TOO_LATE,
                END_TIME - CREATE_DURING_CUSTOMIZATION_TIME);
        HistogramWatcher histograms = builder.build();

        mPartnerCustomizationsUma.logAsyncInitStarted(START_TIME);
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(SOME_DELEGATE);
        mPartnerCustomizationsUma.onCreateInitialTab(
                false, NTP_URL, mActivityLifecycleDispatcherMock, HELPER_FOR_NTP);
        mPartnerCustomizationsUma.logAsyncInitCompleted();
        mPartnerCustomizationsUma.logAsyncInitFinalized(false);

        captureObserverFromLifecycleMockForEnabledFeature().onFinishNativeInitialization();
        histograms.assertExpected();
    }

    @Test
    public void testCreateNtpUnknownBeforeCustomization() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureList.setTestValues(null);
        HistogramWatcher.Builder builder =
                expectCustomizationOutcome(NTP_UNKNOWN, NOT_CACHED, SOME_DELEGATE);
        expectInitializationCompleted(builder, NOT_CACHED, SOME_DELEGATE, CANCELLED, UNUSED_TIME);

        HistogramWatcher histograms = builder.build();

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
    }

    @Test
    public void testCreatePartnerHomepageBeforeCustomization() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureList.setTestValues(null);
        HistogramWatcher.Builder builder =
                expectCustomizationOutcome(PARTNER_CUSTOM_HOMEPAGE, NOT_CACHED, SOME_DELEGATE);
        expectInitializationCompleted(
                builder,
                NOT_CACHED,
                SOME_DELEGATE,
                COMPLETED_TOO_LATE,
                END_TIME - CREATE_DURING_CUSTOMIZATION_TIME);
        HistogramWatcher histograms = builder.build();

        mPartnerCustomizationsUma.logAsyncInitStarted(START_TIME);
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(SOME_DELEGATE);
        mPartnerCustomizationsUma.onCreateInitialTab(
                false, NON_NTP_URL, mActivityLifecycleDispatcherMock, HELPER_FOR_PARTNER_NON_NTP);
        mPartnerCustomizationsUma.logAsyncInitCompleted();
        mPartnerCustomizationsUma.logAsyncInitFinalized(true);

        captureObserverFromLifecycleMockForEnabledFeature().onFinishNativeInitialization();
        histograms.assertExpected();
    }

    @Test
    public void testCreateOtherHomepageBeforeCustomization() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureList.setTestValues(null);
        HistogramWatcher.Builder builder =
                expectCustomizationOutcome(OTHER_CUSTOM_HOMEPAGE, false, SOME_DELEGATE);
        expectInitializationCompleted(
                builder,
                NOT_CACHED,
                SOME_DELEGATE,
                COMPLETED_TOO_LATE,
                END_TIME - CREATE_DURING_CUSTOMIZATION_TIME);
        HistogramWatcher histograms = builder.build();

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
    }

    // ==============================================================================================
    // Additional sequences that are not problematic: Homepage is cached, or creation of the
    // initial Tab is done after customization finishes.
    // ==============================================================================================

    @Test
    public void testCreateNtpCorrectlyCached() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureList.setTestValues(null);
        HistogramWatcher.Builder builder =
                expectCustomizationOutcome(NTP_CORRECTLY, CACHED, SOME_DELEGATE);
        expectInitializationCompleted(
                builder,
                CACHED,
                SOME_DELEGATE,
                COMPLETED_TOO_LATE,
                END_TIME - CREATE_DURING_CUSTOMIZATION_TIME);
        HistogramWatcher histograms = builder.build();

        mPartnerCustomizationsUma.logAsyncInitStarted(START_TIME);
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(SOME_DELEGATE);
        mPartnerCustomizationsUma.onCreateInitialTab(
                false, NTP_URL, mActivityLifecycleDispatcherMock, HELPER_FOR_NTP);
        mPartnerCustomizationsUma.logAsyncInitCompleted();
        mPartnerCustomizationsUma.logAsyncInitFinalized(false);

        captureObserverFromLifecycleMockForEnabledFeature().onFinishNativeInitialization();
        histograms.assertExpected();
    }

    @Test
    public void testCreatePartnerHomepageCached() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureList.setTestValues(null);
        HistogramWatcher.Builder builder =
                expectCustomizationOutcome(PARTNER_CUSTOM_HOMEPAGE, CACHED, SOME_DELEGATE);
        expectInitializationCompleted(
                builder,
                CACHED,
                SOME_DELEGATE,
                COMPLETED_TOO_LATE,
                END_TIME - CREATE_DURING_CUSTOMIZATION_TIME);
        HistogramWatcher histograms = builder.build();

        mPartnerCustomizationsUma.logAsyncInitStarted(START_TIME);
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(SOME_DELEGATE);
        mPartnerCustomizationsUma.onCreateInitialTab(
                false, NON_NTP_URL, mActivityLifecycleDispatcherMock, HELPER_FOR_PARTNER_NON_NTP);
        mPartnerCustomizationsUma.logAsyncInitCompleted();
        mPartnerCustomizationsUma.logAsyncInitFinalized(false);

        captureObserverFromLifecycleMockForEnabledFeature().onFinishNativeInitialization();
        histograms.assertExpected();
    }

    @Test
    public void testCreateNtpCorrectlyAfterCustomization() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureList.setTestValues(null);
        HistogramWatcher.Builder builder =
                expectCustomizationOutcome(NTP_CORRECTLY, NOT_CACHED, SOME_DELEGATE);
        expectInitializationCompleted(
                builder, NOT_CACHED, SOME_DELEGATE, COMPLETED_IN_TIME, UNUSED_TIME);
        HistogramWatcher histograms = builder.build();

        mPartnerCustomizationsUma.logAsyncInitStarted(START_TIME);
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(SOME_DELEGATE);
        mPartnerCustomizationsUma.logAsyncInitCompleted();
        mPartnerCustomizationsUma.logAsyncInitFinalized(false);

        mPartnerCustomizationsUma.onCreateInitialTab(
                false, NTP_URL, mActivityLifecycleDispatcherMock, HELPER_FOR_NTP);

        captureObserverFromLifecycleMockForEnabledFeature().onFinishNativeInitialization();
        histograms.assertExpected();
    }

    @Test
    public void testCreatePartnerHomepageAfterCustomization() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureList.setTestValues(null);
        HistogramWatcher.Builder builder =
                expectCustomizationOutcome(PARTNER_CUSTOM_HOMEPAGE, NOT_CACHED, SOME_DELEGATE);
        expectInitializationCompleted(
                builder, NOT_CACHED, SOME_DELEGATE, COMPLETED_IN_TIME, UNUSED_TIME);
        HistogramWatcher histograms = builder.build();

        mPartnerCustomizationsUma.logAsyncInitStarted(START_TIME);
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(SOME_DELEGATE);
        mPartnerCustomizationsUma.logAsyncInitCompleted();
        mPartnerCustomizationsUma.logAsyncInitFinalized(true);

        mPartnerCustomizationsUma.onCreateInitialTab(
                true, NON_NTP_URL, mActivityLifecycleDispatcherMock, HELPER_FOR_PARTNER_NON_NTP);

        captureObserverFromLifecycleMockForEnabledFeature().onFinishNativeInitialization();
        histograms.assertExpected();
    }

    // ==============================================================================================
    // Problematic or unexpected sequences.
    // ==============================================================================================

    @Test
    public void testCreateInitialTabCalledBeforeCustomizationStarts() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureList.setTestValues(null);
        HistogramWatcher.Builder beforeStartedBuilder =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Android.PartnerCustomization.HomepageCustomizationOutcome");

        mPartnerCustomizationsUma.onCreateInitialTab(
                false, NON_NTP_URL, mActivityLifecycleDispatcherMock, HELPER_FOR_PARTNER_NON_NTP);
        beforeStartedBuilder.build().assertExpected();

        // Histograms should be emitted once the Async task starts up.
        HistogramWatcher.Builder builder =
                expectCustomizationOutcome(PARTNER_CUSTOM_HOMEPAGE, NOT_CACHED, SOME_DELEGATE);
        expectInitializationCompleted(
                builder,
                NOT_CACHED,
                SOME_DELEGATE,
                COMPLETED_TOO_LATE,
                END_TIME - CREATE_BEFORE_CUSTOMIZATION_TIME);
        HistogramWatcher histograms = builder.build();

        mPartnerCustomizationsUma.logAsyncInitStarted(START_TIME);
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(SOME_DELEGATE);
        mPartnerCustomizationsUma.logAsyncInitCompleted();
        mPartnerCustomizationsUma.logAsyncInitFinalized(true);

        captureObserverFromLifecycleMockForEnabledFeature().onFinishNativeInitialization();
        histograms.assertExpected();
    }

    /**
     * Tests that multi-instance causing multiple onCreateInitialTab calls ignores all but the
     * first.
     */
    @Test
    public void testCreateInitialTabCalledMultipleTimes() {
        // Unset test values so that FeatureList#isInitialized returns false.
        FeatureList.setTestValues(null);
        HistogramWatcher.Builder builder =
                expectCustomizationOutcome(PARTNER_CUSTOM_HOMEPAGE, NOT_CACHED, SOME_DELEGATE);
        expectInitializationCompleted(
                builder,
                NOT_CACHED,
                SOME_DELEGATE,
                COMPLETED_TOO_LATE,
                END_TIME - CREATE_DURING_CUSTOMIZATION_TIME);
        HistogramWatcher histograms = builder.build();

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
    }
}
