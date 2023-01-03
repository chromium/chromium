// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.speech.RecognizerIntent;
import android.view.ViewGroup;

import androidx.annotation.ColorRes;
import androidx.annotation.Nullable;

import org.junit.Assert;
import org.mockito.Mock;

import org.chromium.base.FeatureList;
import org.chromium.base.jank_tracker.DummyJankTracker;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxPedalDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownEmbedder;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownScrollListener;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceInteractionSource;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceResult;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.ui.permissions.PermissionCallback;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * A helper class that simplifies creation of {@link VoiceRecognitionHandler}'s
 * test dependencies and provides utility methods for tests.
 */
public class RecognitionTestHelper {
    public static final String DEFAULT_URL = "https://example.com/";
    public static final String DEFAULT_USER_EMAIL = "test@test.com";

    @Mock
    OmniboxPedalDelegate mPedalDelegate;
    @Mock
    ModalDialogManager mModalDialogManager;

    private final TestWindowAndroid mTestWindowAndroid;
    private final TestDataProvider mTestDataProvider;
    private final TestAndroidPermissionDelegate mAndroidPermissionDelegate;
    private final TestDelegate mDelegate;
    private final TestAutocompleteCoordinator mAutocompleteCoordinator;
    private final TestVoiceRecognitionHandler mHandler;

    public RecognitionTestHelper(AssistantVoiceSearchService assistantVoiceSearchService,
            ObservableSupplierImpl<Profile> profileSupplier, Activity activity) {
        mTestDataProvider = new TestDataProvider();
        mTestWindowAndroid = new TestWindowAndroid(activity);
        mAndroidPermissionDelegate = new TestAndroidPermissionDelegate();

        ViewGroup parent = (ViewGroup) activity.findViewById(android.R.id.content);
        Assert.assertNotNull(parent);
        mAutocompleteCoordinator = new TestAutocompleteCoordinator(parent, null, null, null,
                mModalDialogManager, mTestDataProvider, profileSupplier, mPedalDelegate);
        mDelegate =
                new TestDelegate(mTestWindowAndroid, mTestDataProvider, mAutocompleteCoordinator);
        mHandler = new TestVoiceRecognitionHandler(
                mDelegate, assistantVoiceSearchService, profileSupplier);
    }

    public TestVoiceRecognitionHandler getVoiceRecognitionHandler() {
        return mHandler;
    }

    public TestWindowAndroid getWindowAndroid() {
        return mTestWindowAndroid;
    }

    public TestDelegate getDelegate() {
        return mDelegate;
    }

    public TestDataProvider getDataProvider() {
        return mTestDataProvider;
    }

    public TestAutocompleteCoordinator getAutocompleteCoordinator() {
        return mAutocompleteCoordinator;
    }

    public TestAndroidPermissionDelegate getAndroidPermissionDelegate() {
        return mAndroidPermissionDelegate;
    }

    /**
     * Resets test features in order to allow fallback values to be taken from
     * defaults.
     * This achieves a similar objective to EnableFeatures/DisableFeatures, but
     * is more consistent and prevents re-start of the batched tests.
     * Note that multiple {@link VoiceRecognitionHandler}'s tests rely on currently
     * set feature defaults and any changes done there that drag into test execution
     * produce
     * false failures.
     *
     * Should be called in the test set up.
     */
    public static FeatureList.TestValues resetTestFeatures() {
        FeatureList.TestValues features = new FeatureList.TestValues();
        FeatureList.setTestValues(features);
        FeatureList.setTestCanUseDefaultsForTesting();
        return features;
    }

    /**
     * Creates a test bundle.
     *
     * @param text       a query representing transcription result
     * @param confidence confidence value for the query
     */
    public static Bundle createDummyBundle(String text, float confidence) {
        return createDummyBundle(new String[] {text}, new float[] {confidence}, null);
    }

    /**
     * Creates a test bundle
     *
     * @param text       a query representing transcription result
     * @param confidence confidence value for the query
     * @param language   optional - recognized query language
     */
    public static Bundle createDummyBundle(
            String text, float confidence, @Nullable String language) {
        return createDummyBundle(new String[] {text}, new float[] {confidence},
                language == null ? null : new String[] {language});
    }

    /**
     * Creates a test bundle.
     *
     * @param texts       the queries representing transcription results
     * @param confidences confidence values for corresponding queries
     */
    public static Bundle createDummyBundle(String[] texts, float[] confidences) {
        return createDummyBundle(texts, confidences, null);
    }

    /**
     * Creates a test bundle.
     *
     * @param texts       the queries representing transcription results
     * @param confidences confidence values for corresponding queries
     * @param languages   optional - recognized query languages
     */
    public static Bundle createDummyBundle(
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

    /** Sets the value of the {@link Pref.AUDIO_CAPTURED_ALLOWED}. */
    public static void setAudioCapturePref(boolean value) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            UserPrefs.get(Profile.getLastUsedRegularProfile())
                    .setBoolean(Pref.AUDIO_CAPTURE_ALLOWED, value);
        });
    }

    /**
     * Toggles specific features on.
     *
     * @param features Comma-separated list of features to be enabled.
     */
    public static void enableFeatures(FeatureList.TestValues testFeatureList, String... features) {
        for (String feature : features) {
            testFeatureList.addFeatureFlagOverride(feature, true);
        }
    }

    /**
     * Toggles specific features off.
     *
     * @param features Comma-separated list of features to be disabled.
     */
    public static void disableFeatures(FeatureList.TestValues testFeatureList, String... features) {
        for (String feature : features) {
            testFeatureList.addFeatureFlagOverride(feature, false);
        }
    }

    /**
     * Kicks off voice recognition with the given source, for testing
     * {@linkVoiceRecognitionHandler.VoiceRecognitionCompleteCallback}.
     *
     * @param source The source of the voice recognition initiation.
     */
    public static void startVoiceRecognition(TestAndroidPermissionDelegate permissionDelegate,
            VoiceRecognitionHandler handler, @VoiceInteractionSource int source) {
        permissionDelegate.setHasPermission(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> { handler.startVoiceRecognition(source); });
    }

    public static void assertVoiceResultsAreEqual(
            List<VoiceResult> results, String[] texts, float[] confidences) {
        assertVoiceResultsAreEqual(results, texts, confidences, null);
    }

    public static void assertVoiceResultsAreEqual(
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
    /**
     * TODO(crbug.com/962527): Remove this dependency on
     * {@link AutocompleteCoordinator}.
     */
    public static class TestAutocompleteCoordinator extends AutocompleteCoordinator {
        private List<VoiceResult> mAutocompleteVoiceResults;

        public TestAutocompleteCoordinator(ViewGroup parent, AutocompleteDelegate delegate,
                OmniboxSuggestionsDropdownEmbedder dropdownEmbedder,
                UrlBarEditingTextStateProvider urlBarEditingTextProvider,
                ModalDialogManager modalDialogManager, TestDataProvider dataProvider,
                ObservableSupplierImpl<Profile> profileSupplier,
                OmniboxPedalDelegate pedalDelegate) {
            // clang-format off
      super(parent, delegate, dropdownEmbedder, urlBarEditingTextProvider,
          () -> modalDialogManager, null, null, dataProvider,
          profileSupplier, (tab) -> {
          }, null, (url) -> false, new DummyJankTracker(),
          pedalDelegate, new OmniboxSuggestionsDropdownScrollListener() {});
            // clang-format on
        }

        @Override
        public void onVoiceResults(List<VoiceResult> results) {
            mAutocompleteVoiceResults = results;
        }

        public List<VoiceResult> getAutocompleteVoiceResults() {
            return mAutocompleteVoiceResults;
        }
    }

    /**
     * Test implementation of {@link VoiceRecognitionHandler.Delegate}.
     */
    public static class TestDelegate implements VoiceRecognitionHandler.Delegate {
        private String mUrl;
        private final AutocompleteCoordinator mAutocompleteCoordinator;
        private final LocationBarDataProvider mDataProvider;
        private final WindowAndroid mWindowAndroid;

        TestDelegate(WindowAndroid windowAndroid, LocationBarDataProvider dataProvider,
                TestAutocompleteCoordinator coordinator) {
            mWindowAndroid = windowAndroid;
            mDataProvider = dataProvider;
            mAutocompleteCoordinator = coordinator;
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

        @Override
        public void notifyVoiceRecognitionCanceled() {}
    }

    /**
     * Test implementation of {@link ActivityWindowAndroid}.
     */
    public static class TestWindowAndroid extends ActivityWindowAndroid {
        private boolean mCancelableIntentSuccess = true;
        private int mResultCode = Activity.RESULT_OK;
        private Intent mResults = new Intent();
        private Activity mActivity;
        private boolean mWasCancelableIntentShown;
        private Intent mCancelableIntent;
        private IntentCallback mCallback;
        private TestAndroidPermissionDelegate mAndroidPermissionDelegate;

        public TestWindowAndroid(Activity activity) {
            super(activity, /* listenToActivityState= */ true,
                    IntentRequestTracker.createFromActivity(activity));
            setAndroidPermissionDelegate(new TestAndroidPermissionDelegate());
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
                callback.onIntentCompleted(mResultCode, mResults);
                return 0;
            }
            return WindowAndroid.START_INTENT_FAILURE;
        }

        public void setAndroidPermissionDelegate(TestAndroidPermissionDelegate delegate) {
            super.setAndroidPermissionDelegate(delegate);
            mAndroidPermissionDelegate = delegate;
        }

        public TestAndroidPermissionDelegate getAndroidPermissionDelegate() {
            return mAndroidPermissionDelegate;
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
    public static class TestAndroidPermissionDelegate implements AndroidPermissionDelegate {
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

    /**
     * Test implementation of {@link ToolbarDataProvider}.
     */
    public static class TestDataProvider implements LocationBarDataProvider {
        private boolean mIncognito;
        private Tab mTab;

        public void setIncognito(boolean incognito) {
            mIncognito = incognito;
        }

        public void setTab(Tab tab) {
            mTab = tab;
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
        public GURL getCurrentGurl() {
            return GURL.emptyGURL();
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
        public int getPageClassification(boolean isFocusedFromFakebox, boolean isPrefetch) {
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
     * An implementation of the real {@link VoiceRecognitionHandler} except instead
     * of recording histograms we just flag whether we would have or not.
     */
    public class TestVoiceRecognitionHandler extends VoiceRecognitionHandler {
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

        public TestVoiceRecognitionHandler(Delegate delegate,
                AssistantVoiceSearchService assistantVoiceSearchService,
                ObservableSupplierImpl<Profile> profileSupplier) {
            super(delegate, () -> assistantVoiceSearchService, () -> {}, profileSupplier);
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
}
