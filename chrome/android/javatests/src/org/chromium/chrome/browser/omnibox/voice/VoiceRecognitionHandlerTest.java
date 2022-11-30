// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.FeatureList;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.metrics.HistogramTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerJni;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.AssistantActionPerformed;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.AudioPermissionState;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceIntentTarget;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceInteractionSource;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceResult;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.WindowAndroid.IntentCallback;
import org.chromium.url.GURL;

import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Tests for {@link VoiceRecognitionHandler}. For more tests specific to the
 * Assistant recognition and actions see
 * {@link AssistantVoiceRecognitionHandlerTest}
 * and {@link AssistantActionsHanlerTest} respectively.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class VoiceRecognitionHandlerTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();
    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public HistogramTestRule mHistograms = new HistogramTestRule();

    @Mock
    Intent mIntent;
    @Mock
    AssistantVoiceSearchService mAssistantVoiceSearchService;
    @Mock
    Tab mTab;
    @Mock
    VoiceRecognitionHandler.Observer mObserver;
    @Mock
    AutocompleteController mController;
    @Mock
    AutocompleteController.Natives mControllerJniMock;
    @Mock
    AutocompleteMatch mMatch;

    private RecognitionTestHelper.TestDataProvider mDataProvider;
    private RecognitionTestHelper.TestDelegate mDelegate;
    private RecognitionTestHelper.TestAutocompleteCoordinator mAutocompleteCoordinator;
    private RecognitionTestHelper.TestVoiceRecognitionHandler mHandler;
    private RecognitionTestHelper.TestAndroidPermissionDelegate mPermissionDelegate;
    private RecognitionTestHelper.TestWindowAndroid mWindowAndroid;
    private ObservableSupplierImpl<Profile> mProfileSupplier;
    private FeatureList.TestValues mFeatures;

    @BeforeClass
    public static void setUpClass() {
        sActivityTestRule.startMainActivityOnBlankPage();
        sActivityTestRule.waitForDeferredStartup();
    }

    @Before
    public void setUp() throws InterruptedException, ExecutionException {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(AutocompleteControllerJni.TEST_HOOKS, mControllerJniMock);
        doReturn(mController).when(mControllerJniMock).getForProfile(any());
        doReturn(mMatch).when(mController).classify(any(), anyBoolean());
        doReturn(new GURL("https://www.google.com/search?q=abc")).when(mMatch).getUrl();
        doReturn(true).when(mMatch).isSearchSuggestion();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mProfileSupplier = new ObservableSupplierImpl<>();
            RecognitionTestHelper testHelper =
                    new RecognitionTestHelper(mAssistantVoiceSearchService, mProfileSupplier,
                            sActivityTestRule.getActivity());
            mDataProvider = testHelper.getDataProvider();
            mDataProvider.setTab(mTab);
            mPermissionDelegate = testHelper.getAndroidPermissionDelegate();
            mWindowAndroid = testHelper.getWindowAndroid();

            mWindowAndroid.setAndroidPermissionDelegate(mPermissionDelegate);
            mDelegate = testHelper.getDelegate();
            mHandler = testHelper.getVoiceRecognitionHandler();
            mHandler.addObserver(mObserver);
            mAutocompleteCoordinator = testHelper.getAutocompleteCoordinator();
        });

        doReturn(new GURL(RecognitionTestHelper.DEFAULT_URL)).when(mTab).getUrl();

        doReturn(false).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        doReturn(false).when(mAssistantVoiceSearchService).needsEnabledCheck();

        mFeatures = RecognitionTestHelper.resetTestFeatures();
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            RecognitionTestHelper.setAudioCapturePref(true);
            mHandler.removeObserver(mObserver);
            VoiceRecognitionHandler.setIsRecognitionIntentPresentForTesting(null);
            mHandler.setIsVoiceSearchEnabledCacheForTesting(null);
            mWindowAndroid.destroy();
        });
    }

    /**
     * Specifies a value for a fieldtrial param.
     *
     * @param feature The feature that the parameter is bound to.
     * @param param The parameter name.
     * @param value The value of the parameter.
     */
    void setFeatureParam(String feature, String param, String value) {
        mFeatures.addFieldTrialParamOverride(feature, param, value);
    }

    /**
     * Tests for {@link VoiceRecognitionHandler#isVoiceSearchEnabled}.
     */
    @Test
    @SmallTest
    public void testIsVoiceSearchEnabled_FalseOnNullTab() {
        Assert.assertFalse(isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    public void testIsVoiceSearchEnabled_FalseOnNullDataProvider() {
        mDataProvider = null;
        Assert.assertFalse(isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    public void testIsVoiceSearchEnabled_FalseWhenIncognito() {
        mDataProvider.setIncognito(true);
        Assert.assertFalse(isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    public void testIsVoiceSearchEnabled_FalseWhenNoPermissionAndCantRequestPermission() {
        Assert.assertFalse(isVoiceSearchEnabled());
        Assert.assertTrue(mPermissionDelegate.calledHasPermission());
        Assert.assertTrue(mPermissionDelegate.calledCanRequestPermission());
    }

    @Test
    @SmallTest
    public void testIsVoiceSearchEnabled_Success() {
        mPermissionDelegate.setCanRequestPermission(true);
        mPermissionDelegate.setHasPermission(true);
        Assert.assertTrue(isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    public void testIsVoiceSearchEnabled_AllowedByPolicy() {
        RecognitionTestHelper.enableFeatures(
                mFeatures, ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY);
        RecognitionTestHelper.setAudioCapturePref(true);
        mPermissionDelegate.setCanRequestPermission(true);
        mPermissionDelegate.setHasPermission(true);
        Assert.assertTrue(isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    public void testIsVoiceSearchEnabled_DisabledByPolicy() {
        RecognitionTestHelper.enableFeatures(
                mFeatures, ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY);
        RecognitionTestHelper.setAudioCapturePref(false);
        mPermissionDelegate.setCanRequestPermission(true);
        mPermissionDelegate.setHasPermission(true);
        Assert.assertFalse(isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    public void testIsVoiceSearchEnabled_AudioCapturePolicyAllowsByDefault() {
        RecognitionTestHelper.enableFeatures(
                mFeatures, ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY);
        mPermissionDelegate.setCanRequestPermission(true);
        mPermissionDelegate.setHasPermission(true);
        Assert.assertTrue(isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    public void testIsVoiceSearchEnabled_SkipPolicyCheckWhenDisabled() {
        RecognitionTestHelper.disableFeatures(
                mFeatures, ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY);
        RecognitionTestHelper.setAudioCapturePref(false);
        mPermissionDelegate.setCanRequestPermission(true);
        mPermissionDelegate.setHasPermission(true);
        Assert.assertTrue(isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    public void testIsVoiceSearchEnabled_UpdateAfterProfileSet() {
        RecognitionTestHelper.enableFeatures(
                mFeatures, ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY);
        RecognitionTestHelper.setAudioCapturePref(true);
        mPermissionDelegate.setCanRequestPermission(true);
        mPermissionDelegate.setHasPermission(true);
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
        Assert.assertTrue(isVoiceSearchEnabled());

        RecognitionTestHelper.setAudioCapturePref(false);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mProfileSupplier.set(Profile.getLastUsedRegularProfile()); });
        Assert.assertFalse(isVoiceSearchEnabled());
        verify(mObserver).onVoiceAvailabilityImpacted();
    }

    /** Calls isVoiceSearchEnabled(), ensuring it is run on the UI thread. */
    private boolean isVoiceSearchEnabled() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mHandler.isVoiceSearchEnabled());
    }

    /**
     * Tests for {@link VoiceRecognitionHandler#startVoiceRecognition}.
     */
    @Test
    @SmallTest
    public void testStartVoiceRecognition_OnlyUpdateMicButtonStateIfCantRequestPermission() {
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX); });
        Assert.assertEquals(-1, mHandler.getVoiceSearchStartEventSource());
        verify(mObserver).onVoiceAvailabilityImpacted();
        verify(mAssistantVoiceSearchService).reportMicPressUserEligibility();
    }

    @Test
    @SmallTest
    public void
    testStartVoiceRecognition_DontUpdateMicIfPermissionsNotGrantedButCanRequestPermissions() {
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
        mPermissionDelegate.setCanRequestPermission(true);
        mPermissionDelegate.setPermissionResults(PackageManager.PERMISSION_DENIED);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX); });
        Assert.assertEquals(-1, mHandler.getVoiceSearchStartEventSource());
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
        verify(mAssistantVoiceSearchService).reportMicPressUserEligibility();
    }

    @Test
    @SmallTest
    public void
    testStartVoiceRecognition_UpdateMicIfPermissionsNotGrantedAndCantRequestPermissions() {
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
        mPermissionDelegate.setCanRequestPermission(false);
        mPermissionDelegate.setPermissionResults(PackageManager.PERMISSION_DENIED);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX); });
        Assert.assertEquals(-1, mHandler.getVoiceSearchStartEventSource());
        verify(mObserver).onVoiceAvailabilityImpacted();
        verify(mAssistantVoiceSearchService).reportMicPressUserEligibility();
    }

    @Test
    @SmallTest
    public void testStartVoiceRecognition_StartsVoiceSearchWithFailedIntent() {
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
        mWindowAndroid.setCancelableIntentSuccess(false);
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.OMNIBOX);
        Assert.assertEquals(
                VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchStartEventSource());
        Assert.assertEquals(VoiceIntentTarget.SYSTEM, mHandler.getVoiceSearchStartEventTarget());
        verify(mObserver).onVoiceAvailabilityImpacted();

        Assert.assertEquals(
                VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchFailureEventSource());
        Assert.assertEquals(VoiceIntentTarget.SYSTEM, mHandler.getVoiceSearchFailureEventTarget());
    }

    @Test
    @SmallTest
    public void testStartVoiceRecognition_StartsVoiceSearchWithSuccessfulIntent() {
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.OMNIBOX);
        Assert.assertEquals(
                VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchStartEventSource());
        Assert.assertEquals(VoiceIntentTarget.SYSTEM, mHandler.getVoiceSearchStartEventTarget());
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
    }

    /**
     * Tests for the {@link VoiceRecognitionHandler.VoiceRecognitionCompleteCallback}.
     *
     * These tests are kicked off by
     * {@link VoiceRecognitionHandler#startVoiceRecognition} to test the flow as it would
     * be in reality.
     */
    @Test
    @SmallTest
    public void testCallback_noVoiceSearchResultWithBadResultCode() {
        mWindowAndroid.setResultCode(Activity.RESULT_FIRST_USER);
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.NTP);
        Assert.assertEquals(VoiceInteractionSource.NTP, mHandler.getVoiceSearchStartEventSource());
        Assert.assertEquals(null, mHandler.getVoiceSearchResult());
        Assert.assertEquals(
                VoiceInteractionSource.NTP, mHandler.getVoiceSearchFailureEventSource());
        Assert.assertEquals(
                0, mHistograms.getHistogramTotalCount("VoiceInteraction.QueryDuration.Android"));
    }

    @Test
    @SmallTest
    public void testCallback_noVoiceSearchResultCanceled() {
        mWindowAndroid.setResultCode(Activity.RESULT_CANCELED);
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.NTP);
        Assert.assertEquals(VoiceInteractionSource.NTP, mHandler.getVoiceSearchStartEventSource());
        Assert.assertEquals(null, mHandler.getVoiceSearchResult());
        Assert.assertEquals(
                VoiceInteractionSource.NTP, mHandler.getVoiceSearchDismissedEventSource());
        Assert.assertEquals(
                0, mHistograms.getHistogramTotalCount("VoiceInteraction.QueryDuration.Android"));
    }

    @Test
    @SmallTest
    public void testCallback_noVoiceSearchResultWithNullAutocompleteResult() {
        mWindowAndroid.setVoiceResults(new Bundle());
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.SEARCH_WIDGET);
        Assert.assertEquals(
                VoiceInteractionSource.SEARCH_WIDGET, mHandler.getVoiceSearchStartEventSource());
        Assert.assertEquals(false, mHandler.getVoiceSearchResult());
        Assert.assertEquals(
                1, mHistograms.getHistogramTotalCount("VoiceInteraction.QueryDuration.Android"));
    }

    @Test
    @SmallTest
    public void testCallback_noVoiceSearchResultWithNoMatch() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mWindowAndroid.setVoiceResults(RecognitionTestHelper.createDummyBundle("", 1f));
            RecognitionTestHelper.startVoiceRecognition(
                    mPermissionDelegate, mHandler, VoiceInteractionSource.OMNIBOX);
            Assert.assertEquals(
                    VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchStartEventSource());
            Assert.assertEquals(false, mHandler.getVoiceSearchResult());
            Assert.assertEquals(1,
                    mHistograms.getHistogramTotalCount("VoiceInteraction.QueryDuration.Android"));
        });
    }

    @Test
    @SmallTest
    public void testCallback_successWithLowConfidence() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            float confidence =
                    VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD - 0.01f;
            mWindowAndroid.setVoiceResults(
                    RecognitionTestHelper.createDummyBundle("testing", confidence));
            RecognitionTestHelper.startVoiceRecognition(
                    mPermissionDelegate, mHandler, VoiceInteractionSource.OMNIBOX);
            Assert.assertEquals(
                    VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchStartEventSource());
            Assert.assertEquals(
                    VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchFinishEventSource());
            Assert.assertTrue(mHandler.getVoiceSearchResult());
            Assert.assertEquals(VoiceIntentTarget.SYSTEM, mHandler.getVoiceSearchResultTarget());
            Assert.assertTrue(confidence == mHandler.getVoiceConfidenceValue());
            Assert.assertEquals(VoiceIntentTarget.SYSTEM, mHandler.getVoiceConfidenceValueTarget());
            RecognitionTestHelper.assertVoiceResultsAreEqual(
                    mAutocompleteCoordinator.getAutocompleteVoiceResults(),
                    new String[] {"testing"}, new float[] {confidence});
            Assert.assertEquals(1,
                    mHistograms.getHistogramTotalCount("VoiceInteraction.QueryDuration.Android"));
        });
    }

    @Test
    @SmallTest
    public void testCallback_successWithHighConfidence() {
        // Needs to run on the UI thread because we use the TemplateUrlService on success.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mWindowAndroid.setVoiceResults(RecognitionTestHelper.createDummyBundle(
                    "testing", VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD));
            RecognitionTestHelper.startVoiceRecognition(
                    mPermissionDelegate, mHandler, VoiceInteractionSource.OMNIBOX);
            Assert.assertEquals(
                    VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchStartEventSource());
            Assert.assertEquals(
                    VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchFinishEventSource());
            Assert.assertTrue(mHandler.getVoiceSearchResult());
            Assert.assertEquals(VoiceIntentTarget.SYSTEM, mHandler.getVoiceSearchResultTarget());
            Assert.assertTrue(VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD
                    == mHandler.getVoiceConfidenceValue());
            Assert.assertEquals(VoiceIntentTarget.SYSTEM, mHandler.getVoiceConfidenceValueTarget());
            RecognitionTestHelper.assertVoiceResultsAreEqual(
                    mAutocompleteCoordinator.getAutocompleteVoiceResults(),
                    new String[] {"testing"},
                    new float[] {
                            VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD});
            Assert.assertEquals(1,
                    mHistograms.getHistogramTotalCount("VoiceInteraction.QueryDuration.Android"));
        });
    }

    @Test
    @SmallTest
    public void testCallback_successWithLanguages() {
        // Needs to run on the UI thread because we use the TemplateUrlService on success.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mWindowAndroid.setVoiceResults(RecognitionTestHelper.createDummyBundle("testing",
                    VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD, "en-us"));
            RecognitionTestHelper.startVoiceRecognition(
                    mPermissionDelegate, mHandler, VoiceInteractionSource.OMNIBOX);
            Assert.assertEquals(
                    VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchStartEventSource());
            Assert.assertEquals(
                    VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchFinishEventSource());
            Assert.assertTrue(mHandler.getVoiceSearchResult());
            Assert.assertEquals(VoiceIntentTarget.SYSTEM, mHandler.getVoiceSearchResultTarget());
            Assert.assertTrue(VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD
                    == mHandler.getVoiceConfidenceValue());
            Assert.assertEquals(VoiceIntentTarget.SYSTEM, mHandler.getVoiceConfidenceValueTarget());
            RecognitionTestHelper.assertVoiceResultsAreEqual(
                    mAutocompleteCoordinator.getAutocompleteVoiceResults(),
                    new String[] {"testing"},
                    new float[] {
                            VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD},
                    new String[] {"en-us"});
            Assert.assertTrue(mDelegate.getUrl().contains("&hl=en-us"));
        });
    }

    @Test
    @SmallTest
    public void testParseResults_EmptyBundle() {
        Assert.assertNull(mHandler.convertBundleToVoiceResults(new Bundle()));
    }

    @Test
    @SmallTest
    public void testParseResults_MismatchedTextAndConfidenceScores() {
        Assert.assertNull(
                mHandler.convertBundleToVoiceResults(RecognitionTestHelper.createDummyBundle(
                        new String[] {"blah"}, new float[] {0f, 1f})));
        Assert.assertNull(
                mHandler.convertBundleToVoiceResults(RecognitionTestHelper.createDummyBundle(
                        new String[] {"blah", "foo"}, new float[] {7f})));
        Assert.assertNull(
                mHandler.convertBundleToVoiceResults(RecognitionTestHelper.createDummyBundle(
                        new String[] {"blah", "foo"}, new float[] {7f, 1f}, new String[] {"foo"})));
    }

    @Test
    @SmallTest
    public void testParseResults_ValidBundle() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            String[] texts = new String[] {"a", "b", "c"};
            float[] confidences = new float[] {0.8f, 1.0f, 1.0f};

            List<VoiceResult> results = mHandler.convertBundleToVoiceResults(
                    RecognitionTestHelper.createDummyBundle(texts, confidences));

            RecognitionTestHelper.assertVoiceResultsAreEqual(results, texts, confidences);
        });
    }

    @Test
    @SmallTest
    public void testParseResults_VoiceResponseURLConversion() {
        doReturn(false).when(mMatch).isSearchSuggestion();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Needed to interact with classifier.
            // AutocompleteCoordinator#classify() requires a valid profile.
            mProfileSupplier.set(Profile.getLastUsedRegularProfile());

            String[] texts =
                    new String[] {"a", "www. b .co .uk", "engadget .com", "www.google.com"};
            float[] confidences = new float[] {1.0f, 1.0f, 1.0f, 1.0f};
            List<VoiceResult> results = mHandler.convertBundleToVoiceResults(
                    RecognitionTestHelper.createDummyBundle(texts, confidences));

            RecognitionTestHelper.assertVoiceResultsAreEqual(results,
                    new String[] {"a", "www.b.co.uk", "engadget.com", "www.google.com"},
                    new float[] {1.0f, 1.0f, 1.0f, 1.0f});
        });
    }

    @Test
    @SmallTest
    public void testRecordSuccessMetrics_calledWithNullStartTime() {
        mHandler.setQueryStartTimeForTesting(null);
        mHandler.recordSuccessMetrics(VoiceInteractionSource.OMNIBOX, VoiceIntentTarget.SYSTEM,
                AssistantActionPerformed.TRANSCRIPTION);
        Assert.assertEquals(
                0, mHistograms.getHistogramTotalCount("VoiceInteraction.QueryDuration.Android"));
        Assert.assertEquals(0,
                mHistograms.getHistogramTotalCount(
                        "VoiceInteraction.QueryDuration.Android.Transcription"));
    }

    @Test
    @SmallTest
    public void testRecordAudioState_deniedCannotAsk() {
        mPermissionDelegate.setHasPermission(false);
        mPermissionDelegate.setCanRequestPermission(false);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX); });
        Assert.assertEquals(1,
                mHistograms.getHistogramValueCount("VoiceInteraction.AudioPermissionEvent",
                        AudioPermissionState.DENIED_CANNOT_ASK_AGAIN));
    }

    @Test
    @SmallTest
    public void testRecordAudioState_deniedCanAsk() {
        mPermissionDelegate.setCanRequestPermission(true);
        mPermissionDelegate.setPermissionResults(PackageManager.PERMISSION_DENIED);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX); });
        Assert.assertEquals(1,
                mHistograms.getHistogramValueCount("VoiceInteraction.AudioPermissionEvent",
                        AudioPermissionState.DENIED_CAN_ASK_AGAIN));
    }

    @Test
    @SmallTest
    public void testRecordAudioState_granted() {
        mPermissionDelegate.setHasPermission(true);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX); });
        Assert.assertEquals(1,
                mHistograms.getHistogramValueCount(
                        "VoiceInteraction.AudioPermissionEvent", AudioPermissionState.GRANTED));
    }

    @Test
    @SmallTest
    public void testCallback_CalledTwice() {
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.NTP);
        Assert.assertEquals(-1, mHandler.getVoiceSearchUnexpectedResultSource());

        IntentCallback callback = mWindowAndroid.getIntentCallback();
        callback.onIntentCompleted(Activity.RESULT_CANCELED, null);
        Assert.assertEquals(
                VoiceInteractionSource.NTP, mHandler.getVoiceSearchUnexpectedResultSource());
    }
}
