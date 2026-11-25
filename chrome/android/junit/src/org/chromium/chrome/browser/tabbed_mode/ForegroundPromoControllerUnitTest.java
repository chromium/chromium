// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ChromeInactivityTracker;
import org.chromium.chrome.browser.ChromeInactivityTracker.InactivityObserver;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics.LaunchCause;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.settings.GlicNavigationUtils;
import org.chromium.chrome.browser.glic.ui.GlicPromoCoordinator;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher.ActivityState;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tips.TipsOptInCoordinator;
import org.chromium.chrome.browser.tips.TipsUtils;
import org.chromium.chrome.browser.tips.TipsUtils.TipsOptInPromoTriggerOutcome;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarPrefs;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.TestActivity;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

/** Unit tests for {@link ForegroundPromoController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures({
    ChromeFeatureList.ANDROID_BOTTOM_BAR,
    ChromeFeatureList.ANDROID_TIPS_NOTIFICATIONS
})
public class ForegroundPromoControllerUnitTest {
    private static final long TIME_IN_BACKGROUND_MS = TimeUnit.HOURS.toMillis(5);
    private static final String COLD_START_HISTOGRAM =
            TipsUtils.TRIGGER_OUTCOME_HISTOGRAM_PREFIX
                    + TipsUtils.TRIGGER_OUTCOME_COLD_START_SUFFIX;
    private static final String WARM_START_HISTOGRAM =
            TipsUtils.TRIGGER_OUTCOME_HISTOGRAM_PREFIX
                    + TipsUtils.TRIGGER_OUTCOME_WARM_START_SUFFIX;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Profile mProfile;
    @Mock private ChromeInactivityTracker mInactivityTracker;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Tracker mTracker;
    @Mock private OneshotSupplier<ChromeInactivityTracker> mInactivityTrackerSupplier;
    @Mock private OneshotSupplier<Boolean> mTrackerInitializedSupplier;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    @Captor private ArgumentCaptor<Callback<ChromeInactivityTracker>> mInactivityTrackerCallback;
    @Captor private ArgumentCaptor<Callback<Boolean>> mTrackerInitializedCallback;
    @Captor private ArgumentCaptor<InactivityObserver> mInactivityObserver;

    private Activity mActivity;
    private Supplier<Profile> mProfileSupplier;
    private Supplier<BottomSheetController> mBottomSheetControllerSupplier;
    private Supplier<SnackbarManager> mSnackbarManagerSupplier;
    private @LaunchCause int mLaunchCause = LaunchCause.MAIN_LAUNCHER_ICON;

    /** Values passed to the Tips opt-in promo check, in order. */
    private final List<Long> mTipsPromoTimes = new ArrayList<>();

    private ForegroundPromoController mController;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        mProfileSupplier = () -> mProfile;
        mBottomSheetControllerSupplier = () -> mBottomSheetController;
        mSnackbarManagerSupplier = () -> mSnackbarManager;
        doReturn(mInactivityTracker).when(mInactivityTrackerSupplier).get();
        doReturn(ActivityState.CREATED_WITH_NATIVE)
                .when(mActivityLifecycleDispatcher)
                .getCurrentActivityState();

        TrackerFactory.setTrackerForTests(mTracker);
        GlicEnabling.setEnabledForTesting(false);
    }

    @After
    public void tearDown() {
        if (mController != null) {
            mController.destroy();
        }
        TrackerFactory.setTrackerForTests(null);
        GlicEnabling.setEnabledForTesting(false);
        GlicNavigationUtils.setLauncher(null);
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS);
    }

    private void createController() {
        mController =
                new ForegroundPromoController(
                        mActivity,
                        mProfileSupplier,
                        mInactivityTrackerSupplier,
                        mTrackerInitializedSupplier,
                        mBottomSheetControllerSupplier,
                        mSnackbarManagerSupplier,
                        mActivityLifecycleDispatcher,
                        () -> mLaunchCause);
        mController.setTipsOptInPromoShowCallbackForTesting(mTipsPromoTimes::add);
    }

    private void setActivityState(@ActivityState int state) {
        doReturn(state).when(mActivityLifecycleDispatcher).getCurrentActivityState();
    }

    private void resumeActivity() {
        setActivityState(ActivityState.RESUMED_WITH_NATIVE);
        mController.onResumeWithNativeCompleted();
    }

    private void pauseActivity() {
        setActivityState(ActivityState.PAUSED_WITH_NATIVE);
    }

    /** Delivers the inactivity tracker and returns the observer the controller registered. */
    private InactivityObserver deliverInactivityTracker() {
        verify(mInactivityTrackerSupplier).onAvailable(mInactivityTrackerCallback.capture());
        mInactivityTrackerCallback.getValue().onResult(mInactivityTracker);
        verify(mInactivityTracker).addObserver(mInactivityObserver.capture());
        return mInactivityObserver.getValue();
    }

    /** Sets whether the feature engagement tracker is initialized and runs its callback. */
    private void deliverTrackerInitialized(boolean initialized) {
        doReturn(initialized).when(mTrackerInitializedSupplier).get();
        verify(mTrackerInitializedSupplier).onAvailable(mTrackerInitializedCallback.capture());
        mTrackerInitializedCallback.getValue().onResult(initialized);
    }

    /** Sets up the Glic promo so that it shows the bottom sheet. */
    private void setUpGlicPromoToShow() {
        GlicEnabling.setEnabledForTesting(true);
        doReturn(false)
                .when(mTracker)
                .wouldTriggerHelpUi(
                        FeatureConstants.ADAPTIVE_BUTTON_PIN_GLIC_TOOLBAR_BUTTON_FEATURE);
        AdaptiveToolbarPrefs.saveToolbarButtonManualOverride(AdaptiveToolbarButtonVariant.AUTO);
    }

    @Test
    public void testPromosOnResume_GlicPromoShown_SkipsTipsPromo() {
        setUpGlicPromoToShow();
        createController();
        deliverInactivityTracker();
        deliverTrackerInitialized(/* initialized= */ true);

        resumeActivity();

        assertNotNull(mController.getGlicPromoCoordinatorForTesting());
        assertTrue(mTipsPromoTimes.isEmpty());
    }

    @Test
    public void testPromosOnResume_GlicPromoShown_RecordsTriggerOutcome() {
        setUpGlicPromoToShow();
        createController();
        InactivityObserver observer = deliverInactivityTracker();

        // First foreground + resume is a cold start.
        HistogramWatcher coldStartWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                COLD_START_HISTOGRAM,
                                TipsOptInPromoTriggerOutcome.SKIPPED_FOR_GLIC_PROMO)
                        .expectNoRecords(WARM_START_HISTOGRAM)
                        .build();
        deliverTrackerInitialized(/* initialized= */ true);
        observer.onForegrounded(TIME_IN_BACKGROUND_MS);
        resumeActivity();
        coldStartWatcher.assertExpected();

        // Any later foreground + resume is a warm start.
        HistogramWatcher warmStartWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                WARM_START_HISTOGRAM,
                                TipsOptInPromoTriggerOutcome.SKIPPED_FOR_GLIC_PROMO)
                        .expectNoRecords(COLD_START_HISTOGRAM)
                        .build();
        pauseActivity();
        // Showing the Glic promo marks it as evaluated; clear that so it wins again.
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED);
        observer.onForegrounded(TIME_IN_BACKGROUND_MS);
        resumeActivity();
        warmStartWatcher.assertExpected();
        assertTrue(mTipsPromoTimes.isEmpty());
    }

    @Test
    public void testTipsOptInPromo_RecordsColdAndWarmStartOutcomes() {
        mController =
                new ForegroundPromoController(
                        mActivity,
                        mProfileSupplier,
                        mInactivityTrackerSupplier,
                        mTrackerInitializedSupplier,
                        mBottomSheetControllerSupplier,
                        mSnackbarManagerSupplier,
                        mActivityLifecycleDispatcher,
                        () -> mLaunchCause);
        InactivityObserver observer = deliverInactivityTracker();

        // First foreground + resume forwards isColdStart=true to TipsUtils.
        HistogramWatcher coldStartWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                COLD_START_HISTOGRAM,
                                TipsOptInPromoTriggerOutcome.SKIPPED_FEATURE_DISABLED)
                        .expectNoRecords(WARM_START_HISTOGRAM)
                        .build();
        observer.onForegrounded(TIME_IN_BACKGROUND_MS);
        resumeActivity();
        deliverTrackerInitialized(/* initialized= */ true);
        coldStartWatcher.assertExpected();

        // Subsequent foregrounds forward isColdStart=false.
        HistogramWatcher warmStartWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                WARM_START_HISTOGRAM,
                                TipsOptInPromoTriggerOutcome.SKIPPED_FEATURE_DISABLED)
                        .expectNoRecords(COLD_START_HISTOGRAM)
                        .build();
        pauseActivity();
        observer.onForegrounded(TIME_IN_BACKGROUND_MS);
        resumeActivity();
        warmStartWatcher.assertExpected();
    }

    @Test
    public void testPromosOnResume_GlicPromoNotShown_FallsBackToTipsPromo() {
        createController();
        InactivityObserver observer = deliverInactivityTracker();
        deliverTrackerInitialized(/* initialized= */ true);

        observer.onForegrounded(TIME_IN_BACKGROUND_MS);
        resumeActivity();

        assertNull(mController.getGlicPromoCoordinatorForTesting());
        assertEquals(List.of(TIME_IN_BACKGROUND_MS), mTipsPromoTimes);
    }

    @Test
    public void testPromosOnResume_NoBottomSheetController_SkipsBothPromos() {
        mBottomSheetController = null;
        setUpGlicPromoToShow();
        createController();
        deliverInactivityTracker();
        deliverTrackerInitialized(/* initialized= */ true);

        resumeActivity();

        assertNull(mController.getGlicPromoCoordinatorForTesting());
    }

    @Test
    public void testPromosOnResume_StartWithoutResume_DoesNotEvaluate() {
        setUpGlicPromoToShow();
        createController();
        InactivityObserver observer = deliverInactivityTracker();
        setActivityState(ActivityState.STARTED_WITH_NATIVE);

        observer.onForegrounded(TIME_IN_BACKGROUND_MS);
        deliverTrackerInitialized(/* initialized= */ true);

        assertNull(mController.getGlicPromoCoordinatorForTesting());
        assertTrue(mTipsPromoTimes.isEmpty());
    }

    @Test
    public void testPromosOnResume_ActivityFinishing_DoesNotEvaluate() {
        setUpGlicPromoToShow();
        createController();
        InactivityObserver observer = deliverInactivityTracker();
        deliverTrackerInitialized(/* initialized= */ true);
        doReturn(true).when(mActivityLifecycleDispatcher).isActivityFinishingOrDestroyed();

        observer.onForegrounded(TIME_IN_BACKGROUND_MS);
        resumeActivity();

        assertNull(mController.getGlicPromoCoordinatorForTesting());
        assertTrue(mTipsPromoTimes.isEmpty());
    }

    @Test
    public void testPromosOnResume_PauseResumeWithoutStop_DoesNotReevaluate() {
        createController();
        InactivityObserver observer = deliverInactivityTracker();
        deliverTrackerInitialized(/* initialized= */ true);

        observer.onForegrounded(TIME_IN_BACKGROUND_MS);
        resumeActivity();
        assertEquals(List.of(TIME_IN_BACKGROUND_MS), mTipsPromoTimes);

        // Pause and resume without a stop/start (e.g. permission dialog or multi-window focus).
        pauseActivity();
        resumeActivity();
        assertEquals(List.of(TIME_IN_BACKGROUND_MS), mTipsPromoTimes);

        // A new foreground session (after stop) re-evaluates on the next resume.
        observer.onForegrounded(TIME_IN_BACKGROUND_MS + 1);
        resumeActivity();
        assertEquals(List.of(TIME_IN_BACKGROUND_MS, TIME_IN_BACKGROUND_MS + 1), mTipsPromoTimes);
    }

    @Test
    public void testPromosOnResume_TrackerNotInitialized_SkipsPromos() {
        setUpGlicPromoToShow();
        createController();
        deliverInactivityTracker();
        deliverTrackerInitialized(/* initialized= */ false);

        resumeActivity();

        assertNull(mController.getGlicPromoCoordinatorForTesting());
        assertTrue(mTipsPromoTimes.isEmpty());
    }

    @Test
    public void testPromosOnResume_TrackerReadyAfterResume_EvaluatesOnceWithCapturedTime() {
        createController();
        InactivityObserver observer = deliverInactivityTracker();

        observer.onForegrounded(TIME_IN_BACKGROUND_MS);
        resumeActivity();
        assertTrue(mTipsPromoTimes.isEmpty());

        deliverTrackerInitialized(/* initialized= */ true);
        assertEquals(List.of(TIME_IN_BACKGROUND_MS), mTipsPromoTimes);
    }

    @Test
    public void testPromosOnResume_TrackerReadyAfterPause_DoesNotEvaluateUntilResumed() {
        createController();
        InactivityObserver observer = deliverInactivityTracker();

        observer.onForegrounded(TIME_IN_BACKGROUND_MS);
        resumeActivity();
        pauseActivity();

        deliverTrackerInitialized(/* initialized= */ true);
        assertTrue(mTipsPromoTimes.isEmpty());

        resumeActivity();
        assertEquals(List.of(TIME_IN_BACKGROUND_MS), mTipsPromoTimes);
    }

    @Test
    public void testPromosOnResume_TrackerReadyBeforeForeground_NoDoubleEvaluation() {
        createController();
        InactivityObserver observer = deliverInactivityTracker();
        // Tracker initializes before onForegrounded and before resume.
        deliverTrackerInitialized(/* initialized= */ true);
        assertTrue(mTipsPromoTimes.isEmpty());

        observer.onForegrounded(TIME_IN_BACKGROUND_MS);
        assertTrue(mTipsPromoTimes.isEmpty());

        resumeActivity();
        assertEquals(List.of(TIME_IN_BACKGROUND_MS), mTipsPromoTimes);
    }

    @Test
    public void testPromosOnResume_ExternalLaunch_SkipsTips() {
        mLaunchCause = LaunchCause.EXTERNAL_VIEW_INTENT;
        createController();
        InactivityObserver observer = deliverInactivityTracker();
        deliverTrackerInitialized(/* initialized= */ true);

        observer.onForegrounded(TIME_IN_BACKGROUND_MS);
        resumeActivity();
        assertTrue(mTipsPromoTimes.isEmpty());

        // Non-external return (e.g. Recents) still evaluates Tips.
        pauseActivity();
        mLaunchCause = LaunchCause.RECENTS;
        observer.onForegrounded(TIME_IN_BACKGROUND_MS);
        resumeActivity();
        assertEquals(List.of(TIME_IN_BACKGROUND_MS), mTipsPromoTimes);
    }

    @Test
    public void testPromosOnResume_ExternalLaunch_GlicStillShown() {
        mLaunchCause = LaunchCause.NOTIFICATION;
        setUpGlicPromoToShow();
        createController();
        InactivityObserver observer = deliverInactivityTracker();
        deliverTrackerInitialized(/* initialized= */ true);

        observer.onForegrounded(TIME_IN_BACKGROUND_MS);
        resumeActivity();

        assertNotNull(mController.getGlicPromoCoordinatorForTesting());
        assertTrue(mTipsPromoTimes.isEmpty());
    }

    @Test
    public void testMaybeShowGlicPromo_WouldTrigger_ToolbarNotPinned() {
        createController();

        GlicEnabling.setEnabledForTesting(true);
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED);

        // Mock tracker wouldTriggerHelpUi to return true.
        doReturn(true)
                .when(mTracker)
                .wouldTriggerHelpUi(
                        FeatureConstants.ADAPTIVE_BUTTON_PIN_GLIC_TOOLBAR_BUTTON_FEATURE);

        // Ensure toolbar is not pinned.
        AdaptiveToolbarPrefs.saveToolbarButtonManualOverride(AdaptiveToolbarButtonVariant.AUTO);

        mController.maybeShowGlicPromo();

        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED, false));
        // Toolbar should be manually overridden to GLIC.
        assertEquals(
                AdaptiveToolbarButtonVariant.GLIC, AdaptiveToolbarPrefs.getCustomizationSetting());
        // Promo coordinator should be null (not shown).
        assertNull(mController.getGlicPromoCoordinatorForTesting());

        // Verify that the trigger event was notified to the tracker.
        verify(mTracker).notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_GLIC_IPH_TRIGGER);
    }

    @Test
    public void testMaybeShowGlicPromo_WouldTrigger_ToolbarPinned() {
        createController();

        GlicEnabling.setEnabledForTesting(true);
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED);

        // Mock tracker wouldTriggerHelpUi to return true.
        doReturn(true)
                .when(mTracker)
                .wouldTriggerHelpUi(
                        FeatureConstants.ADAPTIVE_BUTTON_PIN_GLIC_TOOLBAR_BUTTON_FEATURE);

        // Ensure toolbar is pinned.
        AdaptiveToolbarPrefs.saveToolbarButtonManualOverride(AdaptiveToolbarButtonVariant.NEW_TAB);

        mController.maybeShowGlicPromo();

        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED, false));
        // Toolbar should still be NEW_TAB.
        assertEquals(
                AdaptiveToolbarButtonVariant.NEW_TAB,
                AdaptiveToolbarPrefs.getCustomizationSetting());
        // Promo coordinator should be non-null (shown).
        assertNotNull(mController.getGlicPromoCoordinatorForTesting());

        // Verify that the trigger event was notified to the tracker.
        verify(mTracker).notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_GLIC_IPH_TRIGGER);
    }

    @Test
    public void testMaybeShowGlicPromo_WouldNotTrigger() {
        createController();

        GlicEnabling.setEnabledForTesting(true);
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED);

        // Mock tracker wouldTriggerHelpUi to return false.
        doReturn(false)
                .when(mTracker)
                .wouldTriggerHelpUi(
                        FeatureConstants.ADAPTIVE_BUTTON_PIN_GLIC_TOOLBAR_BUTTON_FEATURE);

        // Toolbar is AUTO (not pinned).
        AdaptiveToolbarPrefs.saveToolbarButtonManualOverride(AdaptiveToolbarButtonVariant.AUTO);

        mController.maybeShowGlicPromo();

        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED, false));
        // Toolbar should still be AUTO.
        assertEquals(
                AdaptiveToolbarButtonVariant.AUTO, AdaptiveToolbarPrefs.getCustomizationSetting());
        // Promo coordinator should be non-null (shown).
        assertNotNull(mController.getGlicPromoCoordinatorForTesting());

        // Verify that the trigger event was notified to the tracker.
        verify(mTracker).notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_GLIC_IPH_TRIGGER);
    }

    @Test
    public void testMaybeShowGlicPromo_DisabledByParam() {
        createController();

        GlicEnabling.setEnabledForTesting(true);
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED);
        FeatureOverrides.overrideParam(ChromeFeatureList.GLIC, "glic-bottom-sheet-promo", false);

        // Mock tracker wouldTriggerHelpUi to return false.
        doReturn(false)
                .when(mTracker)
                .wouldTriggerHelpUi(
                        FeatureConstants.ADAPTIVE_BUTTON_PIN_GLIC_TOOLBAR_BUTTON_FEATURE);

        // Toolbar is AUTO (not pinned).
        AdaptiveToolbarPrefs.saveToolbarButtonManualOverride(AdaptiveToolbarButtonVariant.AUTO);

        mController.maybeShowGlicPromo();

        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED, false));
        // Toolbar should still be AUTO.
        assertEquals(
                AdaptiveToolbarButtonVariant.AUTO, AdaptiveToolbarPrefs.getCustomizationSetting());
        // Promo coordinator should be null (not shown due to param).
        assertNull(mController.getGlicPromoCoordinatorForTesting());

        // Verify that the trigger event was notified to the tracker.
        verify(mTracker).notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_GLIC_IPH_TRIGGER);
    }

    @Test
    public void testMaybeShowGlicPromo_GlicDisabledByFlags_ResetsPref() {
        createController();

        GlicEnabling.setEnabledForTesting(false);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED, true);
        assertFalse(mController.maybeShowGlicPromo());

        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .contains(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED));
    }

    @Test
    public void testDestroy() {
        createController();
        InactivityObserver observer = deliverInactivityTracker();

        GlicPromoCoordinator glicPromoCoordinator = mock(GlicPromoCoordinator.class);
        TipsOptInCoordinator tipsOptInCoordinator = mock(TipsOptInCoordinator.class);
        mController.setGlicPromoCoordinatorForTesting(glicPromoCoordinator);
        mController.setTipsOptInCoordinatorForTesting(tipsOptInCoordinator);

        mController.destroy();

        verify(glicPromoCoordinator).destroy();
        verify(tipsOptInCoordinator).destroy();
        verify(mInactivityTracker).removeObserver(observer);

        mController = null;
    }

    @Test
    public void testDestroy_CancelsPendingPromoEvaluation() {
        setUpGlicPromoToShow();
        createController();
        deliverInactivityTracker();
        setActivityState(ActivityState.RESUMED_WITH_NATIVE);

        mController.destroy();
        deliverTrackerInitialized(/* initialized= */ true);
        mController.onResumeWithNativeCompleted();

        assertNull(mController.getGlicPromoCoordinatorForTesting());
        assertTrue(mTipsPromoTimes.isEmpty());

        mController = null;
    }
}
