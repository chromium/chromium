// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.activity.ComponentDialog;
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
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;

/** Unit tests for {@link GlicExperimentalOptInUiCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures(ChromeFeatureList.GLIC_EXPERIMENTAL_OPT_IN_DIALOG_NON_SCROLLABLE)
public class GlicExperimentalOptInUiCoordinatorUnitTest {
    private static final long NATIVE_PTR = 12345L;

    /**
     * A real {@link org.chromium.ui.base.WindowAndroid} backed by the test activity. A Mockito mock
     * cannot be used because {@code WindowAndroid#getIntentRequestTracker()} is final and therefore
     * cannot be stubbed.
     */
    private static class TestWindowAndroid extends ActivityWindowAndroid {
        WeakReference<Activity> mActivityRef;
        ModalDialogManager mDialogManager;

        TestWindowAndroid(Activity activity) {
            super(
                    activity,
                    /* listenToActivityState= */ false,
                    IntentRequestTracker.createFromActivity(activity),
                    /* insetObserver= */ null,
                    /* occlusionTrackingAllowed= */ false);
            mActivityRef = new WeakReference<>(activity);
        }

        @Override
        public WeakReference<Activity> getActivity() {
            return mActivityRef;
        }

        @Override
        public ModalDialogManager getModalDialogManager() {
            return mDialogManager;
        }
    }

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private WebContents mWebContents;
    @Mock private ThinWebView mThinWebView;
    @Mock private GlicExperimentalOptInUiCoordinator.Natives mNativeMock;

    private TestActivity mActivity;
    private TestWindowAndroid mWindowAndroid;

    @Before
    public void setUp() {
        GlicExperimentalOptInUiCoordinatorJni.setInstanceForTesting(mNativeMock);
        ThinWebViewFactory.setInstanceForTesting(mThinWebView);

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mActivity = activity;
                            mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
                        });

        mWindowAndroid = new TestWindowAndroid(mActivity);
        mWindowAndroid.mDialogManager = mModalDialogManager;

        when(mThinWebView.getView()).thenReturn(new View(mActivity));
        // ContentView talks to the WebContents once it takes focus, which a mock cannot service.
        when(mWebContents.isDestroyed()).thenReturn(true);
    }

    @After
    public void tearDown() {
        mWindowAndroid.destroy();
    }

    @Test
    public void testShow_Success() {
        GlicExperimentalOptInUiCoordinator coordinator =
                GlicExperimentalOptInUiCoordinator.show(NATIVE_PTR, mWindowAndroid, mWebContents);

        assertNotNull("Coordinator should not be null", coordinator);
        verify(mThinWebView).attachWebContents(eq(mWebContents), any(), any());

        PropertyModel model = coordinator.getPropertyModelForTesting();
        assertNotNull("PropertyModel should not be null", model);
        assertNotNull("Controller should not be null", model.get(ModalDialogProperties.CONTROLLER));
        assertEquals(false, model.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));
        assertNotNull(model.get(ModalDialogProperties.CUSTOM_VIEW));
        assertNotNull("Close button should be attached", coordinator.getCloseButtonForTesting());

        verify(mModalDialogManager).showDialog(model, ModalDialogManager.ModalDialogType.APP);
    }

    @Test
    public void testShow_NullActivity_ReturnsNull() {
        mWindowAndroid.mActivityRef = new WeakReference<>(null);

        GlicExperimentalOptInUiCoordinator coordinator =
                GlicExperimentalOptInUiCoordinator.show(NATIVE_PTR, mWindowAndroid, mWebContents);

        assertNull("Coordinator should be null when activity is null", coordinator);
        verify(mModalDialogManager, never()).showDialog(any(), any(Integer.class));
    }

    @Test
    public void testShow_NullModalDialogManager_ReturnsNull() {
        mWindowAndroid.mDialogManager = null;

        GlicExperimentalOptInUiCoordinator coordinator =
                GlicExperimentalOptInUiCoordinator.show(NATIVE_PTR, mWindowAndroid, mWebContents);

        assertNull("Coordinator should be null when ModalDialogManager is null", coordinator);
        verify(mModalDialogManager, never()).showDialog(any(), any(Integer.class));
    }

    @Test
    public void testOnDismiss_FromModalDialogManager() {
        GlicExperimentalOptInUiCoordinator coordinator =
                GlicExperimentalOptInUiCoordinator.show(NATIVE_PTR, mWindowAndroid, mWebContents);
        assertNotNull(coordinator);
        PropertyModel model = coordinator.getPropertyModelForTesting();
        assertNotNull(model);

        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);
        assertNotNull("Controller should not be null", controller);
        controller.onDismiss(model, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

        verify(mNativeMock).onDismissed(NATIVE_PTR);
        verify(mThinWebView).destroy();
    }

    @Test
    public void testOnNativeDestroyed() {
        GlicExperimentalOptInUiCoordinator coordinator =
                GlicExperimentalOptInUiCoordinator.show(NATIVE_PTR, mWindowAndroid, mWebContents);
        assertNotNull(coordinator);
        PropertyModel model = coordinator.getPropertyModelForTesting();
        assertNotNull(model);

        coordinator.onNativeDestroyed();

        verify(mModalDialogManager)
                .dismissDialog(eq(model), eq(DialogDismissalCause.DISMISSED_BY_NATIVE));

        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);
        assertNotNull("Controller should not be null", controller);
        controller.onDismiss(model, DialogDismissalCause.DISMISSED_BY_NATIVE);

        verify(mThinWebView).destroy();
        // Regression guard: native destruction must not call back into native.
        verify(mNativeMock, never()).onDismissed(any(Long.class));
    }

    @Test
    public void testCloseButton_ClicksAndDismissesDialog() {
        GlicExperimentalOptInUiCoordinator coordinator =
                GlicExperimentalOptInUiCoordinator.show(NATIVE_PTR, mWindowAndroid, mWebContents);
        assertNotNull(coordinator);
        PropertyModel model = coordinator.getPropertyModelForTesting();
        assertNotNull(model);

        View closeButton = coordinator.getCloseButtonForTesting();
        assertNotNull("Close button should be attached", closeButton);
        assertEquals(View.VISIBLE, closeButton.getVisibility());
        closeButton.performClick();

        verify(mModalDialogManager)
                .dismissDialog(eq(model), eq(DialogDismissalCause.DISMISSED_BY_NATIVE));

        // ModalDialogManager is mocked, so drive the dismissal callback manually to verify
        // that teardown runs.
        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);
        assertNotNull("Controller should not be null", controller);
        controller.onDismiss(model, DialogDismissalCause.DISMISSED_BY_NATIVE);

        verify(mThinWebView).destroy();
    }

    @Test
    @Config(qualifiers = "w1000dp-h1000dp")
    public void testShow_FlagDisabled_UsesDefaultSize() {
        GlicExperimentalOptInUiCoordinator coordinator =
                GlicExperimentalOptInUiCoordinator.show(NATIVE_PTR, mWindowAndroid, mWebContents);
        assertNotNull(coordinator);

        ViewGroup.LayoutParams params = getCardLayoutParams(coordinator);
        assertEquals(
                getDimensionPixelSize(R.dimen.glic_experimental_opt_in_dialog_max_width),
                params.width);
        assertEquals(
                getDimensionPixelSize(R.dimen.glic_experimental_opt_in_dialog_max_height),
                params.height);
        assertEquals(
                params.height,
                coordinator.getPropertyModelForTesting().get(ModalDialogProperties.MAX_HEIGHT));
        assertEquals(Gravity.CENTER_HORIZONTAL, ((FrameLayout.LayoutParams) params).gravity);
    }

    @Test
    @Config(qualifiers = "w1000dp-h1000dp")
    @EnableFeatures(ChromeFeatureList.GLIC_EXPERIMENTAL_OPT_IN_DIALOG_NON_SCROLLABLE)
    public void testShow_FlagEnabled_UsesNonScrollableHeight() {
        GlicExperimentalOptInUiCoordinator coordinator =
                GlicExperimentalOptInUiCoordinator.show(NATIVE_PTR, mWindowAndroid, mWebContents);
        assertNotNull(coordinator);

        ViewGroup.LayoutParams params = getCardLayoutParams(coordinator);
        assertEquals(
                getDimensionPixelSize(R.dimen.glic_experimental_opt_in_dialog_max_width),
                params.width);
        assertEquals(
                getDimensionPixelSize(
                        R.dimen.glic_experimental_opt_in_dialog_non_scrollable_max_height),
                params.height);
        assertEquals(
                params.height,
                coordinator.getPropertyModelForTesting().get(ModalDialogProperties.MAX_HEIGHT));
    }

    @Test
    public void testShow_ClearsWindowBackgroundAndRoundsCard() {
        GlicExperimentalOptInUiCoordinator coordinator =
                GlicExperimentalOptInUiCoordinator.show(NATIVE_PTR, mWindowAndroid, mWebContents);
        assertNotNull(coordinator);
        PropertyModel model = coordinator.getPropertyModelForTesting();
        assertTrue(model.get(ModalDialogProperties.CUSTOM_VIEW).getClipToOutline());

        ArgumentCaptor<ModalDialogManager.ModalDialogManagerObserver> observer =
                ArgumentCaptor.forClass(ModalDialogManager.ModalDialogManagerObserver.class);
        verify(mModalDialogManager).addObserver(observer.capture());
        ComponentDialog dialog = new ComponentDialog(mActivity);
        observer.getValue().onDialogCreated(model, dialog);
        assertEquals(
                Color.TRANSPARENT,
                ((ColorDrawable) dialog.getWindow().getDecorView().getBackground()).getColor());
    }

    private ViewGroup.LayoutParams getCardLayoutParams(
            GlicExperimentalOptInUiCoordinator coordinator) {
        PropertyModel model = coordinator.getPropertyModelForTesting();
        assertNotNull(model);
        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        assertNotNull(customView);
        return customView.getLayoutParams();
    }

    private int getDimensionPixelSize(int dimenRes) {
        return mActivity.getResources().getDimensionPixelSize(dimenRes);
    }
}
