// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.speech.RecognizerIntent;
import android.view.ViewGroup;

import androidx.annotation.ColorRes;
import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinatorImpl;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteResult;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdown;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceInteractionSource;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceResult;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.OmniboxTestUtils.SuggestionsResult;
import org.chromium.chrome.test.util.OmniboxTestUtils.TestAutocompleteController;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.base.PermissionCallback;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.IntentCallback;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Tests for {@link VoiceRecognitionHandler}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class VoiceRecognitionHandlerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock
    Intent mIntent;
    @Mock
    AssistantVoiceSearchService mAssistantVoiceSearchService;

    private TestDataProvider mDataProvider;
    private TestDelegate mDelegate;
    private TestVoiceRecognitionHandler mHandler;
    private TestAutocompleteController mAutocomplete;
    private TestAndroidPermissionDelegate mPermissionDelegate;
    private TestWindowAndroid mWindowAndroid;
    private Tab mTab;
    private List<VoiceResult> mAutocompleteVoiceResults;

    private static final OnSuggestionsReceivedListener sEmptySuggestionListener =
            new OnSuggestionsReceivedListener() {
                @Override
                public void onSuggestionsReceived(
                        AutocompleteResult autocompleteResult, String inlineAutocompleteText) {}
            };

    /**
     * An implementation of the real {@link VoiceRecognitionHandler} except instead of
     * recording histograms we just flag whether we would have or not.
     */
    private class TestVoiceRecognitionHandler extends VoiceRecognitionHandler {
        @VoiceInteractionSource
        private int mStartSource = -1;
        @VoiceInteractionSource
        private int mFinishSource = -1;
        @VoiceInteractionSource
        private int mDismissedSource = -1;
        @VoiceInteractionSource
        private int mFailureSource = -1;
        @VoiceInteractionSource
        private int mUnexpectedResultSource = -1;
        private Boolean mResult;
        private Float mVoiceConfidenceValue;

        public TestVoiceRecognitionHandler(Delegate delegate) {
            super(delegate);
        }

        @Override
        protected void recordVoiceSearchStartEventSource(@VoiceInteractionSource int source) {
            mStartSource = source;
        }

        @Override
        protected void recordVoiceSearchFinishEventSource(@VoiceInteractionSource int source) {
            mFinishSource = source;
        }

        @Override
        protected void recordVoiceSearchFailureEventSource(@VoiceInteractionSource int source) {
            mFailureSource = source;
        }

        @Override
        protected void recordVoiceSearchDismissedEventSource(@VoiceInteractionSource int source) {
            mDismissedSource = source;
        }

        @Override
        protected void recordVoiceSearchUnexpectedResultSource(@VoiceInteractionSource int source) {
            mUnexpectedResultSource = source;
        }

        @Override
        protected void recordVoiceSearchResult(boolean result) {
            mResult = result;
        }

        @Override
        protected void recordVoiceSearchConfidenceValue(float value) {
            mVoiceConfidenceValue = value;
        }

        @Override
        protected boolean isRecognitionIntentPresent(boolean useCachedValue) {
            return true;
        }

        @VoiceInteractionSource
        public int getVoiceSearchStartEventSource() {
            return mStartSource;
        }

        @VoiceInteractionSource
        public int getVoiceSearchFinishEventSource() {
            return mFinishSource;
        }

        @VoiceInteractionSource
        public int getVoiceSearchDismissedEventSource() {
            return mDismissedSource;
        }

        @VoiceInteractionSource
        public int getVoiceSearchFailureEventSource() {
            return mFailureSource;
        }

        @VoiceInteractionSource
        public int getVoiceSearchUnexpectedResultSource() {
            return mUnexpectedResultSource;
        }

        public Boolean getVoiceSearchResult() {
            return mResult;
        }

        public Float getVoiceConfidenceValue() {
            return mVoiceConfidenceValue;
        }
    }

    /**
     * Test implementation of {@link ToolbarDataProvider}.
     */
    private class TestDataProvider implements ToolbarDataProvider {
        private boolean mIncognito;

        public void setIncognito(boolean incognito) {
            mIncognito = incognito;
        }

        @Override
        public Tab getTab() {
            return mTab;
        }

        @Override
        public boolean hasTab() {
            return false;
        }

        @Override
        public String getCurrentUrl() {
            return null;
        }

        @Override
        public NewTabPage getNewTabPageForCurrentTab() {
            return null;
        }

        @Override
        public boolean isIncognito() {
            return mIncognito;
        }

        @Override
        public boolean shouldShowLocationBarInOverviewMode() {
            return false;
        }

        @Override
        public boolean isInOverviewAndShowingOmnibox() {
            return false;
        }

        @Override
        public Profile getProfile() {
            return null;
        }

        @Override
        public UrlBarData getUrlBarData() {
            return UrlBarData.EMPTY;
        }

        @Override
        public String getTitle() {
            return null;
        }

        @Override
        public int getPrimaryColor() {
            return 0;
        }

        @Override
        public boolean isUsingBrandColor() {
            return false;
        }

        @Override
        public boolean isOfflinePage() {
            return false;
        }

        @Override
        public boolean isPreview() {
            return false;
        }

        @Override
        public int getSecurityLevel() {
            return 0;
        }

        @Override
        public int getSecurityIconResource(boolean isTablet) {
            return 0;
        }

        @Override
        public @ColorRes int getSecurityIconColorStateList() {
            return 0;
        }
    }

    /**
     * TODO(crbug.com/962527): Remove this dependency on {@link AutocompleteCoordinatorImpl}.
     */
    private class TestAutocompleteCoordinatorImpl extends AutocompleteCoordinatorImpl {
        public TestAutocompleteCoordinatorImpl(ViewGroup parent, AutocompleteDelegate delegate,
                OmniboxSuggestionsDropdown.Embedder dropdownEmbedder,
                UrlBarEditingTextStateProvider urlBarEditingTextProvider) {
            super(parent, delegate, dropdownEmbedder, urlBarEditingTextProvider);
        }

        @Override
        public void onVoiceResults(List<VoiceResult> results) {
            mAutocompleteVoiceResults = results;
        }
    }

    /**
     * Test implementation of {@link VoiceRecognitionHandler.Delegate}.
     */
    private class TestDelegate implements VoiceRecognitionHandler.Delegate {
        private String mUrl;
        private boolean mUpdatedMicButtonState;
        private AutocompleteCoordinator mAutocompleteCoordinator;

        TestDelegate() {
            ViewGroup parent =
                    (ViewGroup) mActivityTestRule.getActivity().findViewById(android.R.id.content);
            Assert.assertNotNull(parent);
            mAutocompleteCoordinator =
                    new TestAutocompleteCoordinatorImpl(parent, null, null, null);
        }

        @Override
        public void loadUrlFromVoice(String url) {
            mUrl = url;
        }

        @Override
        public void updateMicButtonState() {
            mUpdatedMicButtonState = true;
        }

        @Override
        public void setSearchQuery(final String query) {}

        @Override
        public ToolbarDataProvider getToolbarDataProvider() {
            return mDataProvider;
        }

        @Override
        public AutocompleteCoordinator getAutocompleteCoordinator() {
            return mAutocompleteCoordinator;
        }

        @Override
        public WindowAndroid getWindowAndroid() {
            return mWindowAndroid;
        }

        public boolean updatedMicButtonState() {
            return mUpdatedMicButtonState;
        }

        public String getUrl() {
            return mUrl;
        }
    }

    /**
     * Test implementation of {@link ActivityWindowAndroid}.
     */
    private class TestWindowAndroid extends ActivityWindowAndroid {
        private boolean mCancelableIntentSuccess = true;
        private int mResultCode = Activity.RESULT_OK;
        private Intent mResults = new Intent();
        private Activity mActivity;
        private boolean mWasCancelableIntentShown;
        private Intent mCancelableIntent;
        private IntentCallback mCallback;

        public TestWindowAndroid(Context context) {
            super(context);
        }

        public void setCancelableIntentSuccess(boolean success) {
            mCancelableIntentSuccess = success;
        }

        public void setResultCode(int resultCode) {
            mResultCode = resultCode;
        }

        public void setVoiceResults(Bundle results) {
            mResults.putExtras(results);
        }

        public void setActivity(Activity activity) {
            mActivity = activity;
        }

        public boolean wasCancelableIntentShown() {
            return mWasCancelableIntentShown;
        }

        public Intent getCancelableIntent() {
            return mCancelableIntent;
        }

        public IntentCallback getIntentCallback() {
            return mCallback;
        }

        @Override
        public int showCancelableIntent(Intent intent, IntentCallback callback, Integer errorId) {
            mWasCancelableIntentShown = true;
            mCancelableIntent = intent;
            mCallback = callback;
            if (mCancelableIntentSuccess) {
                callback.onIntentCompleted(mWindowAndroid, mResultCode, mResults);
                return 0;
            }
            return WindowAndroid.START_INTENT_FAILURE;
        }

        @Override
        public WeakReference<Activity> getActivity() {
            if (mActivity == null) return super.getActivity();
            return new WeakReference<>(mActivity);
        }
    }

    /**
     * Test implementation of {@link AndroidPermissionDelegate}.
     */
    private class TestAndroidPermissionDelegate implements AndroidPermissionDelegate {
        private boolean mHasPermission;
        private boolean mCanRequestPermission;

        private boolean mCalledHasPermission;
        private boolean mCalledCanRequestPermission;
        private int mPermissionResult = PackageManager.PERMISSION_GRANTED;

        public void setPermissionResults(int result) {
            mPermissionResult = result;
        }

        public void setHasPermission(boolean hasPermission) {
            mHasPermission = hasPermission;
        }

        public void setCanRequestPermission(boolean canRequestPermission) {
            mCanRequestPermission = canRequestPermission;
        }

        public boolean calledHasPermission() {
            return mCalledHasPermission;
        }

        public boolean calledCanRequestPermission() {
            return mCalledCanRequestPermission;
        }

        @Override
        public boolean hasPermission(String permission) {
            mCalledHasPermission = true;
            return mHasPermission;
        }

        @Override
        public boolean canRequestPermission(String permission) {
            mCalledCanRequestPermission = true;
            return mCanRequestPermission;
        }

        @Override
        public boolean isPermissionRevokedByPolicy(String permission) {
            return false;
        }

        @Override
        public void requestPermissions(String[] permissions, PermissionCallback callback) {
            int[] results = new int[permissions.length];
            for (int i = 0; i < permissions.length; i++) {
                results[i] = mPermissionResult;
            }
            callback.onRequestPermissionsResult(permissions, results);
        }

        @Override
        public boolean handlePermissionResult(
                int requestCode, String[] permissions, int[] grantResults) {
            return false;
        }
    }

    @Before
    public void setUp() throws InterruptedException, ExecutionException {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();

        mDataProvider = new TestDataProvider();
        mDelegate = TestThreadUtils.runOnUiThreadBlocking(() -> new TestDelegate());
        mHandler = new TestVoiceRecognitionHandler(mDelegate);
        mPermissionDelegate = new TestAndroidPermissionDelegate();
        mAutocomplete = new TestAutocompleteController(null /* view */, sEmptySuggestionListener,
                new HashMap<String, List<SuggestionsResult>>());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mWindowAndroid = new TestWindowAndroid(mActivityTestRule.getActivity());
            mWindowAndroid.setAndroidPermissionDelegate(mPermissionDelegate);
            mTab = new MockTab(0, false);
        });

        doReturn(false).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        doReturn(mIntent).when(mAssistantVoiceSearchService).getAssistantVoiceSearchIntent();
        mHandler.setAssistantVoiceSearchService(mAssistantVoiceSearchService);
    }

    @After
    public void tearDown() {
        SysUtils.resetForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> { mWindowAndroid.destroy(); });
    }

    /**
     * Tests for {@link VoiceRecognitionHandler#isVoiceSearchEnabled}.
     */
    @Test
    @SmallTest
    public void testIsVoiceSearchEnabled_FalseOnNullTab() {
        Assert.assertFalse(mHandler.isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    public void testIsVoiceSearchEnabled_FalseOnNullDataProvider() {
        mDataProvider = null;
        Assert.assertFalse(mHandler.isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    public void testIsVoiceSearchEnabled_FalseWhenIncognito() {
        mDataProvider.setIncognito(true);
        Assert.assertFalse(mHandler.isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    public void testIsVoiceSearchEnabled_FalseWhenNoPermissionAndCantRequestPermission() {
        Assert.assertFalse(mHandler.isVoiceSearchEnabled());
        Assert.assertTrue(mPermissionDelegate.calledHasPermission());
        Assert.assertTrue(mPermissionDelegate.calledCanRequestPermission());
    }

    @Test
    @SmallTest
    public void testIsVoiceSearchEnabled_Success() {
        mPermissionDelegate.setCanRequestPermission(true);
        mPermissionDelegate.setHasPermission(true);
        Assert.assertTrue(mHandler.isVoiceSearchEnabled());
    }

    /**
     * Tests for {@link VoiceRecognitionHandler#startVoiceRecognition}.
     */
    @Test
    @SmallTest
    public void testStartVoiceRecognition_OnlyUpdateMicButtonStateIfCantRequestPermission() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX); });
        Assert.assertEquals(-1, mHandler.getVoiceSearchStartEventSource());
        Assert.assertTrue(mDelegate.updatedMicButtonState());
        verify(mAssistantVoiceSearchService).reportUserEligibility();
    }

    @Test
    @SmallTest
    public void testStartVoiceRecognition_OnlyUpdateMicButtonStateIfPermissionsNotGranted() {
        mPermissionDelegate.setCanRequestPermission(true);
        mPermissionDelegate.setPermissionResults(PackageManager.PERMISSION_DENIED);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX); });
        Assert.assertEquals(-1, mHandler.getVoiceSearchStartEventSource());
        Assert.assertTrue(mDelegate.updatedMicButtonState());
        verify(mAssistantVoiceSearchService).reportUserEligibility();
    }

    @Test
    @SmallTest
    @Feature("OmniboxAssistantVoiceSearch")
    @EnableFeatures("OmniboxAssistantVoiceSearch")
    public void testStartVoiceRecognition_StartsAssistantVoiceSearch() {
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        startVoiceRecognition(VoiceInteractionSource.OMNIBOX);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        Assert.assertEquals(mIntent, mWindowAndroid.getCancelableIntent());
        verify(mAssistantVoiceSearchService).reportUserEligibility();
    }

    @Test
    @SmallTest
    public void testStartVoiceRecognition_StartsVoiceSearchWithFailedIntent() {
        mWindowAndroid.setCancelableIntentSuccess(false);
        startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
        Assert.assertEquals(
                VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchStartEventSource());
        Assert.assertTrue(mDelegate.updatedMicButtonState());
        Assert.assertEquals(
                VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchFailureEventSource());
    }

    @Test
    @SmallTest
    public void testStartVoiceRecognition_StartsVoiceSearchWithSuccessfulIntent() {
        startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
        Assert.assertEquals(
                VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchStartEventSource());
        Assert.assertFalse(mDelegate.updatedMicButtonState());
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
        startVoiceRecognition(VoiceInteractionSource.NTP);
        Assert.assertEquals(VoiceInteractionSource.NTP, mHandler.getVoiceSearchStartEventSource());
        Assert.assertEquals(null, mHandler.getVoiceSearchResult());
        Assert.assertEquals(
                VoiceInteractionSource.NTP, mHandler.getVoiceSearchFailureEventSource());
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android"));
    }

    @Test
    @SmallTest
    public void testCallback_noVoiceSearchResultCanceled() {
        mWindowAndroid.setResultCode(Activity.RESULT_CANCELED);
        startVoiceRecognition(VoiceInteractionSource.NTP);
        Assert.assertEquals(VoiceInteractionSource.NTP, mHandler.getVoiceSearchStartEventSource());
        Assert.assertEquals(null, mHandler.getVoiceSearchResult());
        Assert.assertEquals(
                VoiceInteractionSource.NTP, mHandler.getVoiceSearchDismissedEventSource());
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android"));
    }

    @Test
    @SmallTest
    public void testCallback_noVoiceSearchResultWithNullAutocompleteResult() {
        mWindowAndroid.setVoiceResults(new Bundle());
        startVoiceRecognition(VoiceInteractionSource.SEARCH_WIDGET);
        Assert.assertEquals(
                VoiceInteractionSource.SEARCH_WIDGET, mHandler.getVoiceSearchStartEventSource());
        Assert.assertEquals(false, mHandler.getVoiceSearchResult());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android"));
    }

    @Test
    @SmallTest
    public void testCallback_noVoiceSearchResultWithNoMatch() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mWindowAndroid.setVoiceResults(createDummyBundle("", 1f));
            startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
            Assert.assertEquals(
                    VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchStartEventSource());
            Assert.assertEquals(false, mHandler.getVoiceSearchResult());
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            "VoiceInteraction.QueryDuration.Android"));
        });
    }

    @Test
    @SmallTest
    public void testCallback_successWithLowConfidence() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            float confidence =
                    VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD - 0.01f;
            mWindowAndroid.setVoiceResults(createDummyBundle("testing", confidence));
            startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
            Assert.assertEquals(
                    VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchStartEventSource());
            Assert.assertEquals(
                    VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchFinishEventSource());
            Assert.assertEquals(true, mHandler.getVoiceSearchResult());
            Assert.assertTrue(confidence == mHandler.getVoiceConfidenceValue());
            assertVoiceResultsAreEqual(
                    mAutocompleteVoiceResults, new String[] {"testing"}, new float[] {confidence});
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            "VoiceInteraction.QueryDuration.Android"));
        });
    }

    @Test
    @SmallTest
    public void testCallback_successWithHighConfidence() {
        // Needs to run on the UI thread because we use the TemplateUrlService on success.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mWindowAndroid.setVoiceResults(createDummyBundle(
                    "testing", VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD));
            startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
            Assert.assertEquals(
                    VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchStartEventSource());
            Assert.assertEquals(
                    VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchFinishEventSource());
            Assert.assertEquals(true, mHandler.getVoiceSearchResult());
            Assert.assertTrue(VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD
                    == mHandler.getVoiceConfidenceValue());
            assertVoiceResultsAreEqual(mAutocompleteVoiceResults, new String[] {"testing"},
                    new float[] {
                            VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD});
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            "VoiceInteraction.QueryDuration.Android"));
        });
    }

    @Test
    @SmallTest
    public void testCallback_successWithLangues() {
        // Needs to run on the UI thread because we use the TemplateUrlService on success.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mWindowAndroid.setVoiceResults(createDummyBundle("testing",
                    VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD, "en-us"));
            startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
            Assert.assertEquals(
                    VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchStartEventSource());
            Assert.assertEquals(
                    VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchFinishEventSource());
            Assert.assertEquals(true, mHandler.getVoiceSearchResult());
            Assert.assertTrue(VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD
                    == mHandler.getVoiceConfidenceValue());
            assertVoiceResultsAreEqual(mAutocompleteVoiceResults, new String[] {"testing"},
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
        Assert.assertNull(mHandler.convertBundleToVoiceResults(
                createDummyBundle(new String[] {"blah"}, new float[] {0f, 1f})));
        Assert.assertNull(mHandler.convertBundleToVoiceResults(
                createDummyBundle(new String[] {"blah", "foo"}, new float[] {7f})));
        Assert.assertNull(mHandler.convertBundleToVoiceResults(createDummyBundle(
                new String[] {"blah", "foo"}, new float[] {7f, 1f}, new String[] {"foo"})));
    }

    @Test
    @SmallTest
    public void testParseResults_ValidBundle() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            String[] texts = new String[] {"a", "b", "c"};
            float[] confidences = new float[] {0.8f, 1.0f, 1.0f};

            List<VoiceResult> results =
                    mHandler.convertBundleToVoiceResults(createDummyBundle(texts, confidences));

            assertVoiceResultsAreEqual(results, texts, confidences);
        });
    }

    @Test
    @SmallTest
    public void testParseResults_VoiceResponseURLConversion() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            String[] texts =
                    new String[] {"a", "www. b .co .uk", "engadget .com", "www.google.com"};
            float[] confidences = new float[] {1.0f, 1.0f, 1.0f, 1.0f};
            List<VoiceResult> results =
                    mHandler.convertBundleToVoiceResults(createDummyBundle(texts, confidences));

            assertVoiceResultsAreEqual(results,
                    new String[] {"a", "www.b.co.uk", "engadget.com", "www.google.com"},
                    new float[] {1.0f, 1.0f, 1.0f, 1.0f});
        });
    }

    @Test
    @SmallTest
    public void testStopTrackingAndRecordQueryDuration() {
        mHandler.setQueryStartTimeForTesting(100L);
        mHandler.stopTrackingAndRecordQueryDuration();
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android"));
    }

    @Test
    @SmallTest
    public void testStopTrackingAndRecordQueryDuration_calledWithNull() {
        mHandler.setQueryStartTimeForTesting(null);
        mHandler.stopTrackingAndRecordQueryDuration();
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android"));
    }

    @Test
    @SmallTest
    public void testCallback_CalledTwice() {
        startVoiceRecognition(VoiceInteractionSource.NTP);
        Assert.assertEquals(-1, mHandler.getVoiceSearchUnexpectedResultSource());

        IntentCallback callback = mWindowAndroid.getIntentCallback();
        callback.onIntentCompleted(mWindowAndroid, Activity.RESULT_CANCELED, null);
        Assert.assertEquals(
                VoiceInteractionSource.NTP, mHandler.getVoiceSearchUnexpectedResultSource());
    }

    /**
     * Kicks off voice recognition with the given source, for testing
     * {@linkVoiceRecognitionHandler.VoiceRecognitionCompleteCallback}.
     *
     * @param source The source of the voice recognition initiation.
     */
    private void startVoiceRecognition(@VoiceInteractionSource int source) {
        mPermissionDelegate.setHasPermission(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mHandler.startVoiceRecognition(source); });
    }

    private static Bundle createDummyBundle(String text, float confidence) {
        return createDummyBundle(new String[] {text}, new float[] {confidence}, null);
    }

    private static Bundle createDummyBundle(
            String text, float confidence, @Nullable String language) {
        return createDummyBundle(new String[] {text}, new float[] {confidence},
                language == null ? null : new String[] {language});
    }

    private static Bundle createDummyBundle(String[] texts, float[] confidences) {
        return createDummyBundle(texts, confidences, null);
    }

    private static Bundle createDummyBundle(
            String[] texts, float[] confidences, @Nullable String[] languages) {
        Bundle b = new Bundle();

        b.putStringArrayList(
                RecognizerIntent.EXTRA_RESULTS, new ArrayList<String>(Arrays.asList(texts)));
        b.putFloatArray(RecognizerIntent.EXTRA_CONFIDENCE_SCORES, confidences);
        if (languages != null) {
            b.putStringArrayList(VoiceRecognitionHandler.VOICE_QUERY_RESULT_LANGUAGES,
                    new ArrayList<String>(Arrays.asList(languages)));
        }

        return b;
    }

    private static void assertVoiceResultsAreEqual(
            List<VoiceResult> results, String[] texts, float[] confidences) {
        assertVoiceResultsAreEqual(results, texts, confidences, null);
    }

    private static void assertVoiceResultsAreEqual(
            List<VoiceResult> results, String[] texts, float[] confidences, String[] languages) {
        Assert.assertTrue("Invalid array sizes",
                results.size() == texts.length && texts.length == confidences.length);
        if (languages != null) {
            Assert.assertTrue("Invalid array sizes", confidences.length == languages.length);
        }

        for (int i = 0; i < texts.length; ++i) {
            VoiceResult result = results.get(i);
            Assert.assertEquals("Match text is not equal", texts[i], result.getMatch());
            Assert.assertEquals(
                    "Confidence is not equal", confidences[i], result.getConfidence(), 0);
            if (languages != null) {
                Assert.assertEquals("Languages not equal", result.getLanguage(), languages[i]);
            }
        }
    }
}
