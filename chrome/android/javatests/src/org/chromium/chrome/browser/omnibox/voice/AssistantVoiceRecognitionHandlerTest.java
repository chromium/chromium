// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Intent;

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
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceIntentTarget;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceInteractionSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.util.concurrent.ExecutionException;

/**
 * Recognition tests with Assistant for {@link VoiceRecognitionHandler}. See
 * {@link AssistantActionsHandlerTest} and {@link VoiceRecognitionHandlerTests}
 * for the other tests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AssistantVoiceRecognitionHandlerTest {
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
        });

        doReturn(new GURL(RecognitionTestHelper.DEFAULT_URL)).when(mTab).getUrl();

        doReturn(false).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        doReturn(false).when(mAssistantVoiceSearchService).needsEnabledCheck();
        doReturn(mIntent).when(mAssistantVoiceSearchService).getAssistantVoiceSearchIntent();
        doReturn(RecognitionTestHelper.DEFAULT_USER_EMAIL)
                .when(mAssistantVoiceSearchService)
                .getUserEmail();

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

    @Test
    @SmallTest
    @Feature({"OmniboxAssistantVoiceSearch"})
    public void testStartVoiceRecognition_StartsPersonalizedAssistantVoiceSearch() {
        RecognitionTestHelper.enableFeatures(
                mFeatures, ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH);
        RecognitionTestHelper.disableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH);
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.OMNIBOX);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        Assert.assertEquals(mIntent, mWindowAndroid.getCancelableIntent());
        Assert.assertEquals(VoiceIntentTarget.ASSISTANT, mHandler.getVoiceSearchStartEventTarget());
        verify(mAssistantVoiceSearchService).reportMicPressUserEligibility();
        verify(mIntent).putExtra(
                eq(VoiceRecognitionHandler.EXTRA_INTENT_SENT_TIMESTAMP), anyLong());
        verify(mIntent).putExtra(
                VoiceRecognitionHandler.EXTRA_VOICE_ENTRYPOINT, VoiceInteractionSource.OMNIBOX);
        verify(mIntent).putExtra(VoiceRecognitionHandler.EXTRA_INTENT_USER_EMAIL,
                RecognitionTestHelper.DEFAULT_USER_EMAIL);
    }

    @Test
    @SmallTest
    @Feature({"OmniboxAssistantVoiceSearch"})
    public void testStartVoiceRecognition_StartsNonPersonalizedAssistantVoiceSearch() {
        RecognitionTestHelper.enableFeatures(mFeatures,
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH,
                ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH);
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.OMNIBOX);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        Assert.assertEquals(mIntent, mWindowAndroid.getCancelableIntent());
        Assert.assertEquals(VoiceIntentTarget.ASSISTANT, mHandler.getVoiceSearchStartEventTarget());
        verify(mAssistantVoiceSearchService).reportMicPressUserEligibility();
        verify(mIntent).putExtra(
                eq(VoiceRecognitionHandler.EXTRA_INTENT_SENT_TIMESTAMP), anyLong());
        verify(mIntent).putExtra(
                VoiceRecognitionHandler.EXTRA_VOICE_ENTRYPOINT, VoiceInteractionSource.OMNIBOX);
        verify(mIntent, never())
                .putExtra(VoiceRecognitionHandler.EXTRA_INTENT_USER_EMAIL,
                        RecognitionTestHelper.DEFAULT_USER_EMAIL);
    }

    @Test
    @SmallTest
    @Feature({"OmniboxAssistantVoiceSearch"})
    public void testStartVoiceRecognition_ShouldRequestConditionsFail() {
        RecognitionTestHelper.enableFeatures(
                mFeatures, ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH);
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(false).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.OMNIBOX);

        verify(mAssistantVoiceSearchService).reportMicPressUserEligibility();
        // We check for the consent dialog when canRequestAssistantVoiceSearch() is
        // true.
        verify(mAssistantVoiceSearchService).needsEnabledCheck();
        verify(mAssistantVoiceSearchService, times(0)).getAssistantVoiceSearchIntent();
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentExperimentId")
    public void testStartVoiceRecognition_AssistantExperimentIdDisabled() {
        RecognitionTestHelper.enableFeatures(
                mFeatures, ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH);
        RecognitionTestHelper.disableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID);
        setFeatureParam(ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID,
                VoiceRecognitionHandler.ASSISTANT_EXPERIMENT_ID_PARAM_NAME, "test");

        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        verify(mIntent, never())
                .putExtra(eq(VoiceRecognitionHandler.EXTRA_EXPERIMENT_ID), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentExperimentId")
    public void testStartVoiceRecognition_IncludeExperimentIdInAssistantIntentFromToolbar() {
        RecognitionTestHelper.enableFeatures(mFeatures,
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH,
                ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID);
        setFeatureParam(ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID,
                VoiceRecognitionHandler.ASSISTANT_EXPERIMENT_ID_PARAM_NAME, "test");
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        verify(mIntent).putExtra(VoiceRecognitionHandler.EXTRA_EXPERIMENT_ID, "test");
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentExperimentId")
    public void testStartVoiceRecognition_IncludeExperimentIdInAssistantIntentFromNonToolbar() {
        RecognitionTestHelper.enableFeatures(mFeatures,
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH,
                ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID);
        setFeatureParam(ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID,
                VoiceRecognitionHandler.ASSISTANT_EXPERIMENT_ID_PARAM_NAME, "test");
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.OMNIBOX);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        verify(mIntent).putExtra(VoiceRecognitionHandler.EXTRA_EXPERIMENT_ID, "test");
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    public void testStartVoiceRecognition_AudioCaptureAllowedByPolicy() {
        RecognitionTestHelper.enableFeatures(mFeatures,
                ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY,
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH);
        RecognitionTestHelper.setAudioCapturePref(true);
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    public void testStartVoiceRecognition_AudioCaptureDisabledByPolicy() {
        RecognitionTestHelper.enableFeatures(mFeatures,
                ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY,
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH);
        RecognitionTestHelper.setAudioCapturePref(false);
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);

        Assert.assertFalse(mWindowAndroid.wasCancelableIntentShown());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    public void testStartVoiceRecognition_AudioCapturePolicyAllowsByDefault() {
        RecognitionTestHelper.enableFeatures(mFeatures,
                ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY,
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH);
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    public void testStartVoiceRecognition_SkipPolicyWhenFeatureDisabled() {
        RecognitionTestHelper.disableFeatures(
                mFeatures, ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY);
        RecognitionTestHelper.enableFeatures(
                mFeatures, ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH);
        RecognitionTestHelper.setAudioCapturePref(false);
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
    }

    /**
     * Specifies a value for a fieldtrial param.
     *
     * @param feature The feature that the parameter is bound to.
     * @param param   The parameter name.
     * @param value   The value of the parameter.
     */
    void setFeatureParam(String feature, String param, String value) {
        mFeatures.addFieldTrialParamOverride(feature, param, value);
    }
}
