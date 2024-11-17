// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_PENDING_OS_CHOICE_DIALOG_SHOWN_ATTEMPTS;

import android.content.Context;
import android.os.Looper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.search_engines.choice_screen.ChoiceDialogCoordinator.DialogSuppressionStatus;
import org.chromium.chrome.browser.search_engines.choice_screen.ChoiceDialogMediator.DialogType;
import org.chromium.chrome.browser.search_engines.choice_screen.ChoiceDialogMediator.LaunchChoiceScreenTapHandlingStatus;
import org.chromium.components.search_engines.SearchEngineChoiceService;
import org.chromium.components.search_engines.SearchEnginesFeatures;
import org.chromium.components.search_engines.test.util.SearchEnginesFeaturesTestUtil;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogPriority;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.time.Duration;
import java.util.Map;

@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({SearchEnginesFeatures.CLAY_BLOCKING})
public class ChoiceDialogCoordinatorUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    public @Rule FakeTimeTestRule mFakeTimeRule = new FakeTimeTestRule();

    private @Mock Context mContext;
    private @Mock ChoiceDialogCoordinator.ViewHolder mViewHolder;
    private @Mock ModalDialogManager mModalDialogManager;
    private @Mock ActivityLifecycleDispatcher mLifecycleDispatcher;
    private @Mock SearchEngineChoiceService mSearchEngineChoiceService;
    private @Captor ArgumentCaptor<PropertyModel> mModelCaptor;
    private @Captor ArgumentCaptor<PauseResumeWithNativeObserver> mLifecycleObserverCaptor;

    @Before
    public void setUp() {
        SearchEngineChoiceService.setInstanceForTests(mSearchEngineChoiceService);
        setUpDialogObserverCapture();
    }

    @Test
    public void testMaybeShow() {
        // For code coverage.
        SearchEnginesFeaturesTestUtil.configureClayBlockingFeatureParams(
                Map.of("enable_verbose_logging", "true"));

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Search.OsDefaultsChoice.DialogShownAttempt", 1)
                        .expectIntRecord(
                                "Search.OsDefaultsChoice.DialogSuppressionStatus",
                                DialogSuppressionStatus.CAN_SHOW)
                        .build();
        var shouldShowSupplier = new ObservableSupplierImpl<>(true);
        doReturn(shouldShowSupplier)
                .when(mSearchEngineChoiceService)
                .getIsDeviceChoiceRequiredSupplier();
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();

        assertTrue(ChoiceDialogCoordinator.maybeShowInternal(this::createCoordinatorWithMocks));
        assertTrue(shouldShowSupplier.hasObservers()); // The dialog started observing.

        shadowOf(Looper.getMainLooper()).idle();
        verify(mModalDialogManager)
                .showDialog(
                        mModelCaptor.capture(),
                        eq(ModalDialogType.APP),
                        eq(ModalDialogPriority.VERY_HIGH));
        verify(mSearchEngineChoiceService).notifyDeviceChoiceBlockShown();
        verify(mViewHolder).updateViewForType(eq(DialogType.CHOICE_LAUNCH));
        assertEquals(
                1,
                ChromeSharedPreferences.getInstance()
                        .readInt(SEARCH_ENGINE_CHOICE_PENDING_OS_CHOICE_DIALOG_SHOWN_ATTEMPTS));

        shouldShowSupplier.set(false);

        shadowOf(Looper.getMainLooper()).idle();
        verify(mSearchEngineChoiceService).notifyDeviceChoiceBlockCleared();
        verify(mViewHolder).updateViewForType(eq(DialogType.CHOICE_CONFIRM));

        PropertyModel model = mModelCaptor.getValue();
        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);
        controller.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);

        verify(mModalDialogManager).dismissDialog(any(), eq(DialogDismissalCause.UNKNOWN));
        assertFalse(shouldShowSupplier.hasObservers()); // The dialog stopped observing.
        histogramWatcher.assertExpected();
    }

    @Test
    public void testMaybeShow_doesNotShowWhenNotEligible() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Search.OsDefaultsChoice.DialogShownAttempt")
                        .expectNoRecords("Search.OsDefaultsChoice.DialogSuppressionStatus")
                        .build();
        doReturn(false).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();

        assertFalse(ChoiceDialogCoordinator.maybeShowInternal(this::createCoordinatorWithMocks));

        shadowOf(Looper.getMainLooper()).idle();
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());
        verify(mSearchEngineChoiceService, never()).notifyDeviceChoiceBlockShown();
        verify(mSearchEngineChoiceService, never()).notifyDeviceChoiceBlockCleared();
        assertEquals(
                0,
                ChromeSharedPreferences.getInstance()
                        .readInt(SEARCH_ENGINE_CHOICE_PENDING_OS_CHOICE_DIALOG_SHOWN_ATTEMPTS));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testMaybeShow_doesNotShowInDarkLaunchMode() {
        SearchEnginesFeaturesTestUtil.configureClayBlockingFeatureParams(
                Map.of("is_dark_launch", "true"));

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Search.OsDefaultsChoice.DialogShownAttempt")
                        .expectIntRecord(
                                "Search.OsDefaultsChoice.DialogSuppressionStatus",
                                DialogSuppressionStatus.SUPPRESSED_DARK_LAUNCH)
                        .build();
        var shouldShowSupplier = new ObservableSupplierImpl<>(true);
        doReturn(shouldShowSupplier)
                .when(mSearchEngineChoiceService)
                .getIsDeviceChoiceRequiredSupplier();
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();

        assertFalse(ChoiceDialogCoordinator.maybeShowInternal(this::createCoordinatorWithMocks));

        shadowOf(Looper.getMainLooper()).idle();
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());
        verify(mSearchEngineChoiceService, never()).notifyDeviceChoiceBlockShown();
        verify(mSearchEngineChoiceService, never()).notifyDeviceChoiceBlockCleared();
        assertEquals(
                0,
                ChromeSharedPreferences.getInstance()
                        .readInt(SEARCH_ENGINE_CHOICE_PENDING_OS_CHOICE_DIALOG_SHOWN_ATTEMPTS));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testMaybeShow_doesNotShowEscapeHatch() {
        SearchEnginesFeaturesTestUtil.configureClayBlockingFeatureParams(
                Map.of("escape_hatch_block_limit", "1"));
        var shouldShowSupplier = new ObservableSupplierImpl<>(true);
        doReturn(shouldShowSupplier)
                .when(mSearchEngineChoiceService)
                .getIsDeviceChoiceRequiredSupplier();
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();

        // Initial run, the dialog is shown
        try (var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Search.OsDefaultsChoice.DialogShownAttempt", 1)
                        .expectIntRecord(
                                "Search.OsDefaultsChoice.DialogSuppressionStatus",
                                DialogSuppressionStatus.CAN_SHOW)
                        .build()) {
            assertTrue(ChoiceDialogCoordinator.maybeShowInternal(this::createCoordinatorWithMocks));

            shadowOf(Looper.getMainLooper()).idle();
            verify(mModalDialogManager)
                    .showDialog(any(), eq(ModalDialogType.APP), eq(ModalDialogPriority.VERY_HIGH));
            assertEquals(
                    1,
                    ChromeSharedPreferences.getInstance()
                            .readInt(SEARCH_ENGINE_CHOICE_PENDING_OS_CHOICE_DIALOG_SHOWN_ATTEMPTS));
        }

        // Second run, the dialog is suppressed
        reset(mModalDialogManager);
        try (var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Search.OsDefaultsChoice.DialogShownAttempt")
                        .expectIntRecord(
                                "Search.OsDefaultsChoice.DialogSuppressionStatus",
                                DialogSuppressionStatus.SUPPRESSED_ESCAPE_HATCH)
                        .build()) {
            assertFalse(
                    ChoiceDialogCoordinator.maybeShowInternal(this::createCoordinatorWithMocks));

            shadowOf(Looper.getMainLooper()).idle();
            verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());
            assertEquals(
                    1,
                    ChromeSharedPreferences.getInstance()
                            .readInt(SEARCH_ENGINE_CHOICE_PENDING_OS_CHOICE_DIALOG_SHOWN_ATTEMPTS));
        }

        // When the device becomes ineligible, the counter is reset.
        reset(mModalDialogManager);
        doReturn(false).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();
        try (var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Search.OsDefaultsChoice.DialogShownAttempt")
                        .expectNoRecords("Search.OsDefaultsChoice.DialogSuppressionStatus")
                        .build()) {
            assertFalse(
                    ChoiceDialogCoordinator.maybeShowInternal(this::createCoordinatorWithMocks));

            shadowOf(Looper.getMainLooper()).idle();
            verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());
            assertEquals(
                    0,
                    ChromeSharedPreferences.getInstance()
                            .readInt(SEARCH_ENGINE_CHOICE_PENDING_OS_CHOICE_DIALOG_SHOWN_ATTEMPTS));
        }
    }

    @Test
    public void testMaybeShow_showsAndBlocksAfterDelayedApproval() {
        SearchEnginesFeaturesTestUtil.configureClayBlockingFeatureParams(
                Map.of("silent_pending_duration_millis", "24"));

        var pendingSupplier = new ObservableSupplierImpl<Boolean>(null);
        doReturn(pendingSupplier)
                .when(mSearchEngineChoiceService)
                .getIsDeviceChoiceRequiredSupplier();
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();

        assertTrue(ChoiceDialogCoordinator.maybeShowInternal(this::createCoordinatorWithMocks));
        assertTrue(pendingSupplier.hasObservers()); // The dialog started observing.
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());

        shadowOf(Looper.getMainLooper()).runToEndOfTasks();
        verify(mModalDialogManager)
                .showDialog(any(), eq(ModalDialogType.APP), eq(ModalDialogPriority.VERY_HIGH));
        verify(mSearchEngineChoiceService).notifyDeviceChoiceBlockShown();
        verify(mViewHolder).updateViewForType(eq(DialogType.LOADING));

        pendingSupplier.set(true);
        shadowOf(Looper.getMainLooper()).runToEndOfTasks();

        verify(mViewHolder).updateViewForType(eq(DialogType.CHOICE_LAUNCH));
        verify(mSearchEngineChoiceService, never()).notifyDeviceChoiceBlockCleared();
    }

    @Test
    public void testMaybeShow_showsAndBlocksAfterDelayedApprovalWithoutPendingDialog() {
        var pendingSupplier = new ObservableSupplierImpl<Boolean>(null);
        doReturn(pendingSupplier)
                .when(mSearchEngineChoiceService)
                .getIsDeviceChoiceRequiredSupplier();
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();

        assertTrue(ChoiceDialogCoordinator.maybeShowInternal(this::createCoordinatorWithMocks));
        assertTrue(pendingSupplier.hasObservers()); // The dialog started observing.
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());

        shadowOf(Looper.getMainLooper()).runToEndOfTasks();
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());
        verify(mSearchEngineChoiceService, never()).notifyDeviceChoiceBlockShown();
        verify(mViewHolder, never()).updateViewForType(anyInt());

        pendingSupplier.set(true);
        shadowOf(Looper.getMainLooper()).runToEndOfTasks();

        verify(mModalDialogManager)
                .showDialog(any(), eq(ModalDialogType.APP), eq(ModalDialogPriority.VERY_HIGH));
        verify(mViewHolder).updateViewForType(eq(DialogType.CHOICE_LAUNCH));
        verify(mSearchEngineChoiceService).notifyDeviceChoiceBlockShown();
        verify(mSearchEngineChoiceService, never()).notifyDeviceChoiceBlockCleared();
    }

    @Test
    public void testMaybeShow_showsAndUnblocksAfterDelayedDisapproval() {
        SearchEnginesFeaturesTestUtil.configureClayBlockingFeatureParams(
                Map.of("silent_pending_duration_millis", "24"));

        var pendingSupplier = new ObservableSupplierImpl<Boolean>(null);
        doReturn(pendingSupplier)
                .when(mSearchEngineChoiceService)
                .getIsDeviceChoiceRequiredSupplier();
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();

        assertTrue(ChoiceDialogCoordinator.maybeShowInternal(this::createCoordinatorWithMocks));
        assertTrue(pendingSupplier.hasObservers()); // The dialog started observing.
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());

        shadowOf(Looper.getMainLooper()).runToEndOfTasks();
        verify(mModalDialogManager)
                .showDialog(any(), eq(ModalDialogType.APP), eq(ModalDialogPriority.VERY_HIGH));
        verify(mSearchEngineChoiceService).notifyDeviceChoiceBlockShown();
        verify(mViewHolder).updateViewForType(eq(DialogType.LOADING));

        pendingSupplier.set(false);
        shadowOf(Looper.getMainLooper()).runToEndOfTasks();

        verify(mSearchEngineChoiceService).notifyDeviceChoiceBlockCleared();
        verify(mViewHolder).updateViewForType(eq(DialogType.CHOICE_CONFIRM));
    }

    @Test
    public void testMaybeShow_unblocksAfterDelayedDisapprovalWithoutPendingDialog() {
        var pendingSupplier = new ObservableSupplierImpl<Boolean>(null);
        doReturn(pendingSupplier)
                .when(mSearchEngineChoiceService)
                .getIsDeviceChoiceRequiredSupplier();
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();

        assertTrue(ChoiceDialogCoordinator.maybeShowInternal(this::createCoordinatorWithMocks));
        assertTrue(pendingSupplier.hasObservers()); // The dialog started observing.
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());

        shadowOf(Looper.getMainLooper()).runToEndOfTasks();
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());
        verify(mSearchEngineChoiceService, never()).notifyDeviceChoiceBlockShown();
        verify(mViewHolder, never()).updateViewForType(eq(DialogType.LOADING));

        pendingSupplier.set(false);
        shadowOf(Looper.getMainLooper()).runToEndOfTasks();

        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());
        verify(mSearchEngineChoiceService, never()).notifyDeviceChoiceBlockShown();
        verify(mSearchEngineChoiceService, never()).notifyDeviceChoiceBlockCleared();
        verify(mViewHolder, never()).updateViewForType(anyInt());
        assertFalse(pendingSupplier.hasObservers());
    }

    @Test
    public void testMaybeShow_doesNotShowWithImmediateDisapproval() {
        var disapprovingSupplier = new ObservableSupplierImpl<>(false);
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();
        doReturn(disapprovingSupplier)
                .when(mSearchEngineChoiceService)
                .getIsDeviceChoiceRequiredSupplier();

        assertTrue(ChoiceDialogCoordinator.maybeShowInternal(this::createCoordinatorWithMocks));
        assertTrue(disapprovingSupplier.hasObservers()); // The dialog started observing.
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());

        shadowOf(Looper.getMainLooper()).idle();
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());
        verify(mSearchEngineChoiceService, never()).notifyDeviceChoiceBlockShown();
        verify(mSearchEngineChoiceService, never()).notifyDeviceChoiceBlockCleared();
        assertFalse(disapprovingSupplier.hasObservers()); // The dialog stopped observing.
    }

    @Test
    public void testMaybeShow_dismissesDialogAfterDisconnection() {
        var shouldShowSupplier = new ObservableSupplierImpl<>(true);
        doReturn(shouldShowSupplier)
                .when(mSearchEngineChoiceService)
                .getIsDeviceChoiceRequiredSupplier();
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();

        assertTrue(ChoiceDialogCoordinator.maybeShowInternal(this::createCoordinatorWithMocks));
        assertTrue(shouldShowSupplier.hasObservers()); // The dialog started observing.

        shadowOf(Looper.getMainLooper()).runToEndOfTasks();
        verify(mModalDialogManager)
                .showDialog(any(), eq(ModalDialogType.APP), eq(ModalDialogPriority.VERY_HIGH));
        verify(mSearchEngineChoiceService).notifyDeviceChoiceBlockShown();
        verify(mViewHolder).updateViewForType(eq(DialogType.CHOICE_LAUNCH));

        shouldShowSupplier.set(null);

        shadowOf(Looper.getMainLooper()).idle();
        verify(mModalDialogManager).dismissDialog(any(), eq(DialogDismissalCause.UNKNOWN));
        verify(mSearchEngineChoiceService, never()).notifyDeviceChoiceBlockCleared();
        assertFalse(shouldShowSupplier.hasObservers()); // The dialog stopped observing.
    }

    @Test
    public void testMaybeShow_dismissesDialogAfterTimeout() {
        int timeoutDuration = 24_000;
        int silentPendingDuration = 1_000;
        SearchEnginesFeaturesTestUtil.configureClayBlockingFeatureParams(
                Map.ofEntries(
                        Map.entry("dialog_timeout_millis", Long.toString(timeoutDuration)),
                        Map.entry(
                                "silent_pending_duration_millis",
                                Long.toString(silentPendingDuration))));

        var pendingSupplier = new ObservableSupplierImpl<>();
        doReturn(pendingSupplier)
                .when(mSearchEngineChoiceService)
                .getIsDeviceChoiceRequiredSupplier();
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();

        assertTrue(ChoiceDialogCoordinator.maybeShowInternal(this::createCoordinatorWithMocks));
        assertTrue(pendingSupplier.hasObservers()); // The dialog started observing.

        // Verify that we don't show the dialog until silentPendingDuration is reached
        shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(silentPendingDuration - 1));
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());

        shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(1));
        verify(mModalDialogManager)
                .showDialog(any(), eq(ModalDialogType.APP), eq(ModalDialogPriority.VERY_HIGH));
        verify(mSearchEngineChoiceService).notifyDeviceChoiceBlockShown();
        verify(mViewHolder).updateViewForType(eq(DialogType.LOADING));

        // Verify that we don't get misc updates before the timeout duration is reached
        reset(mModalDialogManager, mViewHolder);
        shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(timeoutDuration / 2));
        verifyNoMoreInteractions(mModalDialogManager, mViewHolder);

        shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(timeoutDuration));
        verify(mModalDialogManager).dismissDialog(any(), eq(DialogDismissalCause.UNKNOWN));
        verify(mSearchEngineChoiceService, never()).notifyDeviceChoiceBlockCleared();
        assertFalse(pendingSupplier.hasObservers()); // The dialog stopped observing.
    }

    @Test
    public void testLaunchChoiceScreen_isDebounced() {
        doReturn(new ObservableSupplierImpl<>(true))
                .when(mSearchEngineChoiceService)
                .getIsDeviceChoiceRequiredSupplier();
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();

        assertTrue(ChoiceDialogCoordinator.maybeShowInternal(this::createCoordinatorWithMocks));
        shadowOf(Looper.getMainLooper()).idle();
        verify(mModalDialogManager)
                .showDialog(
                        mModelCaptor.capture(),
                        eq(ModalDialogType.APP),
                        eq(ModalDialogPriority.VERY_HIGH));
        verify(mLifecycleDispatcher).register(mLifecycleObserverCaptor.capture());
        verify(mViewHolder).updateViewForType(eq(DialogType.CHOICE_LAUNCH));

        PropertyModel model = mModelCaptor.getValue();
        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);

        try (var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Search.OsDefaultsChoice.LaunchChoiceScreenTapHandlingStatus",
                        LaunchChoiceScreenTapHandlingStatus.INITIAL_TAP)) {
            controller.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);

            // First call to trigger the choice screen
            verify(mSearchEngineChoiceService, times(1)).launchDeviceChoiceScreens();
        }

        try (var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Search.OsDefaultsChoice.LaunchChoiceScreenTapHandlingStatus",
                        LaunchChoiceScreenTapHandlingStatus.SUPPRESSED_TAP)) {
            controller.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);

            // No additional call
            verify(mSearchEngineChoiceService, times(1)).launchDeviceChoiceScreens();
        }

        mFakeTimeRule.advanceMillis(ChoiceDialogMediator.DEBOUNCE_TIME_MILLIS + 1);

        try (var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Search.OsDefaultsChoice.LaunchChoiceScreenTapHandlingStatus",
                        LaunchChoiceScreenTapHandlingStatus.REPEATED_TAP)) {
            controller.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);

            // Second call
            verify(mSearchEngineChoiceService, times(2)).launchDeviceChoiceScreens();
        }

        try (var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Search.OsDefaultsChoice.LaunchChoiceScreenTapHandlingStatus",
                        LaunchChoiceScreenTapHandlingStatus.SUPPRESSED_TAP)) {
            controller.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);

            // No additional call
            verify(mSearchEngineChoiceService, times(2)).launchDeviceChoiceScreens();
        }
    }

    @Test
    public void testLaunchChoiceScreen_pauseResumeRearmsDelay() {
        doReturn(new ObservableSupplierImpl<>(true))
                .when(mSearchEngineChoiceService)
                .getIsDeviceChoiceRequiredSupplier();
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();

        assertTrue(ChoiceDialogCoordinator.maybeShowInternal(this::createCoordinatorWithMocks));
        shadowOf(Looper.getMainLooper()).idle();
        verify(mModalDialogManager)
                .showDialog(
                        mModelCaptor.capture(),
                        eq(ModalDialogType.APP),
                        eq(ModalDialogPriority.VERY_HIGH));
        verify(mLifecycleDispatcher).register(mLifecycleObserverCaptor.capture());
        verify(mViewHolder).updateViewForType(eq(DialogType.CHOICE_LAUNCH));

        PropertyModel model = mModelCaptor.getValue();
        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);

        try (var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Search.OsDefaultsChoice.LaunchChoiceScreenTapHandlingStatus",
                        LaunchChoiceScreenTapHandlingStatus.INITIAL_TAP)) {
            controller.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);

            // Call goes through
            verify(mSearchEngineChoiceService, times(1)).launchDeviceChoiceScreens();
        }

        // Simulate the app being paused, which should trigger a recording of the delay.
        try (var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Search.OsDefaultsChoice.LaunchChoiceScreenDelay")) {
            mLifecycleObserverCaptor.getValue().onPauseWithNative();
        }

        // Clicking after this should lead to the action being processed normally instead of
        // debounced.
        mLifecycleObserverCaptor.getValue().onResumeWithNative();
        try (var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Search.OsDefaultsChoice.LaunchChoiceScreenTapHandlingStatus",
                        LaunchChoiceScreenTapHandlingStatus.INITIAL_TAP)) {
            controller.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);

            // Calling is possible again.
            verify(mSearchEngineChoiceService, times(2)).launchDeviceChoiceScreens();
        }
    }

    private ChoiceDialogCoordinator createCoordinatorWithMocks(
            SearchEngineChoiceService searchEngineChoiceService) {
        return new ChoiceDialogCoordinator(
                mContext,
                mViewHolder,
                mModalDialogManager,
                mLifecycleDispatcher,
                searchEngineChoiceService);
    }

    /**
     * Sets up the {@link #mModalDialogManager} to notify the first observer that gets registered on
     * it of {@link ModalDialogManagerObserver#onDialogAdded} and {@link
     * ModalDialogManagerObserver#onDialogDismissed} events.
     */
    private void setUpDialogObserverCapture() {
        final ModalDialogManagerObserver[] capturedDialogObserverHolder = {null};

        lenient()
                .doAnswer(
                        invocationOnMock -> {
                            capturedDialogObserverHolder[0] =
                                    invocationOnMock.getArgument(
                                            0, ModalDialogManagerObserver.class);
                            return null;
                        })
                .when(mModalDialogManager)
                .addObserver(any());

        lenient()
                .doAnswer(
                        invocationOnMock -> {
                            if (capturedDialogObserverHolder[0]
                                    != invocationOnMock.getArgument(0)) {
                                capturedDialogObserverHolder[0] = null;
                            }
                            return null;
                        })
                .when(mModalDialogManager)
                .removeObserver(any());

        lenient()
                .doAnswer(
                        invocationOnMock -> {
                            if (capturedDialogObserverHolder[0] != null) {
                                capturedDialogObserverHolder[0].onDialogAdded(
                                        invocationOnMock.getArgument(0, PropertyModel.class));
                            }
                            return null;
                        })
                .when(mModalDialogManager)
                .showDialog(any(), anyInt(), anyInt());

        lenient()
                .doAnswer(
                        invocationOnMock -> {
                            if (capturedDialogObserverHolder[0] != null) {
                                capturedDialogObserverHolder[0].onDialogDismissed(
                                        invocationOnMock.getArgument(0, PropertyModel.class));
                            }
                            return null;
                        })
                .when(mModalDialogManager)
                .dismissDialog(any(), anyInt());
    }
}
