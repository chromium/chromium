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
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.EXTRA_EXPERIMENT_ID;
import static org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.EXTRA_INTENT_SENT_TIMESTAMP;
import static org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.EXTRA_INTENT_USER_EMAIL;
import static org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.EXTRA_VOICE_ENTRYPOINT;

import android.content.Intent;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.FeatureList;
import org.chromium.base.StrictModeContext;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.metrics.HistogramTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceIntentTarget;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceInteractionSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
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
    private static final String DEFAULT_URL = "https://example.com/";
    private static final String DEFAULT_USER_EMAIL = "test@test.com";

    public static @ClassRule ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();
    public @Rule HistogramTestRule mHistograms = new HistogramTestRule();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock Intent mIntent;
    private @Mock AssistantVoiceSearchService mAssistantVoiceSearchService;
    private @Mock Tab mTab;
    private @Mock LocationBarDataProvider mDataProvider;
    private @Mock VoiceRecognitionHandler.Delegate mDelegate;
    private @Mock AutocompleteController mController;
    private @Mock AutocompleteCoordinator mAutocompleteCoordinator;
    private @Mock AutocompleteMatch mMatch;
    private @Mock AndroidPermissionDelegate mPermissionDelegate;

    private VoiceRecognitionHandler mHandler;

    private WindowAndroid mWindowAndroid;
    private ObservableSupplierImpl<Profile> mProfileSupplier;
    private FeatureList.TestValues mFeatures;

    @BeforeClass
    public static void setUpClass() {
        sActivityTestRule.startMainActivityOnBlankPage();
        sActivityTestRule.waitForDeferredStartup();
    }

    @Before
    public void setUp() throws InterruptedException, ExecutionException {
        AutocompleteControllerProvider.setControllerForTesting(mController);
        doReturn(mMatch).when(mController).classify(any(), anyBoolean());
        doReturn(new GURL("https://www.google.com/search?q=abc")).when(mMatch).getUrl();
        doReturn(true).when(mMatch).isSearchSuggestion();
        doReturn(true).when(mPermissionDelegate).hasPermission(anyString());

        var activity = sActivityTestRule.getActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mProfileSupplier = new ObservableSupplierImpl<>();
            try (var ignore = StrictModeContext.allowAllThreadPolicies()) {
                mWindowAndroid = spy(new WindowAndroid(activity));
                mHandler = spy(new VoiceRecognitionHandler(
                        mDelegate, () -> mAssistantVoiceSearchService, () -> {}, mProfileSupplier));
            }
        });

        mWindowAndroid.setAndroidPermissionDelegate(mPermissionDelegate);
        doReturn(new WeakReference(activity)).when(mWindowAndroid).getActivity();
        doReturn(new GURL(DEFAULT_URL)).when(mTab).getUrl();
        doReturn(mDataProvider).when(mDelegate).getLocationBarDataProvider();
        doReturn(mAutocompleteCoordinator).when(mDelegate).getAutocompleteCoordinator();
        doReturn(mWindowAndroid).when(mDelegate).getWindowAndroid();

        doReturn(false).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        doReturn(false).when(mAssistantVoiceSearchService).needsEnabledCheck();
        doReturn(mIntent).when(mAssistantVoiceSearchService).getAssistantVoiceSearchIntent();
        doReturn(DEFAULT_USER_EMAIL).when(mAssistantVoiceSearchService).getUserEmail();

        mFeatures = new FeatureList.TestValues();
        FeatureList.setTestValues(mFeatures);
        mFeatures.addFeatureFlagOverride(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH, true);
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            RecognitionTestHelper.setAudioCapturePref(true);
            VoiceRecognitionHandler.setIsRecognitionIntentPresentForTesting(null);
            mWindowAndroid.destroy();
        });
        AutocompleteControllerProvider.setControllerForTesting(null);
        FeatureList.setTestValues(null);
    }

    @Test
    @SmallTest
    @Feature({"OmniboxAssistantVoiceSearch"})
    @UiThreadTest
    public void testStartVoiceRecognition_StartsPersonalizedAssistantVoiceSearch() {
        mFeatures.addFeatureFlagOverride(
                ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH, false);

        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();

        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);

        verify(mWindowAndroid, times(1)).showCancelableIntent(eq(mIntent), any(), any());
        verify(mHandler, times(1))
                .recordVoiceSearchStartEvent(
                        eq(VoiceInteractionSource.OMNIBOX), eq(VoiceIntentTarget.ASSISTANT));
        verify(mAssistantVoiceSearchService).reportMicPressUserEligibility();
        verify(mIntent).putExtra(eq(EXTRA_INTENT_SENT_TIMESTAMP), anyLong());
        verify(mIntent).putExtra(EXTRA_VOICE_ENTRYPOINT, VoiceInteractionSource.OMNIBOX);
        verify(mIntent).putExtra(EXTRA_INTENT_USER_EMAIL, DEFAULT_USER_EMAIL);
    }

    @Test
    @SmallTest
    @Feature({"OmniboxAssistantVoiceSearch"})
    @UiThreadTest
    public void testStartVoiceRecognition_StartsNonPersonalizedAssistantVoiceSearch() {
        mFeatures.addFeatureFlagOverride(
                ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH, true);

        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();

        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);

        verify(mWindowAndroid, times(1)).showCancelableIntent(eq(mIntent), any(), any());
        verify(mHandler, times(1))
                .recordVoiceSearchStartEvent(
                        eq(VoiceInteractionSource.OMNIBOX), eq(VoiceIntentTarget.ASSISTANT));
        verify(mAssistantVoiceSearchService).reportMicPressUserEligibility();
        verify(mIntent).putExtra(eq(EXTRA_INTENT_SENT_TIMESTAMP), anyLong());
        verify(mIntent).putExtra(EXTRA_VOICE_ENTRYPOINT, VoiceInteractionSource.OMNIBOX);
        verify(mIntent, never()).putExtra(EXTRA_INTENT_USER_EMAIL, DEFAULT_USER_EMAIL);
    }

    @Test
    @SmallTest
    @Feature({"OmniboxAssistantVoiceSearch"})
    @UiThreadTest
    public void testStartVoiceRecognition_ShouldRequestConditionsFail() {
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(false).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
        verify(mAssistantVoiceSearchService).reportMicPressUserEligibility();
        // We check for the consent dialog when canRequestAssistantVoiceSearch() is
        // true.
        verify(mAssistantVoiceSearchService).needsEnabledCheck();
        verify(mAssistantVoiceSearchService, times(0)).getAssistantVoiceSearchIntent();
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentExperimentId")
    @UiThreadTest
    public void testStartVoiceRecognition_AssistantExperimentIdDisabled() {
        mFeatures.addFeatureFlagOverride(ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID, false);
        mFeatures.addFieldTrialParamOverride(ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID,
                VoiceRecognitionHandler.ASSISTANT_EXPERIMENT_ID_PARAM_NAME, "test");

        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        mHandler.startVoiceRecognition(VoiceInteractionSource.TOOLBAR);
        verify(mWindowAndroid, times(1)).showCancelableIntent(eq(mIntent), any(), any());
        verify(mIntent, never()).putExtra(eq(EXTRA_EXPERIMENT_ID), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentExperimentId")
    @UiThreadTest
    public void testStartVoiceRecognition_IncludeExperimentIdInAssistantIntentFromToolbar() {
        mFeatures.addFeatureFlagOverride(ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID, true);
        mFeatures.addFieldTrialParamOverride(ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID,
                VoiceRecognitionHandler.ASSISTANT_EXPERIMENT_ID_PARAM_NAME, "test");

        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        mHandler.startVoiceRecognition(VoiceInteractionSource.TOOLBAR);
        verify(mWindowAndroid, times(1)).showCancelableIntent(eq(mIntent), any(), any());
        verify(mIntent).putExtra(EXTRA_EXPERIMENT_ID, "test");
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentExperimentId")
    @UiThreadTest
    public void testStartVoiceRecognition_IncludeExperimentIdInAssistantIntentFromNonToolbar() {
        mFeatures.addFeatureFlagOverride(ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID, true);
        mFeatures.addFieldTrialParamOverride(ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID,
                VoiceRecognitionHandler.ASSISTANT_EXPERIMENT_ID_PARAM_NAME, "test");

        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
        verify(mWindowAndroid, times(1)).showCancelableIntent(eq(mIntent), any(), any());
        verify(mIntent).putExtra(EXTRA_EXPERIMENT_ID, "test");
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    @UiThreadTest
    public void testStartVoiceRecognition_AudioCaptureAllowedByPolicy() {
        mFeatures.addFeatureFlagOverride(ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY, true);

        RecognitionTestHelper.setAudioCapturePref(true);
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        mHandler.startVoiceRecognition(VoiceInteractionSource.TOOLBAR);
        verify(mWindowAndroid, times(1)).showCancelableIntent(any(Intent.class), any(), any());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    @UiThreadTest
    public void testStartVoiceRecognition_AudioCaptureDisabledByPolicy() {
        mFeatures.addFeatureFlagOverride(ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY, true);

        RecognitionTestHelper.setAudioCapturePref(false);
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        mHandler.startVoiceRecognition(VoiceInteractionSource.TOOLBAR);
        verify(mWindowAndroid, never()).showCancelableIntent(any(Intent.class), any(), any());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    @UiThreadTest
    public void testStartVoiceRecognition_AudioCapturePolicyAllowsByDefault() {
        mFeatures.addFeatureFlagOverride(ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY, true);

        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        mHandler.startVoiceRecognition(VoiceInteractionSource.TOOLBAR);
        verify(mWindowAndroid, times(1)).showCancelableIntent(any(Intent.class), any(), any());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    @UiThreadTest
    public void testStartVoiceRecognition_SkipPolicyWhenFeatureDisabled() {
        mFeatures.addFeatureFlagOverride(
                ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY, false);

        RecognitionTestHelper.setAudioCapturePref(false);
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        mHandler.startVoiceRecognition(VoiceInteractionSource.TOOLBAR);
        verify(mWindowAndroid, times(1)).showCancelableIntent(any(Intent.class), any(), any());
    }
}
