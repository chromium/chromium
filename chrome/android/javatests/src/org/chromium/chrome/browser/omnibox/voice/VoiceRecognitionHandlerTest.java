// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
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
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownEmbedder;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.AssistantActionPerformed;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.AudioPermissionState;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.TranslateBridgeWrapper;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceIntentTarget;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceInteractionSource;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceResult;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.base.PermissionCallback;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.IntentCallback;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
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
    @Mock
    ModalDialogManager mModalDialogManager;
    @Mock
    TranslateBridgeWrapper mTranslateBridgeWrapper;
    @Mock
    Tab mTab;
    @Mock
    VoiceRecognitionHandler.Observer mObserver;

    private TestDataProvider mDataProvider;
    private TestDelegate mDelegate;
    private TestVoiceRecognitionHandler mHandler;
    private TestAndroidPermissionDelegate mPermissionDelegate;
    private TestWindowAndroid mWindowAndroid;
    private ActivityLifecycleDispatcher mLifecycleDispatcher;
    private List<VoiceResult> mAutocompleteVoiceResults;
    private ObservableSupplierImpl<Profile> mProfileSupplier;

    private static final OnSuggestionsReceivedListener sEmptySuggestionListener =
            new OnSuggestionsReceivedListener() {
                @Override
                public void onSuggestionsReceived(
                        AutocompleteResult autocompleteResult, String inlineAutocompleteText) {}
            };

    // The default Tab URL.
    private static final String DEFAULT_URL = "https://example.com/";
    private static final String DEFAULT_USER_EMAIL = "test@test.com";

    /**
     * An implementation of the real {@link VoiceRecognitionHandler} except instead of
     * recording histograms we just flag whether we would have or not.
     */
    private class TestVoiceRecognitionHandler extends VoiceRecognitionHandler {
        @VoiceInteractionSource
        public int mStartSource = -1;
        @VoiceIntentTarget
        public int mStartTarget = -1;

        @VoiceInteractionSource
        public int mFinishSource = -1;
        @VoiceIntentTarget
        public int mFinishTarget = -1;

        @VoiceInteractionSource
        public int mDismissedSource = -1;
        @VoiceIntentTarget
        public int mDismissedTarget = -1;

        @VoiceInteractionSource
        public int mFailureSource = -1;
        @VoiceIntentTarget
        public int mFailureTarget = -1;

        @VoiceInteractionSource
        public int mUnexpectedResultSource = -1;
        @VoiceIntentTarget
        public int mUnexpectedResultTarget = -1;

        @AssistantActionPerformed
        private int mActionPerformed = -1;
        @VoiceInteractionSource
        private int mActionPerformedSource = -1;

        private Boolean mResult;
        @VoiceIntentTarget
        private int mResultTarget;

        private Float mVoiceConfidenceValue;
        @VoiceIntentTarget
        private int mVoiceConfidenceValueTarget;

        public TestVoiceRecognitionHandler(
                Delegate delegate, ObservableSupplierImpl<Profile> profileSupplier) {
            super(delegate, () -> mAssistantVoiceSearchService, () -> {}, profileSupplier);
        }

        @Override
        protected void recordVoiceSearchStartEvent(
                @VoiceInteractionSource int source, @VoiceIntentTarget int target) {
            mStartSource = source;
            mStartTarget = target;
        }

        @Override
        protected void recordVoiceSearchFinishEvent(
                @VoiceInteractionSource int source, @VoiceIntentTarget int target) {
            mFinishSource = source;
            mFinishTarget = target;
        }

        @Override
        protected void recordAssistantActionPerformed(
                @VoiceInteractionSource int source, @AssistantActionPerformed int action) {
            mActionPerformedSource = source;
            mActionPerformed = action;
        }

        @Override
        protected void recordVoiceSearchFailureEvent(
                @VoiceInteractionSource int source, @VoiceIntentTarget int target) {
            mFailureSource = source;
            mFailureTarget = target;
        }

        @Override
        protected void recordVoiceSearchDismissedEvent(
                @VoiceInteractionSource int source, @VoiceIntentTarget int target) {
            mDismissedSource = source;
            mDismissedTarget = target;
        }

        @Override
        protected void recordVoiceSearchUnexpectedResult(
                @VoiceInteractionSource int source, @VoiceIntentTarget int target) {
            mUnexpectedResultSource = source;
            mUnexpectedResultTarget = target;
        }

        @Override
        protected void recordVoiceSearchResult(@VoiceIntentTarget int target, boolean result) {
            mResultTarget = target;
            mResult = result;
        }

        @Override
        protected void recordVoiceSearchConfidenceValue(
                @VoiceIntentTarget int target, float value) {
            mVoiceConfidenceValueTarget = target;
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
        @VoiceIntentTarget
        public int getVoiceSearchStartEventTarget() {
            return mStartTarget;
        }

        @VoiceInteractionSource
        public int getVoiceSearchFinishEventSource() {
            return mFinishSource;
        }
        @VoiceIntentTarget
        public int getVoiceSearchFinishEventTarget() {
            return mFinishTarget;
        }

        @VoiceInteractionSource
        public int getVoiceSearchDismissedEventSource() {
            return mDismissedSource;
        }
        @VoiceIntentTarget
        public int getVoiceSearchDismissedEventTarget() {
            return mDismissedTarget;
        }

        @VoiceInteractionSource
        public int getVoiceSearchFailureEventSource() {
            return mFailureSource;
        }
        @VoiceIntentTarget
        public int getVoiceSearchFailureEventTarget() {
            return mFailureTarget;
        }

        @VoiceInteractionSource
        public int getVoiceSearchUnexpectedResultSource() {
            return mUnexpectedResultSource;
        }
        @VoiceIntentTarget
        public int getVoiceSearchUnexpectedResultTarget() {
            return mUnexpectedResultTarget;
        }

        @AssistantActionPerformed
        public int getAssistantActionPerformed() {
            return mActionPerformed;
        }

        @VoiceInteractionSource
        public int getAssistantActionPerformedSource() {
            return mActionPerformedSource;
        }

        public Boolean getVoiceSearchResult() {
            return mResult;
        }
        @VoiceIntentTarget
        public int getVoiceSearchResultTarget() {
            return mResultTarget;
        }

        public Float getVoiceConfidenceValue() {
            return mVoiceConfidenceValue;
        }
        @VoiceIntentTarget
        public int getVoiceConfidenceValueTarget() {
            return mVoiceConfidenceValueTarget;
        }
    }

    /**
     * Test implementation of {@link ToolbarDataProvider}.
     */
    private class TestDataProvider implements LocationBarDataProvider {
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
        public void addObserver(Observer observer) {}

        @Override
        public void removeObserver(Observer observer) {}

        @Override
        public String getCurrentUrl() {
            return null;
        }

        @Override
        public NewTabPageDelegate getNewTabPageDelegate() {
            return NewTabPageDelegate.EMPTY;
        }

        @Override
        public boolean isIncognito() {
            return mIncognito;
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
        public int getSecurityLevel() {
            return 0;
        }

        @Override
        public int getPageClassification(boolean isFocusedFromFakebox) {
            return PageClassification.NTP_VALUE;
        }

        @Override
        public int getSecurityIconResource(boolean isTablet) {
            return 0;
        }

        @Override
        public @ColorRes int getSecurityIconColorStateList() {
            return 0;
        }

        @Override
        public int getSecurityIconContentDescriptionResourceId() {
            return 0;
        }
    }

    /**
     * TODO(crbug.com/962527): Remove this dependency on {@link AutocompleteCoordinator}.
     */
    private class TestAutocompleteCoordinator extends AutocompleteCoordinator {
        public TestAutocompleteCoordinator(ViewGroup parent, AutocompleteDelegate delegate,
                OmniboxSuggestionsDropdownEmbedder dropdownEmbedder,
                UrlBarEditingTextStateProvider urlBarEditingTextProvider) {
            super(parent, delegate, dropdownEmbedder, urlBarEditingTextProvider,
                    mLifecycleDispatcher, () -> mModalDialogManager, null, null, mDataProvider);
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
        private AutocompleteCoordinator mAutocompleteCoordinator;

        TestDelegate() {
            ViewGroup parent =
                    (ViewGroup) mActivityTestRule.getActivity().findViewById(android.R.id.content);
            Assert.assertNotNull(parent);
            mAutocompleteCoordinator = new TestAutocompleteCoordinator(parent, null, null, null);
        }

        @Override
        public void loadUrlFromVoice(String url) {
            mUrl = url;
        }

        @Override
        public void setSearchQuery(final String query) {}

        @Override
        public LocationBarDataProvider getLocationBarDataProvider() {
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

        @Override
        public void clearOmniboxFocus() {}

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
        mLifecycleDispatcher = mActivityTestRule.getActivity().getLifecycleDispatcher();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mWindowAndroid = new TestWindowAndroid(mActivityTestRule.getActivity());
            mProfileSupplier = new ObservableSupplierImpl<>();
        });

        mDataProvider = new TestDataProvider();
        mDelegate = TestThreadUtils.runOnUiThreadBlocking(() -> new TestDelegate());
        mHandler = new TestVoiceRecognitionHandler(mDelegate, mProfileSupplier);
        mHandler.addObserver(mObserver);
        mPermissionDelegate = new TestAndroidPermissionDelegate();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mWindowAndroid.setAndroidPermissionDelegate(mPermissionDelegate);
        });

        doReturn(new GURL(DEFAULT_URL)).when(mTab).getUrl();

        doReturn(false).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        doReturn(false).when(mAssistantVoiceSearchService).needsEnabledCheck();
        doReturn(mIntent).when(mAssistantVoiceSearchService).getAssistantVoiceSearchIntent();
        doReturn(DEFAULT_USER_EMAIL).when(mAssistantVoiceSearchService).getUserEmail();

        doReturn(true).when(mTranslateBridgeWrapper).canManuallyTranslate(notNull());
        doReturn("fr").when(mTranslateBridgeWrapper).getOriginalLanguage(notNull());
        doReturn("de").when(mTranslateBridgeWrapper).getCurrentLanguage(notNull());
        doReturn("ja").when(mTranslateBridgeWrapper).getTargetLanguage();
        mHandler.setTranslateBridgeWrapper(mTranslateBridgeWrapper);
    }

    @After
    public void tearDown() {
        mHandler.removeObserver(mObserver);
        SysUtils.resetForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> { mWindowAndroid.destroy(); });
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
    @EnableFeatures({ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY})
    public void testIsVoiceSearchEnabled_AllowedByPolicy() {
        setAudioCapturePref(true);
        mPermissionDelegate.setCanRequestPermission(true);
        mPermissionDelegate.setHasPermission(true);
        Assert.assertTrue(isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    @EnableFeatures({ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY})
    public void testIsVoiceSearchEnabled_DisabledByPolicy() {
        setAudioCapturePref(false);
        mPermissionDelegate.setCanRequestPermission(true);
        mPermissionDelegate.setHasPermission(true);
        Assert.assertFalse(isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    @EnableFeatures({ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY})
    public void testIsVoiceSearchEnabled_AudioCapturePolicyAllowsByDefault() {
        mPermissionDelegate.setCanRequestPermission(true);
        mPermissionDelegate.setHasPermission(true);
        Assert.assertTrue(isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    @DisableFeatures({ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY})
    public void testIsVoiceSearchEnabled_SkipPolicyCheckWhenDisabled() {
        setAudioCapturePref(false);
        mPermissionDelegate.setCanRequestPermission(true);
        mPermissionDelegate.setHasPermission(true);
        Assert.assertTrue(isVoiceSearchEnabled());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    @EnableFeatures({ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY})
    public void testIsVoiceSearchEnabled_UpdateAfterProfileSet() {
        setAudioCapturePref(true);
        mPermissionDelegate.setCanRequestPermission(true);
        mPermissionDelegate.setHasPermission(true);
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
        Assert.assertTrue(isVoiceSearchEnabled());

        setAudioCapturePref(false);
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
    @Feature({"OmniboxAssistantVoiceSearch"})
    @EnableFeatures({ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH})
    public void testStartVoiceRecognition_StartsAssistantVoiceSearch() {
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        startVoiceRecognition(VoiceInteractionSource.OMNIBOX);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        Assert.assertEquals(mIntent, mWindowAndroid.getCancelableIntent());
        Assert.assertEquals(VoiceIntentTarget.ASSISTANT, mHandler.getVoiceSearchStartEventTarget());
        verify(mAssistantVoiceSearchService).reportMicPressUserEligibility();
        verify(mIntent).putExtra(
                eq(VoiceRecognitionHandler.EXTRA_INTENT_SENT_TIMESTAMP), anyLong());
        verify(mIntent).putExtra(
                VoiceRecognitionHandler.EXTRA_VOICE_ENTRYPOINT, VoiceInteractionSource.OMNIBOX);
        verify(mIntent).putExtra(
                VoiceRecognitionHandler.EXTRA_INTENT_USER_EMAIL, DEFAULT_USER_EMAIL);
    }

    @Test
    @SmallTest
    @Feature({"OmniboxAssistantVoiceSearch"})
    @EnableFeatures({ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH})
    public void testStartVoiceRecognition_ShouldRequestConditionsFail() {
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(false).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        startVoiceRecognition(VoiceInteractionSource.OMNIBOX);

        verify(mAssistantVoiceSearchService).reportMicPressUserEligibility();
        // We check for the consent dialog when canRequestAssistantVoiceSearch() is true.
        verify(mAssistantVoiceSearchService).needsEnabledCheck();
        verify(mAssistantVoiceSearchService, times(0)).getAssistantVoiceSearchIntent();
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentExperimentId")
    @EnableFeatures({ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH + "<Study"})
    @DisableFeatures({ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:"
                    + VoiceRecognitionHandler.ASSISTANT_EXPERIMENT_ID_PARAM_NAME + "/test"})
    public void
    testStartVoiceRecognition_AssistantExperimentIdDisabled() {
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        verify(mIntent, never())
                .putExtra(eq(VoiceRecognitionHandler.EXTRA_EXPERIMENT_ID), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentExperimentId")
    @EnableFeatures({ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH + "<Study",
            ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:"
                    + VoiceRecognitionHandler.ASSISTANT_EXPERIMENT_ID_PARAM_NAME + "/test"})
    public void
    testStartVoiceRecognition_IncludeExperimentIdInAssistantIntentFromToolbar() {
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        verify(mIntent).putExtra(VoiceRecognitionHandler.EXTRA_EXPERIMENT_ID, "test");
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentExperimentId")
    @EnableFeatures({ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH + "<Study",
            ChromeFeatureList.ASSISTANT_INTENT_EXPERIMENT_ID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:"
                    + VoiceRecognitionHandler.ASSISTANT_EXPERIMENT_ID_PARAM_NAME + "/test"})
    public void
    testStartVoiceRecognition_IncludeExperimentIdInAssistantIntentFromNonToolbar() {
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        startVoiceRecognition(VoiceInteractionSource.OMNIBOX);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        verify(mIntent).putExtra(VoiceRecognitionHandler.EXTRA_EXPERIMENT_ID, "test");
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentPageUrl")
    @EnableFeatures(ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL)
    public void testStartVoiceRecognition_ToolbarButtonIncludesPageUrl() {
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        Assert.assertEquals(mIntent, mWindowAndroid.getCancelableIntent());
        verify(mIntent).putExtra(VoiceRecognitionHandler.EXTRA_PAGE_URL, DEFAULT_URL);
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentPageUrl")
    @EnableFeatures(ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL)
    public void testStartVoiceRecognition_OmitPageUrlWhenAssistantVoiceSearchDisabled() {
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(false).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();

        startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        verify(mIntent, never()).putExtra(eq(VoiceRecognitionHandler.EXTRA_PAGE_URL), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentPageUrl")
    @EnableFeatures(ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL)
    public void testStartVoiceRecognition_OmitPageUrlForNonToolbar() {
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();

        startVoiceRecognition(VoiceInteractionSource.NTP);

        verify(mIntent, never()).putExtra(eq(VoiceRecognitionHandler.EXTRA_PAGE_URL), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentPageUrl")
    @EnableFeatures(ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL)
    public void testStartVoiceRecognition_OmitPageUrlForIncognito() {
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        doReturn(true).when(mTab).isIncognito();

        startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

        verify(mIntent, never()).putExtra(eq(VoiceRecognitionHandler.EXTRA_PAGE_URL), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentPageUrl")
    @EnableFeatures(ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL)
    public void testStartVoiceRecognition_OmitPageUrlForInternalPages() {
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        GURL url = new GURL("chrome://version");
        doReturn(url).when(mTab).getUrl();

        startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

        verify(mIntent, never()).putExtra(eq(VoiceRecognitionHandler.EXTRA_PAGE_URL), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentPageUrl")
    @EnableFeatures(ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL)
    public void testStartVoiceRecognition_OmitPageUrlForNonHttp() {
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        GURL url = new GURL("ftp://example.org/");
        doReturn(url).when(mTab).getUrl();

        startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

        verify(mIntent, never()).putExtra(eq(VoiceRecognitionHandler.EXTRA_PAGE_URL), anyString());
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentTranslateInfo")
    @EnableFeatures({ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH,
            ChromeFeatureList.ASSISTANT_INTENT_TRANSLATE_INFO})
    public void
    testStartVoiceRecognition_ToolbarButtonIncludesTranslateInfo() {
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        Assert.assertEquals(mIntent, mWindowAndroid.getCancelableIntent());
        verify(mIntent).putExtra(VoiceRecognitionHandler.EXTRA_TRANSLATE_ORIGINAL_LANGUAGE, "fr");
        verify(mIntent).putExtra(VoiceRecognitionHandler.EXTRA_TRANSLATE_CURRENT_LANGUAGE, "de");
        verify(mIntent).putExtra(VoiceRecognitionHandler.EXTRA_TRANSLATE_TARGET_LANGUAGE, "ja");
        verify(mIntent).putExtra(VoiceRecognitionHandler.EXTRA_PAGE_URL, DEFAULT_URL);
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentTranslateInfo")
    @EnableFeatures({ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH})
    @DisableFeatures({ChromeFeatureList.ASSISTANT_INTENT_TRANSLATE_INFO})
    public void testStartVoiceRecognition_TranslateExtrasDisabled() {
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

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
    @EnableFeatures({ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH,
            ChromeFeatureList.ASSISTANT_INTENT_TRANSLATE_INFO})
    public void
    testStartVoiceRecognition_NoTranslateExtrasForNonToolbar() {
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        startVoiceRecognition(VoiceInteractionSource.OMNIBOX);

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
    @EnableFeatures({ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH,
            ChromeFeatureList.ASSISTANT_INTENT_TRANSLATE_INFO})
    public void
    testStartVoiceRecognition_NoTranslateExtrasForNonTranslatePage() {
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        doReturn(false).when(mTranslateBridgeWrapper).canManuallyTranslate(notNull());
        startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

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
    @EnableFeatures({ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH,
            ChromeFeatureList.ASSISTANT_INTENT_TRANSLATE_INFO})
    public void
    testStartVoiceRecognition_NoTranslateExtrasWhenLanguagesUndetected() {
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        doReturn(null).when(mTranslateBridgeWrapper).getOriginalLanguage(notNull());
        startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

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
    @EnableFeatures({ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH,
            ChromeFeatureList.ASSISTANT_INTENT_TRANSLATE_INFO})
    public void
    testStartVoiceRecognition_TranslateInfoTargetLanguageOptional() {
        doReturn(true).when(mAssistantVoiceSearchService).canRequestAssistantVoiceSearch();
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        doReturn(null).when(mTranslateBridgeWrapper).getTargetLanguage();
        startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
        verify(mIntent).putExtra(VoiceRecognitionHandler.EXTRA_TRANSLATE_ORIGINAL_LANGUAGE, "fr");
        verify(mIntent).putExtra(VoiceRecognitionHandler.EXTRA_TRANSLATE_CURRENT_LANGUAGE, "de");
        verify(mIntent, never())
                .putExtra(eq(VoiceRecognitionHandler.EXTRA_TRANSLATE_TARGET_LANGUAGE), anyString());
        verify(mIntent).putExtra(VoiceRecognitionHandler.EXTRA_PAGE_URL, DEFAULT_URL);
    }

    @Test
    @SmallTest
    public void testStartVoiceRecognition_StartsVoiceSearchWithFailedIntent() {
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
        mWindowAndroid.setCancelableIntentSuccess(false);
        startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
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
        startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
        Assert.assertEquals(
                VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchStartEventSource());
        Assert.assertEquals(VoiceIntentTarget.SYSTEM, mHandler.getVoiceSearchStartEventTarget());
        verify(mObserver, never()).onVoiceAvailabilityImpacted();
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    @EnableFeatures({ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY,
            ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH})
    public void
    testStartVoiceRecognition_AudioCaptureAllowedByPolicy() {
        setAudioCapturePref(true);
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    @EnableFeatures({ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY,
            ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH})
    public void
    testStartVoiceRecognition_AudioCaptureDisabledByPolicy() {
        setAudioCapturePref(false);
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

        Assert.assertFalse(mWindowAndroid.wasCancelableIntentShown());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    @EnableFeatures({ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY,
            ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH})
    public void
    testStartVoiceRecognition_AudioCapturePolicyAllowsByDefault() {
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
    }

    @Test
    @SmallTest
    @Feature("VoiceSearchAudioCapturePolicy")
    @EnableFeatures({ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH})
    @DisableFeatures({ChromeFeatureList.VOICE_SEARCH_AUDIO_CAPTURE_POLICY})
    public void testStartVoiceRecognition_SkipPolicyWhenFeatureDisabled() {
        setAudioCapturePref(false);
        doReturn(true).when(mAssistantVoiceSearchService).shouldRequestAssistantVoiceSearch();
        startVoiceRecognition(VoiceInteractionSource.TOOLBAR);

        Assert.assertTrue(mWindowAndroid.wasCancelableIntentShown());
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
            Assert.assertTrue(mHandler.getVoiceSearchResult());
            Assert.assertEquals(VoiceIntentTarget.SYSTEM, mHandler.getVoiceSearchResultTarget());
            Assert.assertTrue(confidence == mHandler.getVoiceConfidenceValue());
            Assert.assertEquals(VoiceIntentTarget.SYSTEM, mHandler.getVoiceConfidenceValueTarget());
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
            Assert.assertTrue(mHandler.getVoiceSearchResult());
            Assert.assertEquals(VoiceIntentTarget.SYSTEM, mHandler.getVoiceSearchResultTarget());
            Assert.assertTrue(VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD
                    == mHandler.getVoiceConfidenceValue());
            Assert.assertEquals(VoiceIntentTarget.SYSTEM, mHandler.getVoiceConfidenceValueTarget());
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
    public void testCallback_successWithLanguages() {
        // Needs to run on the UI thread because we use the TemplateUrlService on success.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mWindowAndroid.setVoiceResults(createDummyBundle("testing",
                    VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD, "en-us"));
            startVoiceRecognition(VoiceInteractionSource.OMNIBOX);
            Assert.assertEquals(
                    VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchStartEventSource());
            Assert.assertEquals(
                    VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchFinishEventSource());
            Assert.assertTrue(mHandler.getVoiceSearchResult());
            Assert.assertEquals(VoiceIntentTarget.SYSTEM, mHandler.getVoiceSearchResultTarget());
            Assert.assertTrue(VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD
                    == mHandler.getVoiceConfidenceValue());
            Assert.assertEquals(VoiceIntentTarget.SYSTEM, mHandler.getVoiceConfidenceValueTarget());
            assertVoiceResultsAreEqual(mAutocompleteVoiceResults, new String[] {"testing"},
                    new float[] {
                            VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD},
                    new String[] {"en-us"});
            Assert.assertTrue(mDelegate.getUrl().contains("&hl=en-us"));
        });
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentPageUrl")
    @EnableFeatures(ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL)
    public void testCallback_nonTranscriptionAction() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Bundle bundle = new Bundle();
            bundle.putString(VoiceRecognitionHandler.EXTRA_ACTION_PERFORMED, "TRANSLATE");

            mWindowAndroid.setVoiceResults(bundle);
            startVoiceRecognition(VoiceInteractionSource.TOOLBAR);
            Assert.assertEquals(
                    VoiceInteractionSource.TOOLBAR, mHandler.getVoiceSearchStartEventSource());
            Assert.assertEquals(
                    VoiceInteractionSource.TOOLBAR, mHandler.getVoiceSearchFinishEventSource());
            Assert.assertEquals(
                    AssistantActionPerformed.TRANSLATE, mHandler.getAssistantActionPerformed());
            Assert.assertEquals(
                    VoiceInteractionSource.TOOLBAR, mHandler.getAssistantActionPerformedSource());
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            "VoiceInteraction.QueryDuration.Android"));
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            "VoiceInteraction.QueryDuration.Android"));
        });
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentPageUrl")
    @EnableFeatures(ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL)
    public void testCallback_defaultToTranscription() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mWindowAndroid.setVoiceResults(createDummyBundle(
                    "testing", VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD));
            startVoiceRecognition(VoiceInteractionSource.TOOLBAR);
            Assert.assertEquals(
                    AssistantActionPerformed.TRANSCRIPTION, mHandler.getAssistantActionPerformed());
            Assert.assertEquals(
                    VoiceInteractionSource.TOOLBAR, mHandler.getAssistantActionPerformedSource());
            Assert.assertTrue(mHandler.getVoiceSearchResult());
            assertVoiceResultsAreEqual(mAutocompleteVoiceResults, new String[] {"testing"},
                    new float[] {
                            VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD});
        });
    }

    @Test
    @SmallTest
    @Feature("AssistantIntentPageUrl")
    @DisableFeatures(ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL)
    public void testCallback_pageUrlExtraDisabled() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mWindowAndroid.setVoiceResults(createDummyBundle(
                    "testing", VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD));
            startVoiceRecognition(VoiceInteractionSource.TOOLBAR);
            Assert.assertTrue(mHandler.getVoiceSearchResult());
            // Ensure that we don't record UMA when the feature is disabled.
            Assert.assertEquals(-1, mHandler.getAssistantActionPerformed());
            Assert.assertEquals(-1, mHandler.getAssistantActionPerformedSource());
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
    @DisableFeatures(ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL)
    public void testRecordSuccessMetrics_noActionMetrics() {
        mHandler.setQueryStartTimeForTesting(100L);
        mHandler.recordSuccessMetrics(VoiceInteractionSource.OMNIBOX, VoiceIntentTarget.ASSISTANT,
                AssistantActionPerformed.TRANSCRIPTION);
        Assert.assertEquals(
                VoiceInteractionSource.OMNIBOX, mHandler.getVoiceSearchFinishEventSource());
        Assert.assertEquals(
                VoiceIntentTarget.ASSISTANT, mHandler.getVoiceSearchFinishEventTarget());
        Assert.assertEquals(-1, mHandler.getAssistantActionPerformed());
        Assert.assertEquals(-1, mHandler.getAssistantActionPerformedSource());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android"));
        // Split action metrics should not be recorded.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android.Transcription"));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ASSISTANT_INTENT_PAGE_URL)
    public void testRecordSuccessMetrics_splitActionMetrics() {
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
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android"));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android.Transcription"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android.Translate"));
    }

    @Test
    @SmallTest
    public void testRecordSuccessMetrics_calledWithNullStartTime() {
        mHandler.setQueryStartTimeForTesting(null);
        mHandler.recordSuccessMetrics(VoiceInteractionSource.OMNIBOX, VoiceIntentTarget.SYSTEM,
                AssistantActionPerformed.TRANSCRIPTION);
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "VoiceInteraction.QueryDuration.Android"));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
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
                RecordHistogram.getHistogramValueCountForTesting(
                        "VoiceInteraction.AudioPermissionEvent",
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
                RecordHistogram.getHistogramValueCountForTesting(
                        "VoiceInteraction.AudioPermissionEvent",
                        AudioPermissionState.DENIED_CAN_ASK_AGAIN));
    }

    @Test
    @SmallTest
    public void testRecordAudioState_granted() {
        mPermissionDelegate.setHasPermission(true);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mHandler.startVoiceRecognition(VoiceInteractionSource.OMNIBOX); });
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "VoiceInteraction.AudioPermissionEvent", AudioPermissionState.GRANTED));
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

    private static void setAudioCapturePref(boolean value) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            UserPrefs.get(Profile.getLastUsedRegularProfile())
                    .setBoolean(Pref.AUDIO_CAPTURE_ALLOWED, value);
        });
    }
}
