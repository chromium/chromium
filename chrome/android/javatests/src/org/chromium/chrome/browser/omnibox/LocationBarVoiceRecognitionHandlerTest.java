// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.ColorStateList;
import android.os.Bundle;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.LocationBarVoiceRecognitionHandler.VoiceInteractionSource;
import org.chromium.chrome.browser.omnibox.VoiceSuggestionProvider.VoiceResult;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.OmniboxTestUtils.SuggestionsResult;
import org.chromium.chrome.test.util.OmniboxTestUtils.TestAutocompleteController;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.base.PermissionCallback;
import org.chromium.ui.base.WindowAndroid;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ExecutionException;

/**
 * Tests for {@link LocationBarVoiceRecognitionHandler}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LocationBarVoiceRecognitionHandlerTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private TestDataProvider mDataProvider;
    private TestDelegate mDelegate;
    private TestLocationBarVoiceRecognitionHandler mHandler;
    private LocalTestAutocompleteController mAutocomplete;
    private TestAndroidPermissionDelegate mPermissionDelegate;
    private TestWindowAndroid mWindowAndroid;
    private Tab mTab;

    private static final OnSuggestionsReceivedListener sEmptySuggestionListener =
            new OnSuggestionsReceivedListener() {
                @Override
                public void onSuggestionsReceived(
                        List<OmniboxSuggestion> suggestions, String inlineAutocompleteText) {}
            };

    /**
     * An implementation of the real {@link LocationBarVoiceRecognitionHandler} except instead of
     * recording histograms we just flag whether we would have or not.
     */
    private class TestLocationBarVoiceRecognitionHandler
            extends LocationBarVoiceRecognitionHandler {
        @VoiceInteractionSource
        private int mStartSource = -1;
        @VoiceInteractionSource
        private int mFinishSource = -1;
        @VoiceInteractionSource
        private int mDismissedSource = -1;
        @VoiceInteractionSource
        private int mFailureSource = -1;
        private Boolean mResult;
        private Float mVoiceConfidenceValue;

        public TestLocationBarVoiceRecognitionHandler(Delegate delegate) {
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
        protected void recordVoiceSearchResult(boolean result) {
            mResult = result;
        }

        @Override
        protected void recordVoiceSearchConfidenceValue(float value) {
            mVoiceConfidenceValue = value;
        }

        @Override
        protected boolean isRecognitionIntentPresent(Context context, boolean useCachedValue) {
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
        public boolean shouldShowVerboseStatus() {
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
        public ColorStateList getSecurityIconColorStateList() {
            return null;
        }

        @Override
        public boolean shouldDisplaySearchTerms() {
            return false;
        }
    }

    /**
     * Test implementation of {@link LocationBarVoiceRecognitionHandler.Delegate}.
     */
    private class TestDelegate implements LocationBarVoiceRecognitionHandler.Delegate {
        private boolean mUpdatedMicButtonState;
        private AutocompleteCoordinator mCoordinator;

        TestDelegate() {
            ViewGroup parent =
                    (ViewGroup) mActivityTestRule.getActivity().findViewById(android.R.id.content);
            Assert.assertNotNull(parent);
            mCoordinator = new AutocompleteCoordinator(parent, null, null, null) {
                @Override
                public VoiceResult onVoiceResults(Bundle data) {
                    return mAutocomplete.onVoiceResults(data);
                }
            };
        }

        @Override
        public void loadUrlFromVoice(String url) {}

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
            return mCoordinator;
        }

        @Override
        public WindowAndroid getWindowAndroid() {
            return mWindowAndroid;
        }

        public boolean updatedMicButtonState() {
            return mUpdatedMicButtonState;
        }
    }

    /**
     * Test implementation of {@link ActivityWindowAndroid}.
     */
    private class TestWindowAndroid extends ActivityWindowAndroid {
        private boolean mCancelableIntentSuccess = true;
        private int mResultCode = Activity.RESULT_OK;

        public TestWindowAndroid(Context context) {
            super(context);
        }

        public void setCancelableIntentSuccess(boolean success) {
            mCancelableIntentSuccess = success;
        }

        public void setResultCode(int resultCode) {
            mResultCode = resultCode;
        }

        @Override
        public int showCancelableIntent(Intent intent, IntentCallback callback, Integer errorId) {
            if (mCancelableIntentSuccess) {
                callback.onIntentCompleted(mWindowAndroid, mResultCode, intent);
                return 0;
            }
            return WindowAndroid.START_INTENT_FAILURE;
        }
    }

    /**
     * Test implementation of the {@link AutocompleteController}.
     */
    private class LocalTestAutocompleteController extends TestAutocompleteController {
        private VoiceResult mVoiceResult;

        public LocalTestAutocompleteController(View view, OnSuggestionsReceivedListener listener,
                Map<String, List<SuggestionsResult>> suggestions) {
            super(view, listener, suggestions);
        }

        public void setVoiceResult(VoiceResult voiceResult) {
            mVoiceResult = voiceResult;
        }

        @Override
        public VoiceResult onVoiceResults(Bundle data) {
            return mVoiceResult;
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
        mActivityTestRule.startMainActivityOnBlankPage();

        mDataProvider = new TestDataProvider();
        mDelegate = ThreadUtils.runOnUiThreadBlocking(() -> new TestDelegate());
        mHandler = new TestLocationBarVoiceRecognitionHandler(mDelegate);
        mPermissionDelegate = new TestAndroidPermissionDelegate();
        mAutocomplete = new LocalTestAutocompleteController(null /* view */,
                sEmptySuggestionListener, new HashMap<String, List<SuggestionsResult>>());

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mWindowAndroid = new TestWindowAndroid(mActivityTestRule.getActivity());
            mWindowAndroid.setAndroidPermissionDelegate(mPermissionDelegate);
            mTab = new Tab(0, false /* incognito */, mWindowAndroid);
        });
    }

    /**
     * Tests for {@link LocationBarVoiceRecognitionHandler#isVoiceSearchEnabled}.
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
     * Tests for {@link LocationBarVoiceRecognitionHandler#startVoiceRecognition}.
     */
    @Test
    @SmallTest
    public void testStartVoiceRecognition_OnlyUpdateMicButtonStateIfCantRequestPermission() {
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
        Assert.assertEquals(-1, mHandler.getVoiceSearchStartEventSource());
        Assert.assertTrue(mDelegate.updatedMicButtonState());
    }

    @Test
    @SmallTest
    public void testStartVoiceRecognition_OnlyUpdateMicButtonStateIfPermissionsNotGranted() {
        mPermissionDelegate.setCanRequestPermission(true);
        mPermissionDelegate.setPermissionResults(PackageManager.PERMISSION_DENIED);
        mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
        Assert.assertEquals(-1, mHandler.getVoiceSearchStartEventSource());
        Assert.assertTrue(mDelegate.updatedMicButtonState());
    }

    /**
     * Kicks off voice recognition with the given source, for testing
     * {@link LocationBarVoiceRecognitionHandler.VoiceRecognitionCompleteCallback}.
     *
     * @param source The source of the voice recognition initiation.
     */
    private void startVoiceRecognition(@VoiceInteractionSource int source) {
        mPermissionDelegate.setHasPermission(true);
        mHandler.startVoiceRecognition(source);
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
     * Tests for the {@link LocationBarVoiceRecognitionHandler.VoiceRecognitionCompleteCallback}.
     *
     * These tests are kicked off by
     * {@link LocationBarVoiceRecognitionHandler#startVoiceRecognition} to test the flow as it would
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
    }

    @Test
    @SmallTest
    public void testCallback_noVoiceSearchResultWithNullAutocompleteResult() {
        startVoiceRecognition(VoiceInteractionSource.SEARCH_WIDGET);
        Assert.assertEquals(
                VoiceInteractionSource.SEARCH_WIDGET, mHandler.getVoiceSearchStartEventSource());
        Assert.assertEquals(false, mHandler.getVoiceSearchResult());
    }

    @Test
    @SmallTest
    public void testCallback_noVoiceSearchResultWithNoMatch() {
        mAutocomplete.setVoiceResult(new VoiceResult("", 1.0f));
        startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
        Assert.assertEquals(
                VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchStartEventSource());
        Assert.assertEquals(false, mHandler.getVoiceSearchResult());
    }

    @Test
    @SmallTest
    public void testCallback_successWithLowConfidence() {
        mAutocomplete.setVoiceResult(new VoiceResult("testing",
                LocationBarVoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD
                        - 0.01f));
        startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
        Assert.assertEquals(
                VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchStartEventSource());
        Assert.assertEquals(
                VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchFinishEventSource());
        Assert.assertEquals(true, mHandler.getVoiceSearchResult());
        Assert.assertTrue(
                LocationBarVoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD
                        - 0.01f
                == mHandler.getVoiceConfidenceValue());
    }

    @Test
    @SmallTest
    public void testCallback_successWithHighConfidence() {
        // Needs to run on the UI thread because we use the TemplateUrlService on success.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mAutocomplete.setVoiceResult(new VoiceResult("testing",
                    LocationBarVoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD));
            startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
            Assert.assertEquals(
                    VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchStartEventSource());
            Assert.assertEquals(
                    VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchFinishEventSource());
            Assert.assertEquals(true, mHandler.getVoiceSearchResult());
            Assert.assertTrue(
                    LocationBarVoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD
                    == mHandler.getVoiceConfidenceValue());
        });
    }
}
