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
import org.chromium.base.Holder;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.search_engines.choice_screen.ChoiceDialogCoordinator.DialogSuppressionStatus;
import org.chromium.chrome.browser.search_engines.choice_screen.ChoiceDialogMediator.DialogType;
import org.chromium.chrome.browser.search_engines.choice_screen.ChoiceDialogMediator.LaunchChoiceScreenTapHandlingStatus;
import org.chromium.components.search_engines.SearchEngineChoiceService;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogPriority;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.stream.IntStream;

@RunWith(BaseRobolectricTestRunner.class)
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
    @CommandLineFlags.Add({ChromeSwitches.NO_FIRST_RUN})
    public void testMaybeShow_doesNotShowWhenCommanLineFlagSet() {
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();

        assertFalse(ChoiceDialogCoordinator.maybeShowInternal(this::createCoordinatorWithMocks));
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
    public void testMaybeShow_doesNotShowEscapeHatch() {
        var shouldShowSupplier = new ObservableSupplierImpl<>(true);
        doReturn(shouldShowSupplier)
                .when(mSearchEngineChoiceService)
                .getIsDeviceChoiceRequiredSupplier();
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();

        // For the first 10 runs, the dialog is shown.
        try (var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Search.OsDefaultsChoice.DialogShownAttempt",
                                IntStream.rangeClosed(
                                                1, ChoiceDialogCoordinator.ESCAPE_HATCH_BLOCK_LIMIT)
                                        .toArray())
                        .expectIntRecordTimes(
                                "Search.OsDefaultsChoice.DialogSuppressionStatus",
                                DialogSuppressionStatus.CAN_SHOW,
                                ChoiceDialogCoordinator.ESCAPE_HATCH_BLOCK_LIMIT)
                        .build()) {
            for (int i = 1; i <= ChoiceDialogCoordinator.ESCAPE_HATCH_BLOCK_LIMIT; i++) {
                assertTrue(
                        ChoiceDialogCoordinator.maybeShowInternal(
                                this::createCoordinatorWithMocks));

                shadowOf(Looper.getMainLooper()).idle();
                verify(mModalDialogManager)
                        .showDialog(
                                any(), eq(ModalDialogType.APP), eq(ModalDialogPriority.VERY_HIGH));
                assertEquals(
                        i,
                        ChromeSharedPreferences.getInstance()
                                .readInt(
                                        SEARCH_ENGINE_CHOICE_PENDING_OS_CHOICE_DIALOG_SHOWN_ATTEMPTS));
                reset(mModalDialogManager);
            }
        }

        // On the next run, the dialog is suppressed
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
                    ChoiceDialogCoordinator.ESCAPE_HATCH_BLOCK_LIMIT,
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
        verify(mViewHolder, never()).updateViewForType(anyInt());

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
        final Holder<@Nullable ModalDialogManagerObserver> capturedDialogObserverHolder =
                new Holder<>(null);

        lenient()
                .doAnswer(
                        invocationOnMock -> {
                            capturedDialogObserverHolder.value =
                                    invocationOnMock.getArgument(
                                            0, ModalDialogManagerObserver.class);
                            return null;
                        })
                .when(mModalDialogManager)
                .addObserver(any());

        lenient()
                .doAnswer(
                        invocationOnMock -> {
                            if (capturedDialogObserverHolder.value
                                    != invocationOnMock.getArgument(0)) {
                                capturedDialogObserverHolder.value = null;
                            }
                            return null;
                        })
                .when(mModalDialogManager)
                .removeObserver(any());

        lenient()
                .doAnswer(
                        invocationOnMock -> {
                            if (capturedDialogObserverHolder.value != null) {
                                capturedDialogObserverHolder.value.onDialogAdded(
                                        invocationOnMock.getArgument(0, PropertyModel.class));
                            }
                            return null;
                        })
                .when(mModalDialogManager)
                .showDialog(any(), anyInt(), anyInt());

        lenient()
                .doAnswer(
                        invocationOnMock -> {
                            if (capturedDialogObserverHolder.value != null) {
                                capturedDialogObserverHolder.value.onDialogDismissed(
                                        invocationOnMock.getArgument(0, PropertyModel.class));
                            }
                            return null;
                        })
                .when(mModalDialogManager)
                .dismissDialog(any(), anyInt());
    }
}
