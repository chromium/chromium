// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.ViewStub;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.glic.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandlerRegistry;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link GlicUiCoordinator} actor external-trigger snackbar notifications. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicUiCoordinatorUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private SnackbarManager mSnackbarManager;

    private TestActivity mActivity;
    private ActivityWindowAndroid mWindowAndroid;
    private GlicUiCoordinator mGlicUiCoordinator;

    @Before
    public void setUp() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mActivity = activity;
                            mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
                        });
        mWindowAndroid =
                new ActivityWindowAndroid(
                        mActivity,
                        /* listenToActivityState= */ false,
                        IntentRequestTracker.createFromActivity(mActivity),
                        /* insetObserver= */ null,
                        /* occlusionTrackingAllowed= */ false);
    }

    @After
    public void tearDown() {
        if (mGlicUiCoordinator != null) {
            mGlicUiCoordinator.destroy();
            mGlicUiCoordinator = null;
        }
        mWindowAndroid.destroy();
    }

    private GlicUiCoordinator createGlicUiCoordinator() {
        TabModelSelector tabModelSelector = mock(TabModelSelector.class);
        when(tabModelSelector.getCurrentTabSupplier()).thenReturn(ObservableSuppliers.alwaysNull());
        return new GlicUiCoordinator(
                mActivity,
                mWindowAndroid,
                ObservableSuppliers.alwaysNull(),
                ObservableSuppliers.createNonNull(tabModelSelector),
                mock(BrowserControlsVisibilityManager.class),
                mock(TabObscuringHandler.class),
                mSnackbarManager,
                mock(BackPressHandlerRegistry.class),
                ObservableSuppliers.alwaysNull(),
                new ViewStub(mActivity),
                mock(ActivityLifecycleDispatcher.class),
                ObservableSuppliers.createNonNull(false),
                /* sideUiStateProvider= */ null);
    }

    @Test
    public void testOnPendingActorTaskTrigger_ShowsPreparingSnackbarWhenAttached() {
        when(mSnackbarManager.canShowSnackbar()).thenReturn(true);
        mGlicUiCoordinator = createGlicUiCoordinator();
        assertSame(mGlicUiCoordinator, GlicUiCoordinator.from(mWindowAndroid));

        GlicUiCoordinator.onPendingActorTaskTrigger(mWindowAndroid);

        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());
        assertEquals(
                mActivity.getString(R.string.actor_notification_title_preparing_to_start_task),
                snackbarCaptor.getValue().getTextForTesting());
    }

    @Test
    public void testOnPendingActorTaskTrigger_AcceptRefreshesAndKeepsPreparingSnackbar() {
        when(mSnackbarManager.canShowSnackbar()).thenReturn(true);
        mGlicUiCoordinator = createGlicUiCoordinator();

        GlicUiCoordinator.onPendingActorTaskTrigger(mWindowAndroid);
        verify(mSnackbarManager).showSnackbar(any());
        clearInvocations(mSnackbarManager);

        ShadowLooper.idleMainLooper(1, TimeUnit.MINUTES);
        GlicUiCoordinator.onPendingActorTaskTrigger(mWindowAndroid);

        ShadowLooper.idleMainLooper(1, TimeUnit.MINUTES);
        verify(mSnackbarManager, never()).dismissSnackbars(any());
        verify(mSnackbarManager, never()).showSnackbar(any());

        ShadowLooper.idleMainLooper(1, TimeUnit.MINUTES);
        verify(mSnackbarManager).dismissSnackbars(any());
        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    public void testOnPendingActorTaskFailed_ShowsErrorSnackbar() {
        when(mSnackbarManager.canShowSnackbar()).thenReturn(true);
        mGlicUiCoordinator = createGlicUiCoordinator();

        GlicUiCoordinator.onPendingActorTaskTrigger(mWindowAndroid);
        verify(mSnackbarManager).showSnackbar(any());
        clearInvocations(mSnackbarManager);

        GlicUiCoordinator.onPendingActorTaskFailed(mWindowAndroid);

        verify(mSnackbarManager).dismissSnackbars(any());
        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());
        assertEquals(
                mActivity.getString(R.string.actor_task_list_bubble_row_failed_task_subtitle),
                snackbarCaptor.getValue().getTextForTesting());
        assertEquals(
                mActivity.getString(R.string.actor_snackbar_back_to_gemini),
                snackbarCaptor.getValue().getActionText());
    }

    @Test
    public void testOnPendingActorTaskFailed_NoOpWithoutPriorTrigger() {
        when(mSnackbarManager.canShowSnackbar()).thenReturn(true);
        mGlicUiCoordinator = createGlicUiCoordinator();

        GlicUiCoordinator.onPendingActorTaskFailed(mWindowAndroid);

        verify(mSnackbarManager, never()).dismissSnackbars(any());
        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    public void testStaticNotifications_NoOpAfterGlicUiCoordinatorDestroyed() {
        when(mSnackbarManager.canShowSnackbar()).thenReturn(true);
        mGlicUiCoordinator = createGlicUiCoordinator();
        mGlicUiCoordinator.destroy();
        mGlicUiCoordinator = null;
        assertNull(GlicUiCoordinator.from(mWindowAndroid));

        GlicUiCoordinator.onPendingActorTaskTrigger(mWindowAndroid);
        GlicUiCoordinator.onPendingActorTaskFailed(mWindowAndroid);

        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    public void testStaticNotifications_NoOpWhenUnattached() {
        when(mSnackbarManager.canShowSnackbar()).thenReturn(true);

        GlicUiCoordinator.onPendingActorTaskTrigger(mWindowAndroid);
        GlicUiCoordinator.onPendingActorTaskFailed(mWindowAndroid);

        verify(mSnackbarManager, never()).showSnackbar(any());
    }
}
