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
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
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
import org.chromium.chrome.browser.glic.ui.GlicModalCoordinator.ContainerMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
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
    @Mock private BottomSheetController mBottomSheetController;
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
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
    }

    @After
    public void tearDown() {
        BottomSheetControllerProvider.setInstanceForTesting(null);
        mNativePtr.release();
        mWindowAndroid.destroy();
    }

    private GlicModalCoordinator showCoordinator(int portraitHeightDp, boolean showCloseButton) {
        return showCoordinator(
                portraitHeightDp,
                showCloseButton,
                /* loadingView= */ null,
                /* enableAutoResize= */ false,
                /* backgroundColor= */ null,
                /* alwaysUseModalDialog= */ false);
    }

    private GlicModalCoordinator showCoordinator(
            int portraitHeightDp,
            boolean showCloseButton,
            @Nullable View loadingView,
            boolean enableAutoResize) {
        return showCoordinator(
                portraitHeightDp,
                showCloseButton,
                loadingView,
                enableAutoResize,
                /* backgroundColor= */ null,
                /* alwaysUseModalDialog= */ false);
    }

    private GlicModalCoordinator showCoordinator(
            int portraitHeightDp,
            boolean showCloseButton,
            @Nullable View loadingView,
            boolean enableAutoResize,
            @Nullable Integer backgroundColor,
            boolean alwaysUseModalDialog) {
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
                enableAutoResize,
                backgroundColor,
                alwaysUseModalDialog);
    }

    @Test
    @Config(qualifiers = "sw600dp-w1000dp-h1000dp")
    public void testShow_TabletWindow_UsesModalDialog() {
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);

        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);

        assertNotNull(coordinator);
        assertEquals(ContainerMode.MODAL_DIALOG, coordinator.getContainerModeForTesting());
        verify(mThinWebView).attachWebContents(eq(mWebContents), any(), any());

        PropertyModel model = coordinator.getPropertyModelForTesting();
        assertNotNull(model);
        assertFalse(model.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));
        assertNotNull(model.get(ModalDialogProperties.CUSTOM_VIEW));
        assertNotNull(coordinator.getCloseButtonForTesting());

        verify(mModalDialogManager).showDialog(model, ModalDialogType.APP);
        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }

    @Test
    @Config(qualifiers = "sw360dp-w360dp-h800dp")
    public void testShow_CompactWindow_UsesBottomSheet() {
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);

        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);

        assertNotNull(coordinator);
        assertEquals(ContainerMode.BOTTOM_SHEET, coordinator.getContainerModeForTesting());
        verify(mThinWebView).attachWebContents(eq(mWebContents), any(), any());

        BottomSheetContent sheetContent = coordinator.getBottomSheetContentForTesting();
        assertNotNull(sheetContent);
        assertEquals(HeightMode.DISABLED, sheetContent.getPeekHeight());
        assertEquals(HeightMode.DISABLED, (int) sheetContent.getHalfHeightRatio());
        assertEquals(HeightMode.WRAP_CONTENT, (int) sheetContent.getFullHeightRatio());
        assertTrue(sheetContent.showHandlebar());
        assertTrue(sheetContent.swipeToDismissEnabled());
        assertTrue(sheetContent.getSheetType().isModal());
        assertTrue(sheetContent.getSheetType().isUserInitiated());
        assertEquals(
                R.string.glic_bottom_sheet_hidden_a11y_label,
                sheetContent.getSheetClosedAccessibilityStringId());
        assertEquals(
                R.string.glic_bottom_sheet_hidden_a11y_label,
                sheetContent.getSheetHiddenAccessibilityStringId());
        assertNotNull(coordinator.getCloseButtonForTesting());

        verify(mBottomSheetController).requestShowContent(sheetContent, true);
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());
    }

    @Test
    @Config(qualifiers = "sw600dp-w400dp-h800dp")
    public void testShow_TabletSplitScreenCompactWindow_UsesBottomSheet() {
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);

        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);

        assertNotNull(coordinator);
        assertEquals(ContainerMode.BOTTOM_SHEET, coordinator.getContainerModeForTesting());
        assertNotNull(coordinator.getBottomSheetContentForTesting());
        verify(mBottomSheetController)
                .requestShowContent(coordinator.getBottomSheetContentForTesting(), true);
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());
    }

    @Test
    @Config(qualifiers = "sw360dp-w360dp-h800dp")
    public void testShow_CompactWindow_FallsBackToModalWhenNoBottomSheetController() {
        BottomSheetControllerProvider.setInstanceForTesting(null);

        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);

        assertNotNull(coordinator);
        assertEquals(ContainerMode.MODAL_DIALOG, coordinator.getContainerModeForTesting());
        assertNotNull(coordinator.getPropertyModelForTesting());
        verify(mModalDialogManager)
                .showDialog(coordinator.getPropertyModelForTesting(), ModalDialogType.APP);
    }

    @Test
    @Config(qualifiers = "sw360dp-w360dp-h800dp")
    public void testShow_CompactWindow_SuppressedBottomSheetFallsBackToModal() {
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(false);

        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);

        assertNotNull(coordinator);
        assertEquals(ContainerMode.MODAL_DIALOG, coordinator.getContainerModeForTesting());
        assertNull(coordinator.getBottomSheetContentForTesting());
        assertNotNull(coordinator.getPropertyModelForTesting());
        verify(mBottomSheetController).hideContent(any(), eq(false));
        verify(mModalDialogManager)
                .showDialog(coordinator.getPropertyModelForTesting(), ModalDialogType.APP);
    }

    @Test
    @Config(qualifiers = "sw360dp-w360dp-h800dp")
    public void testLiveResize_SwapsBetweenBottomSheetAndModalDialog() {
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);

        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);
        assertNotNull(coordinator);
        assertEquals(ContainerMode.BOTTOM_SHEET, coordinator.getContainerModeForTesting());
        BottomSheetContent initialSheetContent = coordinator.getBottomSheetContentForTesting();
        assertNotNull(initialSheetContent);
        verify(mBottomSheetController).requestShowContent(initialSheetContent, true);

        // Resize window to >=600dp (tablet/desktop window width).
        Configuration config = mActivity.getResources().getConfiguration();
        config.screenWidthDp = 1000;
        config.screenHeightDp = 1000;
        dispatchConfigurationChanged(config);

        assertEquals(ContainerMode.MODAL_DIALOG, coordinator.getContainerModeForTesting());
        verify(mBottomSheetController).hideContent(initialSheetContent, false);
        PropertyModel modalModel = coordinator.getPropertyModelForTesting();
        assertNotNull(modalModel);
        verify(mModalDialogManager).showDialog(modalModel, ModalDialogType.APP);
        verify(mThinWebView, never()).destroy();
        verify(mNativeMock, never()).onDismissed(any());

        // Even if the old modal/sheet fires a late destroy or dismiss callback, it must be ignored.
        initialSheetContent.destroy();
        verify(mThinWebView, never()).destroy();
        verify(mNativeMock, never()).onDismissed(any());

        // Resize window back to <600dp (compact window width).
        config.screenWidthDp = 360;
        config.screenHeightDp = 800;
        dispatchConfigurationChanged(config);
        // Simulate ModalDialogManager firing onDismiss synchronously for the old model.
        modalModel
                .get(ModalDialogProperties.CONTROLLER)
                .onDismiss(modalModel, DialogDismissalCause.DISMISSED_BY_NATIVE);

        assertEquals(ContainerMode.BOTTOM_SHEET, coordinator.getContainerModeForTesting());
        verify(mModalDialogManager)
                .dismissDialog(modalModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
        assertNotNull(coordinator.getBottomSheetContentForTesting());
        verify(mBottomSheetController, times(2)).requestShowContent(any(), eq(true));
        verify(mThinWebView, never()).destroy();
        verify(mNativeMock, never()).onDismissed(any());
    }

    @Test
    public void testShow_NullActivity_ReturnsNull() {
        mWindowAndroid.mActivityRef = new WeakReference<>(null);

        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);

        assertNull(coordinator);
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());
    }

    @Test
    public void testShow_NoControllers_ReturnsNull() {
        mWindowAndroid.mDialogManager = null;
        BottomSheetControllerProvider.setInstanceForTesting(null);

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
    @Config(qualifiers = "sw360dp-w360dp-h800dp")
    public void testOnDismiss_FromBottomSheet() {
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);

        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);
        assertNotNull(coordinator);
        BottomSheetContent sheetContent = coordinator.getBottomSheetContentForTesting();
        assertNotNull(sheetContent);

        sheetContent.destroy();

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
    @Config(qualifiers = "sw360dp-w360dp-h800dp")
    public void testOnNativeDestroyed_BottomSheet() {
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);

        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);
        assertNotNull(coordinator);
        BottomSheetContent sheetContent = coordinator.getBottomSheetContentForTesting();
        assertNotNull(sheetContent);

        coordinator.onNativeDestroyed();

        verify(mBottomSheetController)
                .hideContent(
                        eq(sheetContent), eq(false), eq(StateChangeReason.INTERACTION_COMPLETE));
        sheetContent.destroy();
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
    @Config(qualifiers = "sw360dp-w360dp-h800dp")
    public void testCloseButton_ClicksAndDismissesBottomSheet() {
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);

        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);
        assertNotNull(coordinator);
        BottomSheetContent sheetContent = coordinator.getBottomSheetContentForTesting();
        assertNotNull(sheetContent);

        View closeButton = coordinator.getCloseButtonForTesting();
        assertNotNull(closeButton);
        assertEquals(View.VISIBLE, closeButton.getVisibility());
        closeButton.performClick();

        verify(mBottomSheetController)
                .hideContent(
                        eq(sheetContent), eq(false), eq(StateChangeReason.INTERACTION_COMPLETE));
        verify(mNativeMock).onDismissed(mNativePtr);
        verify(mThinWebView).destroy();
    }

    @Test
    @Config(qualifiers = "sw600dp-w1000dp-h1000dp")
    public void testLiveResize_SwapTargetFails_FallsBackToCurrentMode() {
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);

        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);
        assertNotNull(coordinator);
        assertEquals(ContainerMode.MODAL_DIALOG, coordinator.getContainerModeForTesting());
        PropertyModel initialModel = coordinator.getPropertyModelForTesting();
        assertNotNull(initialModel);

        // Make bottom sheet presentation fail when resizing to compact window (<600dp).
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(false);
        Configuration config = mActivity.getResources().getConfiguration();
        config.screenWidthDp = 360;
        config.screenHeightDp = 800;
        dispatchConfigurationChanged(config);

        assertEquals(ContainerMode.MODAL_DIALOG, coordinator.getContainerModeForTesting());
        assertNull(coordinator.getBottomSheetContentForTesting());
        assertNotNull(coordinator.getPropertyModelForTesting());
        verify(mBottomSheetController).hideContent(any(), eq(false));
        verify(mModalDialogManager, times(2)).showDialog(any(), eq(ModalDialogType.APP));
        verify(mThinWebView, never()).destroy();
        verify(mNativeMock, never()).onDismissed(any());
    }

    @Test
    @Config(qualifiers = "sw600dp-w1000dp-h1000dp")
    public void testOnConfigurationChanged_SameMode_UpdatesHeightAndLayouts() {
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);

        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);
        assertNotNull(coordinator);
        PropertyModel model = coordinator.getPropertyModelForTesting();
        assertNotNull(model);
        assertEquals(dpToPx(PORTRAIT_HEIGHT_DP), model.get(ModalDialogProperties.MAX_HEIGHT));
        View card = measureCard(coordinator, dpToPx(1000), dpToPx(1000));
        assertEquals(dpToPx(PORTRAIT_WIDTH_DP), card.getMeasuredWidth());
        assertEquals(dpToPx(PORTRAIT_HEIGHT_DP), card.getMeasuredHeight());

        // 1st change: rotate to landscape while remaining >=600dp.
        Configuration config = mActivity.getResources().getConfiguration();
        config.orientation = Configuration.ORIENTATION_LANDSCAPE;
        config.screenWidthDp = 1000;
        config.screenHeightDp = 600;
        dispatchConfigurationChanged(config);

        assertEquals(ContainerMode.MODAL_DIALOG, coordinator.getContainerModeForTesting());
        assertEquals(dpToPx(LANDSCAPE_HEIGHT_DP), model.get(ModalDialogProperties.MAX_HEIGHT));
        measureCard(coordinator, dpToPx(1000), dpToPx(600));
        assertEquals(dpToPx(LANDSCAPE_WIDTH_DP), card.getMeasuredWidth());
        assertEquals(dpToPx(LANDSCAPE_HEIGHT_DP), card.getMeasuredHeight());

        // 2nd change: rotate back to portrait and verify the card adapts again.
        config.orientation = Configuration.ORIENTATION_PORTRAIT;
        config.screenWidthDp = 1000;
        config.screenHeightDp = 1000;
        dispatchConfigurationChanged(config);

        assertEquals(dpToPx(PORTRAIT_HEIGHT_DP), model.get(ModalDialogProperties.MAX_HEIGHT));
        measureCard(coordinator, dpToPx(1000), dpToPx(1000));
        assertEquals(dpToPx(PORTRAIT_WIDTH_DP), card.getMeasuredWidth());
        assertEquals(dpToPx(PORTRAIT_HEIGHT_DP), card.getMeasuredHeight());
    }

    @Test
    @Config(qualifiers = "sw360dp-w360dp-h800dp")
    public void testBottomSheetContent_BackPressDefaultsAndScrollOffset() {
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);

        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);
        assertNotNull(coordinator);
        BottomSheetContent sheetContent = coordinator.getBottomSheetContentForTesting();
        assertNotNull(sheetContent);

        assertFalse(sheetContent.getBackPressStateChangedSupplier().get());
        assertFalse(sheetContent.handleBackPress());
        assertEquals(0, sheetContent.getVerticalScrollOffset());
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
        dispatchConfigurationChanged(config);
        measureCard(coordinator, dpToPx(900), dpToPx(650));
        assertEquals(dpToPx(LANDSCAPE_WIDTH_DP), card.getMeasuredWidth());
        assertEquals(dpToPx(LANDSCAPE_HEIGHT_DP), card.getMeasuredHeight());
        assertEquals(dpToPx(LANDSCAPE_HEIGHT_DP), model.get(ModalDialogProperties.MAX_HEIGHT));

        // 2nd resize: narrow back to 650x900dp -> adapts back to portrait bounds.
        config.screenWidthDp = 650;
        config.screenHeightDp = 900;
        dispatchConfigurationChanged(config);
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
        dispatchConfigurationChanged(config);
        measureCard(coordinator, dpToPx(650), dpToPx(900));
        assertEquals(dpToPx(PORTRAIT_WIDTH_DP), card.getMeasuredWidth());
        assertEquals(dpToPx(PORTRAIT_HEIGHT_DP), card.getMeasuredHeight());
        assertEquals(dpToPx(PORTRAIT_HEIGHT_DP), model.get(ModalDialogProperties.MAX_HEIGHT));

        // 2nd resize: widen back to 900x500dp -> adapts back to landscape bounds.
        config.screenWidthDp = 900;
        config.screenHeightDp = 500;
        dispatchConfigurationChanged(config);
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
    @Config(qualifiers = "sw360dp-w360dp-h800dp")
    public void testShow_BottomSheet_WithLoadingView_RevealedOnShowContent() {
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);
        View loadingView = new View(mActivity);
        GlicModalCoordinator coordinator =
                showCoordinator(
                        PORTRAIT_HEIGHT_DP,
                        /* showCloseButton= */ true,
                        loadingView,
                        /* enableAutoResize= */ false);
        assertNotNull(coordinator);
        assertEquals(ContainerMode.BOTTOM_SHEET, coordinator.getContainerModeForTesting());
        assertFalse(coordinator.isRevealedForTesting());
        assertEquals(0f, mWebView.getAlpha(), 0.001f);

        BottomSheetContent sheetContent = coordinator.getBottomSheetContentForTesting();
        assertNotNull(sheetContent);
        FrameLayout sheetContainer = (FrameLayout) sheetContent.getContentView();
        assertEquals(sheetContainer, loadingView.getParent());

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
    @Config(qualifiers = "sw360dp-w360dp-h800dp")
    public void testAutoResize_BottomSheet_OnContentSizeChanged() {
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);
        GlicModalCoordinator coordinator =
                showCoordinator(
                        PORTRAIT_HEIGHT_DP,
                        /* showCloseButton= */ true,
                        /* loadingView= */ null,
                        /* enableAutoResize= */ true);
        assertNotNull(coordinator);
        assertEquals(ContainerMode.BOTTOM_SHEET, coordinator.getContainerModeForTesting());
        assertTrue(coordinator.isRevealedForTesting());

        coordinator.onContentSizeChanged(280);

        assertNotNull(mWebView.getLayoutParams());
        assertEquals(dpToPx(280), mWebView.getLayoutParams().height);
    }

    @Test
    @Config(qualifiers = "sw360dp-w360dp-h800dp")
    public void testAutoResize_PreservedAcrossContainerSwap() {
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);
        GlicModalCoordinator coordinator =
                showCoordinator(
                        PORTRAIT_HEIGHT_DP,
                        /* showCloseButton= */ true,
                        /* loadingView= */ null,
                        /* enableAutoResize= */ true);
        assertNotNull(coordinator);
        assertEquals(ContainerMode.BOTTOM_SHEET, coordinator.getContainerModeForTesting());

        // Set content height while in BottomSheet mode.
        coordinator.onContentSizeChanged(280);
        assertEquals(dpToPx(280), mWebView.getLayoutParams().height);

        // Resize to tablet (>=600dp) swapping to ModalDialog.
        Configuration config = mActivity.getResources().getConfiguration();
        config.screenWidthDp = 1000;
        config.screenHeightDp = 1000;
        dispatchConfigurationChanged(config);

        assertEquals(ContainerMode.MODAL_DIALOG, coordinator.getContainerModeForTesting());
        assertEquals(dpToPx(280), mWebView.getLayoutParams().height);

        // Resize back to compact (<600dp) swapping back to BottomSheet.
        config.screenWidthDp = 360;
        config.screenHeightDp = 800;
        dispatchConfigurationChanged(config);

        assertEquals(ContainerMode.BOTTOM_SHEET, coordinator.getContainerModeForTesting());
        assertEquals(dpToPx(280), mWebView.getLayoutParams().height);
    }

    @Test
    @Config(qualifiers = "sw360dp-w360dp-h800dp")
    public void testConfigurationChanged_UnattachedContainerAndIgnoredAfterDestroy() {
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);
        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);
        assertNotNull(coordinator);
        assertEquals(ContainerMode.BOTTOM_SHEET, coordinator.getContainerModeForTesting());

        // Deliver a standalone Configuration (>=600dp landscape window) via ComponentCallbacks
        // while the sheet view is never attached to a window and before Activity.getResources()
        // changes.
        Configuration widenedConfig =
                new Configuration(mActivity.getResources().getConfiguration());
        widenedConfig.screenWidthDp = 700;
        widenedConfig.screenHeightDp = 500;
        dispatchConfigurationChanged(widenedConfig);

        assertEquals(ContainerMode.MODAL_DIALOG, coordinator.getContainerModeForTesting());
        PropertyModel model = coordinator.getPropertyModelForTesting();
        assertNotNull(model);
        assertEquals(dpToPx(LANDSCAPE_HEIGHT_DP), model.get(ModalDialogProperties.MAX_HEIGHT));

        // After teardown, ComponentCallbacks is unregistered and subsequent config changes are
        // ignored.
        coordinator.onNativeDestroyed();
        clearInvocations(mModalDialogManager, mBottomSheetController);

        Configuration compactConfig = new Configuration(widenedConfig);
        compactConfig.screenWidthDp = 360;
        compactConfig.screenHeightDp = 800;
        dispatchConfigurationChanged(compactConfig);
        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());
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

    @Test
    @Config(qualifiers = "sw411dp-w411dp-h891dp")
    public void testBottomSheet_FillsFullWidthWiderThanPortraitMax() {
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);
        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);
        assertNotNull(coordinator);
        assertEquals(ContainerMode.BOTTOM_SHEET, coordinator.getContainerModeForTesting());

        BottomSheetContent sheetContent = coordinator.getBottomSheetContentForTesting();
        assertNotNull(sheetContent);
        View sheetView = sheetContent.getContentView();
        sheetView.measure(
                MeasureSpec.makeMeasureSpec(dpToPx(411), MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(dpToPx(800), MeasureSpec.AT_MOST));

        // Sheet container must span the full 411dp bottom sheet width rather than capping at
        // PORTRAIT_WIDTH_DP (380dp).
        assertEquals(dpToPx(411), sheetView.getMeasuredWidth());
        assertEquals(dpToPx(PORTRAIT_HEIGHT_DP), sheetView.getMeasuredHeight());
        verify(mNativeMock).onAvailableSizeChanged(eq(mNativePtr), eq(411), eq(PORTRAIT_HEIGHT_DP));
    }

    @Test
    @Config(qualifiers = "sw360dp-w360dp-h800dp")
    public void testBottomSheet_BackgroundColorDefault() {
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);

        GlicModalCoordinator coordinator = showCoordinator(PORTRAIT_HEIGHT_DP, true);
        assertNotNull(coordinator);
        BottomSheetContent sheet = coordinator.getBottomSheetContentForTesting();
        assertNotNull(sheet);
        assertEquals(
                SemanticColorUtils.getDefaultBgColor(mActivity),
                sheet.getSheetBackgroundColorOverride());
    }

    @Test
    @Config(qualifiers = "sw360dp-w360dp-h800dp")
    public void testBottomSheet_BackgroundColorCustomOverride() {
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);

        GlicModalCoordinator coordinator =
                showCoordinator(
                        PORTRAIT_HEIGHT_DP,
                        /* showCloseButton= */ true,
                        /* loadingView= */ null,
                        /* enableAutoResize= */ false,
                        Color.MAGENTA,
                        /* alwaysUseModalDialog= */ false);
        assertNotNull(coordinator);
        BottomSheetContent sheet = coordinator.getBottomSheetContentForTesting();
        assertNotNull(sheet);
        assertEquals(Color.MAGENTA, sheet.getSheetBackgroundColorOverride());
    }

    @Test
    @Config(qualifiers = "sw360dp-w360dp-h800dp")
    public void testShow_AlwaysUseModalDialog_ForcesModalOnCompactWindow() {
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);

        GlicModalCoordinator coordinator =
                showCoordinator(
                        PORTRAIT_HEIGHT_DP,
                        /* showCloseButton= */ true,
                        /* loadingView= */ null,
                        /* enableAutoResize= */ false,
                        /* backgroundColor= */ null,
                        /* alwaysUseModalDialog= */ true);
        assertNotNull(coordinator);
        assertEquals(ContainerMode.MODAL_DIALOG, coordinator.getContainerModeForTesting());
        assertNull(coordinator.getBottomSheetContentForTesting());
        assertNotNull(coordinator.getPropertyModelForTesting());
        verify(mModalDialogManager)
                .showDialog(coordinator.getPropertyModelForTesting(), ModalDialogType.APP);
        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }

    private void dispatchConfigurationChanged(Configuration config) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            mActivity.onConfigurationChanged(config);
        } else {
            RuntimeEnvironment.getApplication().onConfigurationChanged(config);
        }
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
