// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import static org.mockito.Mockito.when;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;

import android.app.Activity;
import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.DeviceInfo;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.ContentPriority;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.GlowSpec;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetType;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.TestBottomSheetContent;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager.ScrimClient;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;
import java.util.List;

/**
 * Render tests for {@link BottomSheet} desktop popup, handlebar, close button, trimmed bounds,
 * wrap-content, and standard glow visual treatments.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class BottomSheetRenderTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE)
                    .setRevision(0)
                    .build();

    @Mock private InsetObserver mInsetObserver;

    private SettableNonNullObservableSupplier<Integer> mEdgeToEdgeBottomInsetSupplier;
    private SettableNonNullObservableSupplier<Integer> mKeyboardInsetSupplier;
    private Activity mActivity;
    private ViewGroup mViewport;
    private ViewGroup mSheetContainer;
    private ManagedBottomSheetController mController;
    private BottomSheetTestSupport mTestSupport;

    private static class RenderSheetContent extends TestBottomSheetContent {
        private boolean mSupportsLargeFormFactor = true;
        private boolean mIsNonModal;
        private boolean mShowHandlebar;
        private @Nullable GlowSpec mGlowSpec;

        RenderSheetContent(Context context, View contentView) {
            super(context, ContentPriority.HIGH, /* hasCustomLifecycle= */ false, contentView);
            setPeekHeight(HeightMode.DISABLED);
            setHalfHeightRatio(0.5f);
            setSkipHalfStateScrollingDown(false);
        }

        void setSupportsLargeFormFactor(boolean supports) {
            mSupportsLargeFormFactor = supports;
        }

        void setNonModal(boolean nonModal) {
            mIsNonModal = nonModal;
            setHasCustomScrimLifecycle(nonModal);
        }

        void setShowHandlebar(boolean show) {
            mShowHandlebar = show;
        }

        void setGlowSpec(@Nullable GlowSpec spec) {
            mGlowSpec = spec;
        }

        @Override
        public @Nullable View getToolbarView() {
            return null;
        }

        @Override
        public boolean supportsLargeFormFactor() {
            return mSupportsLargeFormFactor;
        }

        @Override
        public boolean showHandlebar() {
            return mShowHandlebar;
        }

        @Override
        public BottomSheetType getSheetType() {
            return new BottomSheetType.Builder().setModal(!mIsNonModal).build();
        }

        @Override
        public @Nullable GlowSpec getSheetBackgroundGlowSpecOverride() {
            return mGlowSpec;
        }
    }

    public BottomSheetRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        DeviceInfo.setIsDesktopForTesting(true);
        BottomSheetTestSupport.setSmallScreen(false);
        mActivityTestRule.launchActivity(null);
        runOnUiThreadBlocking(
                () -> {
                    mEdgeToEdgeBottomInsetSupplier = ObservableSuppliers.createNonNull(0);
                    mKeyboardInsetSupplier = ObservableSuppliers.createNonNull(0);
                    when(mInsetObserver.getSupplierForKeyboardInset())
                            .thenReturn(mKeyboardInsetSupplier);
                    mActivity = mActivityTestRule.getActivity();
                });
    }

    @After
    public void tearDown() {
        if (mTestSupport != null) {
            runOnUiThreadBlocking(
                    () -> {
                        mTestSupport.forceDismissAllContent();
                        mTestSupport.endAllAnimations();
                    });
        }
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testDesktopPopup_Full_Modal() throws IOException {
        initController(/* enableLargeFormFactorUi= */ true);
        RenderSheetContent content = createCardContent("Modal Desktop Popup", 360);
        showSheet(content, SheetState.FULL);
        mRenderTestRule.render(mViewport, "desktop_popup_full_modal");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testDesktopPopup_Full_NonModal_CloseButton() throws IOException {
        initController(/* enableLargeFormFactorUi= */ true);
        RenderSheetContent content = createCardContent("Non-Modal With Close Button", 360);
        content.setNonModal(true);
        showSheet(content, SheetState.FULL);
        mRenderTestRule.render(mViewport, "desktop_popup_full_non_modal_close_button");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testDesktopPopup_Half_PartiallyOpen() throws IOException {
        initController(/* enableLargeFormFactorUi= */ true);
        RenderSheetContent content = createCardContent("Partially Open Popup", 420);
        showSheet(content, SheetState.HALF);
        mRenderTestRule.render(mViewport, "desktop_popup_half_trimmed_bounds");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testDesktopPopup_Full_Handlebar() throws IOException {
        initController(/* enableLargeFormFactorUi= */ true);
        RenderSheetContent content = createCardContent("Popup With Handlebar", 360);
        content.setShowHandlebar(true);
        showSheet(content, SheetState.FULL);
        mRenderTestRule.render(mViewport, "desktop_popup_full_handlebar");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testDesktopPopup_WrapContentCard() throws IOException {
        initController(/* enableLargeFormFactorUi= */ true);
        RenderSheetContent content = createCardContent("Compact Wrap-Content Card", 180);
        content.setFullHeightRatio(HeightMode.WRAP_CONTENT);
        content.setNonModal(true);
        showSheet(content, SheetState.FULL);
        mRenderTestRule.render(mViewport, "desktop_popup_wrap_content_card");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testDesktopFallback_Full_OptOutSheet() throws IOException {
        initController(/* enableLargeFormFactorUi= */ true);
        RenderSheetContent content = createCardContent("Desktop Fallback Sheet", 360);
        content.setSupportsLargeFormFactor(false);
        content.setNonModal(true);
        showSheet(content, SheetState.FULL);
        mRenderTestRule.render(mViewport, "desktop_fallback_full_opt_out_sheet");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testDesktopFallback_Half_OptOutSheet() throws IOException {
        initController(/* enableLargeFormFactorUi= */ true);
        RenderSheetContent content = createCardContent("Desktop Fallback Sheet", 420);
        content.setSupportsLargeFormFactor(false);
        content.setNonModal(true);
        showSheet(content, SheetState.HALF);
        mRenderTestRule.render(mViewport, "desktop_fallback_half_opt_out_sheet");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandard_GlowSpecLong() throws IOException {
        DeviceInfo.setIsDesktopForTesting(false);
        initController(/* enableLargeFormFactorUi= */ false);
        RenderSheetContent content = createCardContent("Standard Sheet With Long Glow", 320);
        content.setFullHeightRatio(HeightMode.WRAP_CONTENT);
        int glowColor = runOnUiThreadBlocking(() -> SemanticColorUtils.getColorPrimary(mActivity));
        content.setGlowSpec(new GlowSpec(glowColor, GlowSpec.ShadowSize.LONG));
        showSheet(content, SheetState.FULL);
        mRenderTestRule.render(mViewport, "standard_sheet_glow_spec_long");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandard_EdgeToEdgeAndKeyboardInsets() throws IOException {
        DeviceInfo.setIsDesktopForTesting(false);
        initController(/* enableLargeFormFactorUi= */ false);
        runOnUiThreadBlocking(
                () -> {
                    mEdgeToEdgeBottomInsetSupplier.set(48);
                    mKeyboardInsetSupplier.set(0);
                });
        RenderSheetContent content = createCardContent("Edge-To-Edge Bottom Inset Sheet", 240);
        content.setFullHeightRatio(HeightMode.RESIZE_CONTENT);
        showSheet(content, SheetState.HALF);
        mRenderTestRule.render(mViewport, "standard_sheet_edge_to_edge_inset_no_keyboard");

        runOnUiThreadBlocking(
                () -> {
                    mKeyboardInsetSupplier.set(200);
                    mTestSupport.setSheetState(SheetState.HALF, /* animate= */ false);
                    mTestSupport.endAllAnimations();
                });
        pollUiThread(() -> !mTestSupport.getSheetContainer().isLayoutRequested());
        mRenderTestRule.render(mViewport, "standard_sheet_edge_to_edge_inset_with_keyboard");
    }

    private void initController(boolean enableLargeFormFactorUi) {
        runOnUiThreadBlocking(
                () -> {
                    if (enableLargeFormFactorUi) {
                        mActivity
                                .getTheme()
                                .applyStyle(R.style.ThemeOverlay_BrowserUI_DesktopDensity, true);
                    }
                    ViewGroup activityContent = mActivity.findViewById(android.R.id.content);
                    activityContent.removeAllViews();

                    int viewportBgColor =
                            SemanticColorUtils.getColorSurfaceContainerHighest(mActivity);
                    mViewport = new FrameLayout(mActivity);
                    mViewport.setClipChildren(false);
                    mViewport.setBackgroundColor(viewportBgColor);
                    activityContent.addView(
                            mViewport,
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.MATCH_PARENT));

                    mSheetContainer = new FrameLayout(mActivity);
                    mViewport.addView(
                            mSheetContainer,
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.MATCH_PARENT));

                    ScrimManager scrimManager =
                            new ScrimManager(mActivity, mViewport, ScrimClient.NONE);
                    mController =
                            BottomSheetControllerFactory.createBottomSheetController(
                                    () -> scrimManager,
                                    mActivity.getWindow(),
                                    KeyboardVisibilityDelegate.getInstance(),
                                    () -> mSheetContainer,
                                    mEdgeToEdgeBottomInsetSupplier,
                                    /* desktopWindowStateManager= */ null,
                                    mInsetObserver,
                                    enableLargeFormFactorUi);
                    mTestSupport = new BottomSheetTestSupport(mController);
                });
    }

    private RenderSheetContent createCardContent(String label, int heightDp) {
        return runOnUiThreadBlocking(
                () -> {
                    float density = mActivity.getResources().getDisplayMetrics().density;
                    int heightPx = Math.round(heightDp * density);
                    TextView card = new TextView(mActivity);
                    card.setText(label);
                    card.setGravity(Gravity.CENTER);
                    card.setMinHeight(heightPx);
                    card.setLayoutParams(
                            new ViewGroup.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT, heightPx));
                    return new RenderSheetContent(mActivity, card);
                });
    }

    private void showSheet(BottomSheetContent content, @SheetState int targetState) {
        runOnUiThreadBlocking(
                () -> {
                    mController.requestShowContent(content, /* animate= */ false);
                    mTestSupport.setSheetState(targetState, /* animate= */ false);
                    mTestSupport.endAllAnimations();
                });
        pollUiThread(
                () ->
                        mController.getSheetState() == targetState
                                && !mTestSupport.getSheetContainer().isLayoutRequested());
    }
}
