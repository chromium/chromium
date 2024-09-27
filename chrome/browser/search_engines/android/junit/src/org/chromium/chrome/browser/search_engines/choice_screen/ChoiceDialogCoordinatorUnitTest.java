// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.robolectric.Shadows.shadowOf;

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

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.search_engines.choice_screen.ChoiceDialogMediator.DialogType;
import org.chromium.components.search_engines.SearchEngineChoiceService;
import org.chromium.components.search_engines.SearchEnginesFeatures;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogPriority;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modelutil.PropertyModel;

import java.time.Duration;

@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({SearchEnginesFeatures.CLAY_BLOCKING})
public class ChoiceDialogCoordinatorUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private @Mock ChoiceDialogCoordinator.ViewHolder mViewHolder;
    private @Mock ModalDialogManager mModalDialogManager;
    private @Mock ActivityLifecycleDispatcher mLifecycleDispatcher;
    private @Mock SearchEngineChoiceService mSearchEngineChoiceService;
    private @Captor ArgumentCaptor<Callback<Integer>> mActionButtonCallbackCaptor;

    @Before
    public void setUp() {
        SearchEngineChoiceService.setInstanceForTests(mSearchEngineChoiceService);
        setUpDialogObserverCapture();
    }

    @Test
    public void testMaybeShow() {
        var shouldShowSupplier = new ObservableSupplierImpl<>(true);
        doReturn(shouldShowSupplier)
                .when(mSearchEngineChoiceService)
                .getIsDeviceChoiceRequiredSupplier();
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();

        assertTrue(ChoiceDialogCoordinator.maybeShowInternal(this::createCoordinatorWithMocks));
        assertTrue(shouldShowSupplier.hasObservers()); // The dialog started observing.

        shadowOf(Looper.getMainLooper()).idle();
        verify(mModalDialogManager)
                .showDialog(any(), eq(ModalDialogType.APP), eq(ModalDialogPriority.VERY_HIGH));
        verify(mSearchEngineChoiceService).notifyDeviceChoiceBlockShown();
        verify(mViewHolder).updateViewForType(eq(DialogType.CHOICE_LAUNCH), any());

        shouldShowSupplier.set(false);

        shadowOf(Looper.getMainLooper()).idle();
        verify(mSearchEngineChoiceService).notifyDeviceChoiceBlockCleared();
        verify(mViewHolder)
                .updateViewForType(
                        eq(DialogType.CHOICE_CONFIRM), mActionButtonCallbackCaptor.capture());

        mActionButtonCallbackCaptor.getValue().onResult(DialogType.CHOICE_CONFIRM);

        verify(mModalDialogManager).dismissDialog(any(), eq(DialogDismissalCause.UNKNOWN));
        assertFalse(shouldShowSupplier.hasObservers()); // The dialog stopped observing.
    }

    @Test
    public void testMaybeShow_doesNotShowWhenNotEligible() {
        doReturn(false).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();

        assertFalse(ChoiceDialogCoordinator.maybeShowInternal(this::createCoordinatorWithMocks));

        shadowOf(Looper.getMainLooper()).idle();
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());
        verify(mSearchEngineChoiceService, never()).notifyDeviceChoiceBlockShown();
        verify(mSearchEngineChoiceService, never()).notifyDeviceChoiceBlockCleared();
    }

    @Test
    public void testMaybeShow_doesNotShowInDarkLaunchMode() {
        var testFeatures = new FeatureList.TestValues();
        testFeatures.addFeatureFlagOverride(SearchEnginesFeatures.CLAY_BLOCKING, true);
        testFeatures.addFieldTrialParamOverride(
                SearchEnginesFeatures.CLAY_BLOCKING, "is_dark_launch", "true");
        FeatureList.setTestValues(testFeatures);
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
    }

    @Test
    public void testMaybeShow_showsAndBlocksAfterDelayedApproval() {
        var pendingSupplier = new ObservableSupplierImpl<Boolean>(null);
        doReturn(pendingSupplier)
                .when(mSearchEngineChoiceService)
                .getIsDeviceChoiceRequiredSupplier();
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();

        assertTrue(ChoiceDialogCoordinator.maybeShowInternal(this::createCoordinatorWithMocks));
        assertTrue(pendingSupplier.hasObservers()); // The dialog started observing.
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());

        shadowOf(Looper.getMainLooper()).idle();
        verify(mModalDialogManager)
                .showDialog(any(), eq(ModalDialogType.APP), eq(ModalDialogPriority.VERY_HIGH));
        verify(mSearchEngineChoiceService).notifyDeviceChoiceBlockShown();
        verify(mViewHolder).updateViewForType(eq(DialogType.LOADING), any());

        pendingSupplier.set(true);
        shadowOf(Looper.getMainLooper()).idle();

        verify(mViewHolder).updateViewForType(eq(DialogType.CHOICE_LAUNCH), any());
        verify(mSearchEngineChoiceService, never()).notifyDeviceChoiceBlockCleared();
    }

    @Test
    public void testMaybeShow_showsAndUnblocksAfterDelayedDisapproval() {
        var pendingSupplier = new ObservableSupplierImpl<Boolean>(null);
        doReturn(pendingSupplier)
                .when(mSearchEngineChoiceService)
                .getIsDeviceChoiceRequiredSupplier();
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();

        assertTrue(ChoiceDialogCoordinator.maybeShowInternal(this::createCoordinatorWithMocks));
        assertTrue(pendingSupplier.hasObservers()); // The dialog started observing.
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyInt());

        shadowOf(Looper.getMainLooper()).idle();
        verify(mModalDialogManager)
                .showDialog(any(), eq(ModalDialogType.APP), eq(ModalDialogPriority.VERY_HIGH));
        verify(mSearchEngineChoiceService).notifyDeviceChoiceBlockShown();
        verify(mViewHolder).updateViewForType(eq(DialogType.LOADING), any());

        pendingSupplier.set(false);

        shadowOf(Looper.getMainLooper()).idle();
        verify(mSearchEngineChoiceService).notifyDeviceChoiceBlockCleared();
        verify(mViewHolder).updateViewForType(eq(DialogType.CHOICE_CONFIRM), any());
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
        verify(mViewHolder).updateViewForType(eq(DialogType.CHOICE_LAUNCH), any());

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

        var testFeatures = new FeatureList.TestValues();
        testFeatures.addFeatureFlagOverride(SearchEnginesFeatures.CLAY_BLOCKING, true);
        testFeatures.addFieldTrialParamOverride(
                SearchEnginesFeatures.CLAY_BLOCKING,
                "dialog_timeout_millis",
                Long.toString(timeoutDuration));
        testFeatures.addFieldTrialParamOverride(
                SearchEnginesFeatures.CLAY_BLOCKING,
                "silent_pending_duration_millis",
                Long.toString(silentPendingDuration));
        FeatureList.setTestValues(testFeatures);

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
        verify(mViewHolder).updateViewForType(eq(DialogType.LOADING), any());

        // Verify that we don't get misc updates before the timeout duration is reached
        reset(mModalDialogManager, mViewHolder);
        shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(timeoutDuration / 2));
        verifyNoMoreInteractions(mModalDialogManager, mViewHolder);

        shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(timeoutDuration));
        verify(mModalDialogManager).dismissDialog(any(), eq(DialogDismissalCause.UNKNOWN));
        verify(mSearchEngineChoiceService, never()).notifyDeviceChoiceBlockCleared();
        assertFalse(pendingSupplier.hasObservers()); // The dialog stopped observing.
    }

    private ChoiceDialogCoordinator createCoordinatorWithMocks(
            SearchEngineChoiceService searchEngineChoiceService) {
        return new ChoiceDialogCoordinator(
                mViewHolder, mModalDialogManager, mLifecycleDispatcher, searchEngineChoiceService);
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
