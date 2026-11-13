// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.Gravity;
import android.view.View;
import android.view.View.MeasureSpec;
import android.widget.FrameLayout;

import androidx.activity.ComponentDialog;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.jni_zero.JniRawPtr;
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
import org.mockito.quality.Strictness;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.glic.R;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;

/** Unit tests for {@link GlicModalCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicModalCoordinatorUnitTest {
    private static final long NATIVE_PTR = 12345L;
    private static final int PORTRAIT_WIDTH_DP = 380;
    private static final int PORTRAIT_HEIGHT_DP = 567;
    private static final int LANDSCAPE_WIDTH_DP = 600;
    private static final int LANDSCAPE_HEIGHT_DP = 310;

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
    @Mock private GlicModalCoordinator.Natives mNativeMock;
    @Captor private ArgumentCaptor<ModalDialogManagerObserver> mDialogObserverCaptor;

    private View mWebView;
    private TestActivity mActivity;
    private TestWindowAndroid mWindowAndroid;
    private JniRawPtr<GlicModalCoordinator.NativeGlicModalAndroid> mNativePtr;

    @Before
    public void setUp() {
        GlicModalCoordinatorJni.setInstanceForTesting(mNativeMock);
        ThinWebViewFactory.setInstanceForTesting(mThinWebView);
        mNativePtr = JniRawPtr.createForTesting(NATIVE_PTR);

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mActivity = activity;
                            mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
                        });

        mWindowAndroid = new TestWindowAndroid(mActivity);
        mWindowAndroid.mDialogManager = mModalDialogManager;

        mWebView = new View(mActivity);
        when(mThinWebView.getView()).thenReturn(mWebView);
        when(mWebContents.isDestroyed()).thenReturn(true);
    }

    @After
    public void tearDown() {
        mNativePtr.release();
        mWindowAndroid.destroy();
    }

    private GlicModalCoordinator showCoordinator(int portraitHeightDp, boolean showCloseButton) {
        return showCoordinator(
                portraitHeightDp,
                showCloseButton,
                /* loadingView= */ null,
                /* enableAutoResize= */ false);
    }

    private GlicModalCoordinator showCoordinator(
            int portraitHeightDp,
            boolean showCloseButton,
            @Nullable View loadingView,
            boolean enableAutoResize) {
        return GlicModalCoordinator.show(
                mNativePtr,
                mWindowAndroid,
                mWebContents,
                PORTRAIT_WIDTH_DP,
                portraitHeightDp,
                LANDSCAPE_WIDTH_DP,
                LANDSCAPE_HEIGHT_DP,
                showCloseButton,
                loadingView,
                enableAutoResize);
    }

    @Test
    @Config(qualifiers = "sw600dp-w1000dp-h1000dp")
    public void testShow_UsesModalDialog() {
        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);

        assertNotNull(coordinator);
        verify(mThinWebView).attachWebContents(eq(mWebContents), any(), any());

        PropertyModel model = coordinator.getPropertyModelForTesting();
        assertNotNull(model);
        assertFalse(model.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));
        assertNotNull(model.get(ModalDialogProperties.CUSTOM_VIEW));
        assertNotNull(coordinator.getCloseButtonForTesting());

        verify(mModalDialogManager).showDialog(model, ModalDialogType.APP);
    }

    @Test
    public void testShow_NullActivity_ReturnsNull() {
        mWindowAndroid.mActivityRef = new WeakReference<>(null);

        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);

        assertNull(coordinator);
        verify(mModalDialogManager, never()).showDialog(any(), any(Integer.class));
    }

    @Test
    public void testShow_NullModalDialogManager_ReturnsNull() {
        mWindowAndroid.mDialogManager = null;

        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);

        assertNull(coordinator);
    }

    @Test
    public void testShow_WithoutCloseButton() {
        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, false);
        assertNotNull(coordinator);
        assertNull(coordinator.getCloseButtonForTesting());
    }

    @Test
    public void testOnDismiss_FromModalDialogManager() {
        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);
        assertNotNull(coordinator);
        PropertyModel model = coordinator.getPropertyModelForTesting();
        assertNotNull(model);

        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);
        controller.onDismiss(model, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

        verify(mNativeMock).onDismissed(mNativePtr);
        verify(mThinWebView).destroy();
    }

    @Test
    public void testOnNativeDestroyed_ModalDialog() {
        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);
        assertNotNull(coordinator);
        PropertyModel model = coordinator.getPropertyModelForTesting();
        assertNotNull(model);

        coordinator.onNativeDestroyed();

        verify(mModalDialogManager)
                .dismissDialog(eq(model), eq(DialogDismissalCause.DISMISSED_BY_NATIVE));

        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);
        controller.onDismiss(model, DialogDismissalCause.DISMISSED_BY_NATIVE);

        verify(mThinWebView).destroy();
        verify(mNativeMock, never()).onDismissed(any());
    }

    @Test
    public void testCloseButton_ClicksAndDismissesModal() {
        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);
        assertNotNull(coordinator);
        PropertyModel model = coordinator.getPropertyModelForTesting();
        assertNotNull(model);

        View closeButton = coordinator.getCloseButtonForTesting();
        assertNotNull(closeButton);
        assertEquals(View.VISIBLE, closeButton.getVisibility());
        closeButton.performClick();

        verify(mModalDialogManager)
                .dismissDialog(eq(model), eq(DialogDismissalCause.DISMISSED_BY_NATIVE));
        model.get(ModalDialogProperties.CONTROLLER)
                .onDismiss(model, DialogDismissalCause.DISMISSED_BY_NATIVE);

        verify(mThinWebView).destroy();
    }

    @Test
    @Config(qualifiers = "sw600dp-w1000dp-h1000dp")
    public void testOnConfigurationChanged_UpdatesHeightAndLayouts() {
        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);
        assertNotNull(coordinator);
        PropertyModel model = coordinator.getPropertyModelForTesting();
        assertNotNull(model);
        assertEquals(dpToPx(PORTRAIT_HEIGHT_DP), model.get(ModalDialogProperties.MAX_HEIGHT));
        View card = measureCard(coordinator, dpToPx(1000), dpToPx(1000));
        assertEquals(dpToPx(PORTRAIT_WIDTH_DP), card.getMeasuredWidth());
        assertEquals(dpToPx(PORTRAIT_HEIGHT_DP), card.getMeasuredHeight());

        // 1st change: rotate to landscape.
        RuntimeEnvironment.setQualifiers("sw600dp-w1000dp-h600dp-land");
        card.dispatchConfigurationChanged(mActivity.getResources().getConfiguration());

        assertEquals(dpToPx(LANDSCAPE_HEIGHT_DP), model.get(ModalDialogProperties.MAX_HEIGHT));
        measureCard(coordinator, dpToPx(1000), dpToPx(600));
        assertEquals(dpToPx(LANDSCAPE_WIDTH_DP), card.getMeasuredWidth());
        assertEquals(dpToPx(LANDSCAPE_HEIGHT_DP), card.getMeasuredHeight());

        // 2nd change: rotate back to portrait and verify the card adapts again.
        RuntimeEnvironment.setQualifiers("sw600dp-w1000dp-h1000dp");
        card.dispatchConfigurationChanged(mActivity.getResources().getConfiguration());

        assertEquals(dpToPx(PORTRAIT_HEIGHT_DP), model.get(ModalDialogProperties.MAX_HEIGHT));
        measureCard(coordinator, dpToPx(1000), dpToPx(1000));
        assertEquals(dpToPx(PORTRAIT_WIDTH_DP), card.getMeasuredWidth());
        assertEquals(dpToPx(PORTRAIT_HEIGHT_DP), card.getMeasuredHeight());
    }

    @Test
    @Config(qualifiers = "sw600dp-w1000dp-h1000dp")
    public void testShow_Portrait_UsesPortraitSize() {
        GlicModalCoordinator coordinator = showCoordinator(710, true);
        assertNotNull(coordinator);

        View card = measureCard(coordinator, dpToPx(1000), dpToPx(1000));
        assertEquals(dpToPx(380), card.getMeasuredWidth());
        assertEquals(dpToPx(710), card.getMeasuredHeight());
        assertEquals(
                dpToPx(710),
                coordinator.getPropertyModelForTesting().get(ModalDialogProperties.MAX_HEIGHT));
        assertEquals(
                Gravity.CENTER_HORIZONTAL,
                ((FrameLayout.LayoutParams) card.getLayoutParams()).gravity);
    }

    @Test
    @Config(qualifiers = "sw600dp-w1000dp-h500dp-land")
    public void testShow_Landscape_UsesLandscapeSize() {
        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);
        assertNotNull(coordinator);

        View card = measureCard(coordinator, dpToPx(1000), dpToPx(500));
        assertEquals(dpToPx(600), card.getMeasuredWidth());
        assertEquals(dpToPx(310), card.getMeasuredHeight());
        assertEquals(
                dpToPx(310),
                coordinator.getPropertyModelForTesting().get(ModalDialogProperties.MAX_HEIGHT));
    }

    @Test
    public void testShow_TallNarrowWindowInLandscape_UsesPortraitSize() {
        Configuration config = mActivity.getResources().getConfiguration();
        config.orientation = Configuration.ORIENTATION_LANDSCAPE;
        config.screenWidthDp = 650;
        config.screenHeightDp = 900;

        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);
        assertNotNull(coordinator);
        PropertyModel model = coordinator.getPropertyModelForTesting();
        assertNotNull(model);

        View card = measureCard(coordinator, dpToPx(650), dpToPx(900));
        assertEquals(dpToPx(PORTRAIT_WIDTH_DP), card.getMeasuredWidth());
        assertEquals(dpToPx(PORTRAIT_HEIGHT_DP), card.getMeasuredHeight());
        assertEquals(dpToPx(PORTRAIT_HEIGHT_DP), model.get(ModalDialogProperties.MAX_HEIGHT));

        // 1st resize: widen the window to 900x650dp -> adapts to landscape bounds.
        config.screenWidthDp = 900;
        config.screenHeightDp = 650;
        card.dispatchConfigurationChanged(config);
        measureCard(coordinator, dpToPx(900), dpToPx(650));
        assertEquals(dpToPx(LANDSCAPE_WIDTH_DP), card.getMeasuredWidth());
        assertEquals(dpToPx(LANDSCAPE_HEIGHT_DP), card.getMeasuredHeight());
        assertEquals(dpToPx(LANDSCAPE_HEIGHT_DP), model.get(ModalDialogProperties.MAX_HEIGHT));

        // 2nd resize: narrow back to 650x900dp -> adapts back to portrait bounds.
        config.screenWidthDp = 650;
        config.screenHeightDp = 900;
        card.dispatchConfigurationChanged(config);
        measureCard(coordinator, dpToPx(650), dpToPx(900));
        assertEquals(dpToPx(PORTRAIT_WIDTH_DP), card.getMeasuredWidth());
        assertEquals(dpToPx(PORTRAIT_HEIGHT_DP), card.getMeasuredHeight());
        assertEquals(dpToPx(PORTRAIT_HEIGHT_DP), model.get(ModalDialogProperties.MAX_HEIGHT));
    }

    @Test
    public void testShow_WideShortWindowInPortrait_UsesLandscapeSize() {
        Configuration config = mActivity.getResources().getConfiguration();
        config.orientation = Configuration.ORIENTATION_PORTRAIT;
        config.screenWidthDp = 900;
        config.screenHeightDp = 500;

        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);
        assertNotNull(coordinator);
        PropertyModel model = coordinator.getPropertyModelForTesting();
        assertNotNull(model);

        View card = measureCard(coordinator, dpToPx(900), dpToPx(500));
        assertEquals(dpToPx(LANDSCAPE_WIDTH_DP), card.getMeasuredWidth());
        assertEquals(dpToPx(LANDSCAPE_HEIGHT_DP), card.getMeasuredHeight());
        assertEquals(dpToPx(LANDSCAPE_HEIGHT_DP), model.get(ModalDialogProperties.MAX_HEIGHT));

        // 1st resize: narrow to 650x900dp -> adapts to portrait bounds.
        config.screenWidthDp = 650;
        config.screenHeightDp = 900;
        card.dispatchConfigurationChanged(config);
        measureCard(coordinator, dpToPx(650), dpToPx(900));
        assertEquals(dpToPx(PORTRAIT_WIDTH_DP), card.getMeasuredWidth());
        assertEquals(dpToPx(PORTRAIT_HEIGHT_DP), card.getMeasuredHeight());
        assertEquals(dpToPx(PORTRAIT_HEIGHT_DP), model.get(ModalDialogProperties.MAX_HEIGHT));

        // 2nd resize: widen back to 900x500dp -> adapts back to landscape bounds.
        config.screenWidthDp = 900;
        config.screenHeightDp = 500;
        card.dispatchConfigurationChanged(config);
        measureCard(coordinator, dpToPx(900), dpToPx(500));
        assertEquals(dpToPx(LANDSCAPE_WIDTH_DP), card.getMeasuredWidth());
        assertEquals(dpToPx(LANDSCAPE_HEIGHT_DP), card.getMeasuredHeight());
        assertEquals(dpToPx(LANDSCAPE_HEIGHT_DP), model.get(ModalDialogProperties.MAX_HEIGHT));
    }

    @Test
    @Config(qualifiers = "sw600dp-w1000dp-h1000dp")
    public void testShow_CardCappedToAvailableSpace() {
        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);
        assertNotNull(coordinator);

        View card = measureCard(coordinator, dpToPx(300), dpToPx(400));
        assertEquals(dpToPx(300), card.getMeasuredWidth());
        assertEquals(dpToPx(400), card.getMeasuredHeight());
    }

    @Test
    public void testShow_ClearsWindowBackgroundAndRoundsCard() {
        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);
        assertNotNull(coordinator);
        PropertyModel model = coordinator.getPropertyModelForTesting();
        assertTrue(model.get(ModalDialogProperties.CUSTOM_VIEW).getClipToOutline());

        verify(mModalDialogManager).addObserver(mDialogObserverCaptor.capture());
        ComponentDialog dialog = new ComponentDialog(mActivity);
        mDialogObserverCaptor.getValue().onDialogCreated(model, dialog);
        assertEquals(
                Color.TRANSPARENT,
                ((ColorDrawable) dialog.getWindow().getDecorView().getBackground()).getColor());
    }

    @Test
    public void testShow_WithLoadingView_RevealedOnShowContent() {
        View loadingView = new View(mActivity);
        GlicModalCoordinator coordinator =
                showCoordinator(
                        PORTRAIT_HEIGHT_DP,
                        /* showCloseButton= */ true,
                        loadingView,
                        /* enableAutoResize= */ false);
        assertNotNull(coordinator);
        assertFalse(coordinator.isRevealedForTesting());
        assertEquals(0f, mWebView.getAlpha(), 0.001f);

        PropertyModel model = coordinator.getPropertyModelForTesting();
        assertNotNull(model);
        FrameLayout card = (FrameLayout) model.get(ModalDialogProperties.CUSTOM_VIEW);
        assertEquals(card, loadingView.getParent());

        coordinator.showContent();

        assertTrue(coordinator.isRevealedForTesting());
        assertEquals(1f, mWebView.getAlpha(), 0.001f);
        assertNull(loadingView.getParent());
    }

    @Test
    public void testAutoResize_OnContentSizeChanged_UpdatesWebViewHeight() {
        GlicModalCoordinator coordinator =
                showCoordinator(
                        PORTRAIT_HEIGHT_DP,
                        /* showCloseButton= */ true,
                        /* loadingView= */ null,
                        /* enableAutoResize= */ true);
        assertNotNull(coordinator);
        assertTrue(coordinator.isRevealedForTesting());

        coordinator.onContentSizeChanged(250);

        assertNotNull(mWebView.getLayoutParams());
        assertEquals(dpToPx(250), mWebView.getLayoutParams().height);

        // Grow back up and verify height updates rather than sticking at the smaller size.
        coordinator.onContentSizeChanged(480);
        assertEquals(dpToPx(480), mWebView.getLayoutParams().height);
    }

    @Test
    public void testAutoResize_RatchetDownPreventionOnExactlyMeasurePass() {
        GlicModalCoordinator coordinator =
                showCoordinator(
                        PORTRAIT_HEIGHT_DP,
                        /* showCloseButton= */ true,
                        /* loadingView= */ null,
                        /* enableAutoResize= */ true);
        assertNotNull(coordinator);

        // Initial AT_MOST measure pass (e.g. 800px offered height).
        measureCard(coordinator, dpToPx(1000), dpToPx(800));
        verify(mNativeMock)
                .onAvailableSizeChanged(eq(mNativePtr), anyInt(), eq(PORTRAIT_HEIGHT_DP));

        clearInvocations(mNativeMock);

        // WebUI shrinks to 350dp.
        coordinator.onContentSizeChanged(350);

        // Subsequent EXACTLY measure pass with shrunk height (e.g. 350dp) should NOT ratchet down
        // available height.
        View card = coordinator.getPropertyModelForTesting().get(ModalDialogProperties.CUSTOM_VIEW);
        card.measure(
                MeasureSpec.makeMeasureSpec(dpToPx(PORTRAIT_WIDTH_DP), MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(dpToPx(350), MeasureSpec.EXACTLY));

        assertEquals(dpToPx(350), card.getMeasuredHeight());
        verify(mNativeMock, never()).onAvailableSizeChanged(eq(mNativePtr), anyInt(), eq(350));

        // When WebUI grows again (e.g. to 500dp), the card must expand back up rather than
        // sticking at the shrunk 350dp height.
        coordinator.onContentSizeChanged(500);
        card.measure(
                MeasureSpec.makeMeasureSpec(dpToPx(PORTRAIT_WIDTH_DP), MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(dpToPx(350), MeasureSpec.EXACTLY));
        assertEquals(dpToPx(500), mWebView.getLayoutParams().height);
        assertEquals(dpToPx(500), card.getMeasuredHeight());
    }

    private View measureCard(
            GlicModalCoordinator coordinator, int availableWidth, int availableHeight) {
        View card = coordinator.getPropertyModelForTesting().get(ModalDialogProperties.CUSTOM_VIEW);
        card.measure(
                MeasureSpec.makeMeasureSpec(availableWidth, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(availableHeight, MeasureSpec.AT_MOST));
        return card;
    }

    private int dpToPx(int dp) {
        return ViewUtils.dpToPx(mActivity, dp);
    }
}
