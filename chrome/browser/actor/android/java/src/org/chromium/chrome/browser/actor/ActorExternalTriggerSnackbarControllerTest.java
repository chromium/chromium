// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.actor.ui.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

/** Unit tests for {@link ActorExternalTriggerSnackbarController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ActorExternalTriggerSnackbarControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private ActorKeyedService mActorKeyedService;
    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;

    private Activity mActivity;
    private SettableMonotonicObservableSupplier<Profile> mProfileSupplier;
    private ActorExternalTriggerSnackbarController mController;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mProfileSupplier = ObservableSuppliers.createMonotonic();
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mSnackbarManager.canShowSnackbar()).thenReturn(true);
        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);

        mProfileSupplier.set(mProfile);
        mController =
                new ActorExternalTriggerSnackbarController(
                        mActivity, mSnackbarManager, mProfileSupplier, mLifecycleDispatcher);
    }

    @Test
    public void testShowsSnackbarForPendingTrigger() {
        mController.onPendingActorTaskTrigger();

        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager, never()).dismissSnackbars(mController);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());
        Snackbar snackbar = snackbarCaptor.getValue();
        assertEquals(
                mActivity.getString(R.string.actor_notification_title_preparing_to_start_task),
                snackbar.getTextForTesting());
        assertEquals(Snackbar.UMA_ACTOR_EXTERNAL_TRIGGER, snackbar.getIdentifierForTesting());
        assertTrue(mController.isPendingTaskStartForTesting());
    }

    @Test
    public void testShowsOnStartWhenInitiallyCannotShowSnackbar() {
        when(mSnackbarManager.canShowSnackbar()).thenReturn(false);
        mController.onPendingActorTaskTrigger();
        verify(mSnackbarManager, never()).showSnackbar(any());
        assertTrue(mController.isPendingTaskStartForTesting());

        when(mSnackbarManager.canShowSnackbar()).thenReturn(true);
        mController.onStartWithNative();
        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    public void testDismissesWhenNewTaskStarts() {
        mController.onPendingActorTaskTrigger();
        clearInvocations(mSnackbarManager);

        mController.onTaskStateChanged(/* taskId= */ 42, ActorTaskState.CREATED);

        verify(mSnackbarManager).dismissSnackbars(mController);
        assertFalse(mController.isPendingTaskStartForTesting());
    }

    @Test
    public void testDismissesWhenSwitchedToIncognitoProfile() {
        mController.onPendingActorTaskTrigger();
        clearInvocations(mSnackbarManager);

        // Switching to Incognito should not detach the ActorKeyedService observer.
        mProfileSupplier.set(mIncognitoProfile);
        verify(mActorKeyedService, never()).removeObserver(mController);

        mController.onTaskStateChanged(/* taskId= */ 42, ActorTaskState.CREATED);
        verify(mSnackbarManager).dismissSnackbars(mController);
        assertFalse(mController.isPendingTaskStartForTesting());
    }

    @Test
    public void testIgnoresNonCreatedStateTransitionsAndDismissesOnCreated() {
        mController.onPendingActorTaskTrigger();
        clearInvocations(mSnackbarManager);

        // State changes on pre-existing tasks (which never re-enter CREATED) should not dismiss.
        mController.onTaskStateChanged(/* taskId= */ 10, ActorTaskState.REFLECTING);
        mController.onTaskStateChanged(/* taskId= */ 10, ActorTaskState.PAUSED_BY_USER);
        verify(mSnackbarManager, never()).dismissSnackbars(mController);
        assertTrue(mController.isPendingTaskStartForTesting());

        // New task 20 entering CREATED should dismiss the snackbar.
        mController.onTaskStateChanged(/* taskId= */ 20, ActorTaskState.CREATED);
        verify(mSnackbarManager).dismissSnackbars(mController);
        assertFalse(mController.isPendingTaskStartForTesting());
    }

    @Test
    public void testDismissClearsPendingState() {
        mController.onPendingActorTaskTrigger();
        clearInvocations(mSnackbarManager);

        mController.onDismissNoAction(null);
        assertFalse(mController.isPendingTaskStartForTesting());

        mController.onStartWithNative();
        verify(mSnackbarManager, never()).showSnackbar(any());
    }
}
