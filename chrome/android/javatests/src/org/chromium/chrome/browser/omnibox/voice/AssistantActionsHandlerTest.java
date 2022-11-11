// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Intent;
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
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.TranslateBridgeWrapper;
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
 * Assistant actions tests for {@link VoiceRecognitionHandler}. See
 * {@link AssistantVoiceRecognitionHandlerTest} and
 * {@link VoiceRecognitionHandlerTests}
 * for the other tests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AssistantActionsHandlerTest {
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
    TranslateBridgeWrapper mTranslateBridgeWrapper;
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
        doReturn(mIntent).when(mAssistantVoiceSearchService).getAssistantVoiceSearchIntent();
        doReturn(RecognitionTestHelper.DEFAULT_USER_EMAIL)
                .when(mAssistantVoiceSearchService)
                .getUserEmail();

        doReturn(true).when(mTranslateBridgeWrapper).canManuallyTranslate(notNull());
        doReturn("fr").when(mTranslateBridgeWrapper).getSourceLanguage(notNull());
        doReturn("de").when(mTranslateBridgeWrapper).getCurrentLanguage(notNull());
        doReturn("ja").when(mTranslateBridgeWrapper).getTargetLanguage();
        mHandler.setTranslateBridgeWrapper(mTranslateBridgeWrapper);

        mFeatures = RecognitionTestHelper.resetTestFeatures();
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertNotNull(mHandler);
            mHandler.removeObserver(mObserver);
            VoiceRecognitionHandler.setIsRecognitionIntentPresentForTesting(null);
            mHandler.setIsVoiceSearchEnabledCacheForTesting(null);
            mWindowAndroid.destroy();
        });
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentPageUrl")
    public void testStartVoiceRecognition_ToolbarButtonIncludesPageUrlForPersonalizedVoiceSearch() {
        RecognitionTestHelper.enableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL);
        RecognitionTestHelper.disableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH);

        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        Assert.assertEquals(mIntent, mWindowAndroid.getCancelableIntent());
        verify(mIntent).putExtra(VoiceRecognitionHandler.EXTRA_INTENT_USER_EMAIL,
                RecognitionTestHelper.DEFAULT_USER_EMAIL);
        verify(mIntent).putExtra(
                VoiceRecognitionHandler.EXTRA_PAGE_URL, RecognitionTestHelper.DEFAULT_URL);
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentPageUrl")
    public void testStartVoiceRecognition_OmitPageUrlForNonPersonalizedVoiceSearch() {
        RecognitionTestHelper.enableFeatures(mFeatures, ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL,
                ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH);
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        Assert.assertEquals(mIntent, mWindowAndroid.getCancelableIntent());
        verify(mIntent, never())
                .putExtra(
                        VoiceRecognitionHandler.EXTRA_PAGE_URL, RecognitionTestHelper.DEFAULT_URL);
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentPageUrl")
    public void testStartVoiceRecognition_OmitPageUrlWhenAssistantVoiceSearchDisabled() {
        RecognitionTestHelper.enableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL);
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(false).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();

        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        verify(mIntent, never()).putExtra(eq(VoiceRecognitionHandler.EXTRA_PAGE_URL), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentPageUrl")
    public void testStartVoiceRecognition_OmitPageUrlForNonToolbar() {
        RecognitionTestHelper.enableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL);
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();

        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.NTP);

        verify(mIntent, never()).putExtra(eq(VoiceRecognitionHandler.EXTRA_PAGE_URL), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentPageUrl")
    public void testStartVoiceRecognition_OmitPageUrlForIncognito() {
        RecognitionTestHelper.enableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL);
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        doReturn(true).when(mTab).isIncognito();

        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);

        verify(mIntent, never()).putExtra(eq(VoiceRecognitionHandler.EXTRA_PAGE_URL), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentPageUrl")
    public void testStartVoiceRecognition_OmitPageUrlForInternalPages() {
        RecognitionTestHelper.enableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL);
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        GURL url = new GURL("chrome://version");
        doReturn(url).when(mTab).getUrl();

        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);

        verify(mIntent, never()).putExtra(eq(VoiceRecognitionHandler.EXTRA_PAGE_URL), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentPageUrl")
    public void testStartVoiceRecognition_OmitPageUrlForNonHttp() {
        RecognitionTestHelper.enableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL);
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        GURL url = new GURL("ftp://example.org/");
        doReturn(url).when(mTab).getUrl();

        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);

        verify(mIntent, never()).putExtra(eq(VoiceRecognitionHandler.EXTRA_PAGE_URL), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentTranslateInfo")
    public void testStartVoiceRecognition_ToolbarButtonIncludesTranslateInfo() {
        RecognitionTestHelper.enableFeatures(mFeatures,
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH,
                ChromeFeatureList.ASSISTANT_INTENT_TRANSLATE_INFO);
        RecognitionTestHelper.disableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH);
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        Assert.assertEquals(mIntent, mWindowAndroid.getCancelableIntent());
        verify(mIntent).putExtra(VoiceRecognitionHandler.EXTRA_TRANSLATE_ORIGINAL_LANGUAGE, "fr");
        verify(mIntent).putExtra(VoiceRecognitionHandler.EXTRA_TRANSLATE_CURRENT_LANGUAGE, "de");
        verify(mIntent).putExtra(VoiceRecognitionHandler.EXTRA_TRANSLATE_TARGET_LANGUAGE, "ja");
        verify(mIntent).putExtra(
                VoiceRecognitionHandler.EXTRA_PAGE_URL, RecognitionTestHelper.DEFAULT_URL);
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentTranslateInfo")
    public void testStartVoiceRecognition_TranslateExtrasDisabled() {
        RecognitionTestHelper.disableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_INTENT_TRANSLATE_INFO);
        RecognitionTestHelper.enableFeatures(
                mFeatures, ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH);
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        verify(mIntent, never())
                .putExtra(
                        eq(VoiceRecognitionHandler.EXTRA_TRANSLATE_ORIGINAL_LANGUAGE), anyString());
        verify(mIntent, never())
                .putExtra(
                        eq(VoiceRecognitionHandler.EXTRA_TRANSLATE_CURRENT_LANGUAGE), anyString());
        verify(mIntent, never())
                .putExtra(eq(VoiceRecognitionHandler.EXTRA_TRANSLATE_TARGET_LANGUAGE), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentTranslateInfo")
    public void testStartVoiceRecognition_NoTranslateExtrasForNonToolbar() {
        RecognitionTestHelper.enableFeatures(mFeatures,
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH,
                ChromeFeatureList.ASSISTANT_INTENT_TRANSLATE_INFO);
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.OMNIBOX);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        verify(mIntent, never())
                .putExtra(
                        eq(VoiceRecognitionHandler.EXTRA_TRANSLATE_ORIGINAL_LANGUAGE), anyString());
        verify(mIntent, never())
                .putExtra(
                        eq(VoiceRecognitionHandler.EXTRA_TRANSLATE_CURRENT_LANGUAGE), anyString());
        verify(mIntent, never())
                .putExtra(eq(VoiceRecognitionHandler.EXTRA_TRANSLATE_TARGET_LANGUAGE), anyString());
        verify(mIntent, never()).putExtra(eq(VoiceRecognitionHandler.EXTRA_PAGE_URL), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentTranslateInfo")
    public void testStartVoiceRecognition_NoTranslateExtrasForNonPersonalizedSearch() {
        RecognitionTestHelper.enableFeatures(mFeatures,
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH,
                ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH,
                ChromeFeatureList.ASSISTANT_INTENT_TRANSLATE_INFO);
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        verify(mIntent, never())
                .putExtra(
                        eq(VoiceRecognitionHandler.EXTRA_TRANSLATE_ORIGINAL_LANGUAGE), anyString());
        verify(mIntent, never())
                .putExtra(
                        eq(VoiceRecognitionHandler.EXTRA_TRANSLATE_CURRENT_LANGUAGE), anyString());
        verify(mIntent, never())
                .putExtra(eq(VoiceRecognitionHandler.EXTRA_TRANSLATE_TARGET_LANGUAGE), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentTranslateInfo")
    public void testStartVoiceRecognition_NoTranslateExtrasForNonTranslatePage() {
        RecognitionTestHelper.enableFeatures(mFeatures,
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH,
                ChromeFeatureList.ASSISTANT_INTENT_TRANSLATE_INFO);
        RecognitionTestHelper.disableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH);
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        doReturn(false).when(mTranslateBridgeWrapper).canManuallyTranslate(notNull());
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        verify(mIntent, never())
                .putExtra(
                        eq(VoiceRecognitionHandler.EXTRA_TRANSLATE_ORIGINAL_LANGUAGE), anyString());
        verify(mIntent, never())
                .putExtra(
                        eq(VoiceRecognitionHandler.EXTRA_TRANSLATE_CURRENT_LANGUAGE), anyString());
        verify(mIntent, never())
                .putExtra(eq(VoiceRecognitionHandler.EXTRA_TRANSLATE_TARGET_LANGUAGE), anyString());
        verify(mIntent, never()).putExtra(eq(VoiceRecognitionHandler.EXTRA_PAGE_URL), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentTranslateInfo")
    public void testStartVoiceRecognition_NoTranslateExtrasWhenLanguagesUndetected() {
        RecognitionTestHelper.enableFeatures(mFeatures,
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH,
                ChromeFeatureList.ASSISTANT_INTENT_TRANSLATE_INFO);
        RecognitionTestHelper.disableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH);
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        doReturn(null).when(mTranslateBridgeWrapper).getSourceLanguage(notNull());
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        verify(mIntent, never())
                .putExtra(
                        eq(VoiceRecognitionHandler.EXTRA_TRANSLATE_ORIGINAL_LANGUAGE), anyString());
        verify(mIntent, never())
                .putExtra(
                        eq(VoiceRecognitionHandler.EXTRA_TRANSLATE_CURRENT_LANGUAGE), anyString());
        verify(mIntent, never())
                .putExtra(eq(VoiceRecognitionHandler.EXTRA_TRANSLATE_TARGET_LANGUAGE), anyString());
        verify(mIntent, never()).putExtra(eq(VoiceRecognitionHandler.EXTRA_PAGE_URL), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentTranslateInfo")
    public void testStartVoiceRecognition_TranslateInfoTargetLanguageOptional() {
        RecognitionTestHelper.enableFeatures(mFeatures,
                ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH,
                ChromeFeatureList.ASSISTANT_INTENT_TRANSLATE_INFO);
        RecognitionTestHelper.disableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH);
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        doReturn(null).when(mTranslateBridgeWrapper).getTargetLanguage();
        RecognitionTestHelper.startVoiceRecognition(
                mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        verify(mIntent).putExtra(VoiceRecognitionHandler.EXTRA_TRANSLATE_ORIGINAL_LANGUAGE, "fr");
        verify(mIntent).putExtra(VoiceRecognitionHandler.EXTRA_TRANSLATE_CURRENT_LANGUAGE, "de");
        verify(mIntent, never())
                .putExtra(eq(VoiceRecognitionHandler.EXTRA_TRANSLATE_TARGET_LANGUAGE), anyString());
        verify(mIntent).putExtra(
                VoiceRecognitionHandler.EXTRA_PAGE_URL, RecognitionTestHelper.DEFAULT_URL);
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentPageUrl")
    public void testCallback_nonTranscriptionAction() {
        RecognitionTestHelper.enableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Bundle bundle = new Bundle();
            bundle.putString(VoiceRecognitionHandler.EXTRA_ACTION_PERFORMED, "TRANSLATE");

            mWindowAndroid.setVoiceResults(bundle);
            RecognitionTestHelper.startVoiceRecognition(
                    mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);
            Assert.assertEquals(
                    VoiceInteractionSource.TOOLBAR, mHandler.getVoiceSearchStartEventSource());
            Assert.assertEquals(
                    VoiceInteractionSource.TOOLBAR, mHandler.getVoiceSearchFinishEventSource());
            Assert.assertEquals(
                    AssistantActionPerformed.TRANSLATE, mHandler.getAssistantActionPerformed());
            Assert.assertEquals(
                    VoiceInteractionSource.TOOLBAR, mHandler.getAssistantActionPerformedSource());
            Assert.assertEquals(1,
                    mHistograms.getHistogramTotalCount("VoiceInteraction.QueryDuration.Android"));
            Assert.assertEquals(1,
                    mHistograms.getHistogramTotalCount("VoiceInteraction.QueryDuration.Android"));
        });
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentPageUrl")
    public void testCallback_defaultToTranscription() {
        RecognitionTestHelper.enableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL);
        RecognitionTestHelper.disableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mWindowAndroid.setVoiceResults(RecognitionTestHelper.createDummyBundle(
                    "testing", VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD));
            RecognitionTestHelper.startVoiceRecognition(
                    mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);
            Assert.assertEquals(
                    AssistantActionPerformed.TRANSCRIPTION, mHandler.getAssistantActionPerformed());
            Assert.assertEquals(
                    VoiceInteractionSource.TOOLBAR, mHandler.getAssistantActionPerformedSource());
            Assert.assertTrue(mHandler.getVoiceSearchResult());
            RecognitionTestHelper.assertVoiceResultsAreEqual(
                    mAutocompleteCoordinator.getAutocompleteVoiceResults(),
                    new String[] {"testing"},
                    new float[] {
                            VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD});
        });
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentPageUrl")
    public void testCallback_pageUrlExtraDisabled() {
        RecognitionTestHelper.disableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mWindowAndroid.setVoiceResults(RecognitionTestHelper.createDummyBundle(
                    "testing", VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD));
            RecognitionTestHelper.startVoiceRecognition(
                    mPermissionDelegate, mHandler, VoiceInteractionSource.TOOLBAR);
            Assert.assertTrue(mHandler.getVoiceSearchResult());
            // Ensure that we don't record UMA when the feature is disabled.
            Assert.assertEquals(-1, mHandler.getAssistantActionPerformed());
            Assert.assertEquals(-1, mHandler.getAssistantActionPerformedSource());
        });
    }

    @Test
    @SmallTest
    public void testRecordSuccessMetrics_noActionMetrics() {
        RecognitionTestHelper.disableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL);
        mHandler.setQueryStartTimeForTesting(100L);
        mHandler.recordSuccessMetrics(VoiceInteractionSource.OMNIBOX, VoiceIntentTarget.ASSISTANT,
                AssistantActionPerformed.TRANSCRIPTION);
        Assert.assertEquals(
                VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchFinishEventSource());
        Assert.assertEquals(
                VoiceIntentTarget.ASSISTANT, mHandler.getVoiceSearchFinishEventTarget());
        Assert.assertEquals(-1, mHandler.getAssistantActionPerformed());
        Assert.assertEquals(-1, mHandler.getAssistantActionPerformedSource());
        Assert.assertEquals(
                1, mHistograms.getHistogramTotalCount("VoiceInteraction.QueryDuration.Android"));
        // Split action metrics should not be recorded.
        Assert.assertEquals(0,
                mHistograms.getHistogramTotalCount(
                        "VoiceInteraction.QueryDuration.Android.Transcription"));
    }

    @Test
    @SmallTest
    public void testRecordSuccessMetrics_splitActionMetrics() {
        RecognitionTestHelper.enableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL);
        RecognitionTestHelper.disableFeatures(
                mFeatures, ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH);
        mHandler.setQueryStartTimeForTesting(100L);
        mHandler.recordSuccessMetrics(VoiceInteractionSource.OMNIBOX, VoiceIntentTarget.ASSISTANT,
                AssistantActionPerformed.TRANSLATE);
        Assert.assertEquals(
                VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchFinishEventSource());
        Assert.assertEquals(
                VoiceIntentTarget.ASSISTANT, mHandler.getVoiceSearchFinishEventTarget());
        Assert.assertEquals(
                AssistantActionPerformed.TRANSLATE, mHandler.getAssistantActionPerformed());
        Assert.assertEquals(
                VoiceInteractionSource.OMNIBOX, mHandler.getAssistantActionPerformedSource());
        Assert.assertEquals(
                1, mHistograms.getHistogramTotalCount("VoiceInteraction.QueryDuration.Android"));
        Assert.assertEquals(0,
                mHistograms.getHistogramTotalCount(
                        "VoiceInteraction.QueryDuration.Android.Transcription"));
        Assert.assertEquals(1,
                mHistograms.getHistogramTotalCount(
                        "VoiceInteraction.QueryDuration.Android.Translate"));
    }
}
