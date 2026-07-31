// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.animation.Animator;
import android.app.Activity;
import android.content.res.Resources;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.ColorInt;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.CallbackUtils;
import org.chromium.base.MathUtils;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorTextButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTrailingButtonsCoordinator.StripLayoutTrailingButtonsObserver;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicButtonDelegate;
import org.chromium.chrome.browser.glic.GlicButtonStateController.ButtonState;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedServiceFactory;
import org.chromium.chrome.browser.glic.GlicPrefNames;
import org.chromium.chrome.browser.glic.GlicSplitButtonDelegate;
import org.chromium.chrome.browser.glic.GlicSplitButtonDelegateBridge;
import org.chromium.chrome.browser.glic.GlicSplitButtonDelegateBridgeJni;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskTracker;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiShowability;
import org.chromium.chrome.browser.ui.side_ui.SideUiObserver;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefChangeRegistrarJni;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.TestActivity;

import java.lang.ref.WeakReference;
import java.util.Collections;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.GLIC, ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL})
public class StripLayoutTrailingButtonsCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LayoutUpdateHost mUpdateHost;
    @Mock private LayoutRenderHost mRenderHost;
    @Mock private LayerTitleCache mLayerTitleCache;
    @Mock private GlicKeyedService mGlicKeyedService;
    @Mock private GlicButtonDelegate mGlicClickHandler;
    @Mock private View mToolbarContainerView;
    @Mock private ActivityWindowAndroid mWindowAndroid;
    @Mock private Profile mProfile;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefChangeRegistrar.Natives mPrefChangeRegistrarJniMock;
    @Mock private PrefService mPrefService;
    @Mock private StripLayoutTrailingButtonsObserver mObserver;
    @Mock private ChromeAndroidTaskTracker mTaskTracker;
    @Mock private ChromeAndroidTask mTask;
    @Mock private ActorKeyedService mActorKeyedService;
    @Mock private ActorTask mActorTask;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private GlicSplitButtonDelegateBridge.Natives mGlicSplitButtonDelegateBridgeJniMock;
    @Mock private SideUiStateProvider mSideUiStateProvider;

    @Captor private ArgumentCaptor<List<Animator>> mAnimatorsListCaptor;

    private final OneshotSupplierImpl<SideUiStateProvider> mSideUiStateProviderSupplier =
            new OneshotSupplierImpl<>();
    private final long mBwiPtr = 123L;

    private Activity mActivity;
    private StripLayoutTrailingButtonsCoordinator mCoordinator;
    private TintedCompositorButton mModelSelectorButton;
    private TintedCompositorTextButton mGlicButton;
    private TintedCompositorButton mGlicDismissButton;
    private TintedCompositorTextButton mGlicActorButton;
    private boolean mIsIncognito;
    private boolean mGlicIphShowing;

    @Before
    public void setUp() {
        GlicEnabling.setEnabledForTesting(ChromeFeatureList.isEnabled(ChromeFeatureList.GLIC));
        GlicSplitButtonDelegateBridgeJni.setInstanceForTesting(
                mGlicSplitButtonDelegateBridgeJniMock);
        CompositorAnimationHandler.setTestingMode(true);
        when(mUpdateHost.getAnimationHandler())
                .thenReturn(new CompositorAnimationHandler(CallbackUtils.emptyRunnable()));

        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP)).thenReturn(true);
        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarJniMock);
        when(mPrefChangeRegistrarJniMock.init(any(), any())).thenReturn(1L);

        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.emptyList());
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);

        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(new UnownedUserDataHost());
        when(mToolbarContainerView.getRootView()).thenReturn(mToolbarContainerView);
        when(mToolbarContainerView.getResources()).thenReturn(mActivity.getResources());
        when(mTaskTracker.get(anyInt())).thenReturn(mTask);
        when(mTask.getNativeBrowserWindowPtr(any(), any())).thenReturn(mBwiPtr);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        when(mSideUiStateProvider.canShowSideUi(SideUiId.SIDE_PANEL)).thenReturn(true);
        mSideUiStateProviderSupplier.set(mSideUiStateProvider);

        mCoordinator =
                new StripLayoutTrailingButtonsCoordinator(
                        mActivity,
                        mUpdateHost,
                        mRenderHost,
                        mWindowAndroid,
                        /* density= */ 1.0f,
                        mToolbarContainerView,
                        /* isAppInDesktopWindow= */ false,
                        /* isTopResumedActivity= */ false,
                        mTaskTracker,
                        mIsIncognito,
                        () -> mTabModelSelector,
                        mSideUiStateProviderSupplier,
                        () -> 100f,
                        () -> {},
                        (isFocused, view) -> {},
                        mGlicClickHandler,
                        (isFocused, view) -> {},
                        () -> mGlicIphShowing,
                        mObserver);
        ShadowLooper.idleMainLooper();
        mCoordinator.onProfileAvailable(mProfile);
        mCoordinator.setLayerTitleCache(mLayerTitleCache);
        mCoordinator.onSizeChanged(1000.f, 0.f, 0.f, 0.f);
        mGlicButton = mCoordinator.getGlicButton();
        if (mGlicButton != null) mGlicDismissButton = mGlicButton.getDismissButton();
        mGlicActorButton = mCoordinator.getGlicActorButton();
        mModelSelectorButton = mCoordinator.getModelSelectorButton();
    }

    @After
    public void tearDown() {
        CompositorAnimationHandler.setTestingMode(false);
        if (mCoordinator != null) {
            mCoordinator.destroy();
        }
    }

    // =========================================================================================
    // Model Selector Button (MSB) Unit Tests
    // =========================================================================================

    @Test
    public void testModelSelectorButtonDrawX() {
        // Set model selector button position.
        mCoordinator.setGlicButtonVisible(false);
        showModelSelectorButton();

        // Verify model selector button x-position.
        // width(1000) - endPadding(8) - width(32) = 960
        assertEquals(
                "Model selector button x-position is not as expected",
                960.f,
                mModelSelectorButton.getDrawX(),
                0.0);
    }

    @Test
    public void testModelSelectorButtonDrawX_Rtl() {
        // Set model selector button position.
        LocalizationUtils.setRtlForTesting(true);
        mCoordinator.setGlicButtonVisible(false);
        showModelSelectorButton();

        // Verify model selector button x-position.
        // leftPadding(0) + endPadding(8) = 8
        assertEquals(
                "Model selector button x-position is not as expected",
                8.f, // BUTTON_END_PADDING
                mModelSelectorButton.getDrawX(),
                0.0);
    }

    @Test
    public void testModelSelectorButtonDrawY() {
        // Set model selector button position.
        showModelSelectorButton();

        // Verify model selector button y-position.
        assertEquals(
                "Model selector button y-position is not as expected",
                3.f,
                mModelSelectorButton.getDrawY(),
                0.0);
    }

    @Test
    public void testModelSelectorButtonHoverHighlightProperties() {
        // Set model selector button position.
        showModelSelectorButton();

        // Verify model selector button background resource id.
        assertEquals(
                "Model selector button background resource id is not as expected",
                R.drawable.bg_circle_tab_strip_button,
                mModelSelectorButton.getBackgroundResourceId());

        TintedCompositorButton msb = mModelSelectorButton;

        // Verify model selector button hover highlight default tint.
        msb.setHovered(true);
        @ColorInt
        int hoverBackgroundDefaultColor =
                mActivity.getColor(R.color.tab_strip_button_bg_hover_tint);
        assertEquals(
                "Model selector button hover highlight default tint is not as expected",
                hoverBackgroundDefaultColor,
                msb.getBackgroundTint());

        // Verify model selector button hover highlight pressed tint.
        msb.setHovered(false);
        msb.setPressed(true, true);
        @ColorInt
        int hoverBackgroundPressedColor =
                mActivity.getColor(R.color.tab_strip_button_bg_peripheral_pressed_tint);
        assertEquals(
                "Model selector button hover highlight pressed tint is not as expected",
                hoverBackgroundPressedColor,
                msb.getBackgroundTint());

        // Verify incognito properties.
        mCoordinator.onTabModelSwitched(/* incognito= */ true);

        // Verify model selector button incognito hover highlight default tint.
        msb.setPressed(false);
        msb.setHovered(true);
        @ColorInt
        int hoverBackgroundDefaultIncognitoColor =
                mActivity.getColor(R.color.tab_strip_button_bg_incognito_hover_tint);
        assertEquals(
                "Model selector button incognito hover highlight default tint is not as expected",
                hoverBackgroundDefaultIncognitoColor,
                msb.getBackgroundTint());

        // Verify model selector button incognito hover highlight pressed tint.
        msb.setHovered(false);
        msb.setPressed(true, true);
        @ColorInt
        int hoverBackgroundPressedIncognitoColor =
                mActivity.getColor(R.color.tab_strip_button_bg_incognito_peripheral_pressed_tint);
        assertEquals(
                "Model selector button incognito hover highlight pressed tint is not as expected",
                hoverBackgroundPressedIncognitoColor,
                msb.getBackgroundTint());
    }

    @Test
    public void testModelSelectorButtonHoverEnter() {
        showModelSelectorButton();

        int x = (int) mModelSelectorButton.getDrawX();
        // Hover enters. Mouse position within MSB range(32dp width + 12dp click slop).
        mCoordinator.onHoverEvent(x + 1, 0);
        assertTrue("Model selector button should be hovered", mModelSelectorButton.isHovered());

        // Verify model selector button is NOT hovered when mouse is not on the button.
        // Mouse position out of MSB range(32dp width + 12dp click slop).
        mCoordinator.onHoverEvent(x + 45, 0);
        assertFalse(
                "Model selector button should NOT be hovered", mModelSelectorButton.isHovered());
    }

    @Test
    public void testModelSelectorButtonHoverOnDown() {
        showModelSelectorButton();

        // Verify model selector button is in pressed state when click is from mouse.
        mCoordinator.onDown(mModelSelectorButton.getDrawX() + 1, 0, 1);
        assertTrue(
                "Model selector button should be pressed from mouse",
                mModelSelectorButton.isPressedFromMouse());
    }

    // =========================================================================================
    // Glic Primary Button Unit Tests
    // =========================================================================================

    @Test
    public void testGlicButtonDrawX_Ltr() {
        assertNotNull("Glic button should be created.", mGlicButton);
        showGlicButton();

        // Verify Glic button x-position.
        // width(1000) - endSlop(6) - width(42) = 952
        assertEquals(
                "Glic button LTR x-position is not as expected",
                952.f,
                mGlicButton.getDrawX(),
                0.0);
    }

    @Test
    public void testGlicButtonDrawX_Rtl() {
        assertNotNull("Glic button should be created.", mGlicButton);
        LocalizationUtils.setRtlForTesting(true);
        showGlicButton();

        // Verify Glic button x-position.
        // leftPadding(0) + endSlop(6) = 6
        assertEquals(
                "Glic button RTL x-position is not as expected",
                6.f, // GLIC_BUTTON_END_SLOP
                mGlicButton.getDrawX(),
                0.0);
    }

    @Test
    public void testGlicButtonDrawY() {
        assertNotNull("Glic button should be created.", mGlicButton);
        showGlicButton();

        // Verify Glic button y-position.
        assertEquals("Glic button y-position is not as expected", 3.f, mGlicButton.getDrawY(), 0.0);
    }

    @Test
    public void testGlicButtonHoverHighlightProperties() {
        assertNotNull("Glic button should be created.", mGlicButton);
        assertEquals(
                "Glic button background resource id is not as expected",
                Resources.ID_NULL,
                mGlicButton.getBackgroundResourceId());

        // Standard hover default tint.
        mGlicButton.setHovered(true);
        @ColorInt
        int hoverDefaultColor = mActivity.getColor(R.color.tab_strip_glic_button_bg_hover_tint);
        assertEquals(
                "Glic button hover default tint is not as expected",
                hoverDefaultColor,
                mGlicButton.getBackgroundTint());

        // Standard hover pressed tint.
        mGlicButton.setHovered(false);
        mGlicButton.setPressed(true, true);
        @ColorInt
        int pressedColor = mActivity.getColor(R.color.tab_strip_glic_button_bg_pressed_tint);
        assertEquals(
                "Glic button hover pressed tint is not as expected",
                pressedColor,
                mGlicButton.getBackgroundTint());

        // Switch to incognito mode.
        mCoordinator.onTabModelSwitched(/* incognito= */ true);

        mGlicButton.setPressed(false);
        mGlicButton.setHovered(true);
        @ColorInt
        int incognitoHoverColor =
                mActivity
                        .getColorStateList(R.color.tab_strip_glic_button_bg_incognito_tint_list)
                        .getDefaultColor();
        assertEquals(
                "Glic button incognito hover default tint is not as expected",
                incognitoHoverColor,
                mGlicButton.getBackgroundTint());

        mGlicButton.setHovered(false);
        mGlicButton.setPressed(true, true);
        assertEquals(
                "Glic button incognito pressed tint is not as expected",
                incognitoHoverColor,
                mGlicButton.getBackgroundTint());
    }

    @Test
    public void testGlicButtonHoverEnterAndOnDown() {
        assertNotNull("Glic button should be created.", mGlicButton);
        showGlicButton();

        int x = (int) mGlicButton.getDrawX();
        mCoordinator.onHoverEvent(x + 1, 0);
        assertTrue("Glic button should be hovered", mGlicButton.isHovered());

        // Button has 4dp start click slop, so -5dp is outside button touch target.
        mCoordinator.onHoverEvent(x - 5, 0);
        assertFalse("Glic button should not be hovered outside slop", mGlicButton.isHovered());

        mCoordinator.onDown(mGlicButton.getDrawX() + 1, 0, 1);
        assertTrue("Glic button should be pressed from mouse", mGlicButton.isPressedFromMouse());
    }

    @Test
    public void testSetGlicButtonText() {
        showGlicButton();
        doTestSetButtonText(mGlicButton, "Glic Text", /* isActor= */ false);
    }

    @Test
    public void testGlicDismissNudgeButton() {
        GlicSplitButtonDelegate delegate = mCoordinator.getGlicSplitButtonDelegateForTesting();
        assertNotNull("Glic nudge delegate should be created.", delegate);
        assertFalse("Nudge should not be showing initially.", delegate.getIsShowingGlicNudge());

        // 1. Trigger the nudge via delegate method.
        delegate.onTriggerGlicNudgeUi("Glic Nudge Text", "", "");

        // Verify initial state: Delegate reports showing, Dismiss button visible, Glic button text
        // correct.
        assertTrue("Nudge should be showing after triggering.", delegate.getIsShowingGlicNudge());
        assertNotNull("Dismiss button should exist", mGlicDismissButton);
        assertTrue("Dismiss button should be visible", mGlicDismissButton.isVisible());
        assertEquals("Glic text should match setup text", "Glic Nudge Text", mGlicButton.getText());

        // 2. Simulate user pressing the dismiss button.
        mGlicDismissButton.handleClick(
                /* time= */ 0, /* motionEventButtonState= */ 0, /* modifiers= */ 0);

        // Verify dismiss button hides, delegate reports not showing, and Glic button text restores
        // to default.
        assertFalse(
                "Nudge should not be showing after handleClick dismissal.",
                delegate.getIsShowingGlicNudge());
        assertFalse("Dismiss button should have hidden", mGlicDismissButton.isVisible());
        assertEquals(
                "Glic button text should have been restored to default",
                mActivity.getString(R.string.glic_button_entrypoint_ask_gemini_label),
                mGlicButton.getText());

        // 3. Verify programmatic hide via delegate API directly.
        delegate.onTriggerGlicNudgeUi("Second Nudge Text", "", "");
        assertTrue("Nudge should be showing again.", delegate.getIsShowingGlicNudge());
        delegate.onHideGlicNudgeUi();
        assertFalse(
                "Nudge should not be showing after programmatic hide.",
                delegate.getIsShowingGlicNudge());
        assertFalse("Dismiss button should not be visible.", mGlicDismissButton.isVisible());
        assertEquals(
                "Glic text should have been restored to default.",
                mActivity.getString(R.string.glic_button_entrypoint_ask_gemini_label),
                mGlicButton.getText());
    }

    @Test
    public void testOnLongPress_OnGlicButton() {
        doTestGlicButtonContextMenuTriggered(/* viaSecondaryClick= */ false);
    }

    @Test
    public void testSecondaryClick_OnGlicButton() {
        doTestGlicButtonContextMenuTriggered(/* viaSecondaryClick= */ true);
    }

    private void doTestGlicButtonContextMenuTriggered(boolean viaSecondaryClick) {
        float x = mGlicButton.getDrawX() + mGlicButton.getWidth() / 2;
        float y = mGlicButton.getDrawY() + mGlicButton.getHeight() / 2;

        boolean handled =
                viaSecondaryClick
                        ? mCoordinator.click(0L, x, y, MotionEvent.BUTTON_SECONDARY, 0)
                        : mCoordinator.onLongPress(x, y);
        assertTrue("Context menu trigger should be handled.", handled);
        assertFalse(
                "Glic button should not be pressed after context menu is shown.",
                mGlicButton.isPressed());
        assertTrue("Glic context menu should be showing.", mCoordinator.isMenuShowing());
    }

    @Test
    public void testOpenContextMenu_glicButton() {
        assertFalse(
                "Should return false when Glic button is not keyboard focused.",
                mCoordinator.openKeyboardFocusedContextMenu(mActivity));

        mGlicButton.setKeyboardFocused(true);
        assertTrue(
                "Should return true when Glic button is keyboard focused.",
                mCoordinator.openKeyboardFocusedContextMenu(mActivity));
        assertTrue("Glic context menu should be showing.", mCoordinator.isMenuShowing());
    }

    // =========================================================================================
    // Glic Actor Button Unit Tests
    // =========================================================================================

    @Test
    public void testGlicActorButtonDrawX_Ltr() {
        assertNotNull("Glic Actor button should be created.", mGlicActorButton);
        showGlicActorButton();

        // Verify Glic Actor button x-position.
        // width(1000) - endSlop(6) - width(42) = 952
        assertEquals(
                "Glic Actor button LTR x-position is not as expected",
                952.f,
                mGlicActorButton.getDrawX(),
                0.0);
    }

    @Test
    public void testGlicActorButtonDrawX_Rtl() {
        assertNotNull("Glic Actor button should be created.", mGlicActorButton);
        LocalizationUtils.setRtlForTesting(true);
        showGlicActorButton();

        // Verify Glic Actor button x-position.
        // leftPadding(0) + endSlop(6) = 6
        assertEquals(
                "Glic Actor button RTL x-position is not as expected",
                6.f, // GLIC_BUTTON_END_SLOP
                mGlicActorButton.getDrawX(),
                0.0);
    }

    @Test
    public void testGlicActorButtonDrawY() {
        assertNotNull("Glic Actor button should be created.", mGlicActorButton);
        showGlicActorButton();

        // Verify Glic Actor button y-position.
        assertEquals(
                "Glic Actor button y-position is not as expected",
                3.f,
                mGlicActorButton.getDrawY(),
                0.0);
    }

    @Test
    public void testGlicActorButtonHoverHighlightProperties() {
        showGlicActorButton();
        assertNotNull("Glic Actor button should be created.", mGlicActorButton);
        assertEquals(
                "Glic Actor button background resource id is not as expected",
                Resources.ID_NULL,
                mGlicActorButton.getBackgroundResourceId());

        mGlicActorButton.setHovered(true);
        @ColorInt
        int hoverDefaultColor = mActivity.getColor(R.color.tab_strip_glic_button_bg_hover_tint);
        assertEquals(
                "Glic Actor button hover default tint is not as expected",
                hoverDefaultColor,
                mGlicActorButton.getBackgroundTint());

        mGlicActorButton.setHovered(false);
        mGlicActorButton.setPressed(true, true);
        @ColorInt
        int pressedColor = mActivity.getColor(R.color.tab_strip_glic_button_bg_pressed_tint);
        assertEquals(
                "Glic Actor button hover pressed tint is not as expected",
                pressedColor,
                mGlicActorButton.getBackgroundTint());
    }

    @Test
    public void testGlicActorButtonHoverEnterAndOnDown() {
        assertNotNull("Glic Actor button should be created.", mGlicActorButton);
        showGlicActorButton();

        int x = (int) mGlicActorButton.getDrawX();
        mCoordinator.onHoverEvent(x + 1, 0);
        assertTrue("Glic Actor button should be hovered", mGlicActorButton.isHovered());

        // Button has 4dp start click slop, so -5dp is outside button touch target.
        mCoordinator.onHoverEvent(x - 5, 0);
        assertFalse(
                "Glic Actor button should not be hovered outside slop",
                mGlicActorButton.isHovered());

        // Verify mouse touch down sets isPressedFromMouse.
        mCoordinator.onDown(mGlicActorButton.getDrawX() + 1, 0, 1);
        assertTrue(
                "Glic Actor button should be pressed from mouse",
                mGlicActorButton.isPressedFromMouse());
    }

    @Test
    public void testGlicActorButtonTextCollapsesOnSmallScreen() {
        assertNotNull("Actor button should be created.", mGlicActorButton);

        // Set text while on large screen
        when(mLayerTitleCache.getUpdatedGlicButtonText(any(), anyBoolean(), anyBoolean()))
                .thenReturn(123);
        when(mLayerTitleCache.getButtonTextWidth(any())).thenReturn(100);
        mCoordinator.setGlicActorButtonText("Actor Text", /* forceUpdate= */ false);
        mCoordinator.updateButtonTextProperties(mGlicActorButton);

        assertEquals(
                "Actor button text should be set on large screen.",
                "Actor Text",
                mGlicActorButton.getText());

        // Resize to a small screen width < 700
        mCoordinator.onSizeChanged(
                /* width= */ 500f,
                /* rightPadding= */ 0f,
                /* leftPadding= */ 0f,
                /* topPadding= */ 0f);

        assertNull(
                "Actor button text should collapse (become null) on small screen.",
                mGlicActorButton.getText());
    }

    @Test
    public void testSetGlicActorButtonText() {
        showGlicActorButton();
        doTestSetButtonText(mGlicActorButton, "Actor Text", /* isActor= */ true);
    }

    @Test
    public void testActorButtonStateChangedLifecycle() {
        // --- 1. Start State: Inactive ---
        assertFalse("Initially, Glic Actor button should be hidden.", mGlicActorButton.isVisible());
        assertEquals(
                "Initially, Glic primary button text should be default.",
                mActivity.getString(R.string.glic_button_entrypoint_ask_gemini_label),
                mGlicButton.getText());

        // --- 2. Transition: Active (task starts acting) ---
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(mActorTask));

        mCoordinator.onGlicActorButtonStateChanged(ButtonState.WORKING, false);
        ShadowLooper.idleMainLooper();

        // Verify actor button is shown (with no text) and primary button text is cleared.
        assertTrue("Actor button should become visible.", mGlicActorButton.isVisible());
        assertNull("Actor button text should be null in active state.", mGlicActorButton.getText());
        assertNull(
                "Primary Glic button text should be null when actor button is active.",
                mGlicButton.getText());

        // --- 3. Transition: Done (task finishes) ---
        mCoordinator.onGlicActorButtonStateChanged(ButtonState.DONE, false);
        ShadowLooper.idleMainLooper();

        // Verify actor button is still visible and text becomes "Done".
        assertTrue("Actor button should remain visible.", mGlicActorButton.isVisible());
        assertEquals(
                "Actor button text should become 'Task done'.",
                mActivity
                        .getResources()
                        .getQuantityString(R.plurals.actor_task_nudge_task_complete_label, 1),
                mGlicActorButton.getText());
        assertNull(
                "Primary Glic button text should remain null in done state.",
                mGlicButton.getText());

        // --- 4. Transition: Return to Inactive (task is dismissed/cancelled) ---
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.emptyList());

        mCoordinator.onGlicActorButtonStateChanged(ButtonState.DEFAULT, false);
        ShadowLooper.idleMainLooper();

        // Verify actor button hides and primary Glic button text is restored.
        assertFalse("Actor button should collapse and hide.", mGlicActorButton.isVisible());
        assertEquals(
                "Primary Glic button text should be restored to default.",
                mActivity.getString(R.string.glic_button_entrypoint_ask_gemini_label),
                mGlicButton.getText());
    }

    // =========================================================================================
    // Shared Coordinator & Multi-Button Interaction Unit Tests
    // =========================================================================================

    @Test
    @DisableFeatures(ChromeFeatureList.GLIC)
    public void testGlicButtonsDisabled() {
        assertNull("Glic button should not be created when feature is disabled.", mGlicButton);
        assertNull(
                "Glic Actor button should not be created when feature is disabled.",
                mGlicActorButton);
    }

    @Test
    public void testGlicButtonsEnabled() {
        assertNotNull("Glic button should be created when feature is enabled.", mGlicButton);
        assertNotNull(
                "Glic Actor button should be created when feature is enabled.", mGlicActorButton);
    }

    @Test
    public void testGlicButtonsUnfocusedOpacity() {
        assertNotNull("Glic button should be created.", mGlicButton);
        assertNotNull("Glic Actor button should be created.", mGlicActorButton);

        // Focused state
        mCoordinator.updateGlicButtonOpacity(
                /* isAppInDesktopWindow= */ true, /* isTopResumedActivity= */ true);
        assertEquals(
                "Glic button opacity should be 1.0 when focused in desktop windowing mode.",
                1.0f,
                mGlicButton.getOpacity(),
                MathUtils.EPSILON);
        assertEquals(
                "Glic button clickable threshold should be 1.0 when focused.",
                1.0f,
                mGlicButton.getClickableOpacityThreshold(),
                MathUtils.EPSILON);
        assertEquals(
                "Glic Actor button opacity should be 1.0 when focused.",
                1.0f,
                mGlicActorButton.getOpacity(),
                MathUtils.EPSILON);
        assertEquals(
                "Glic Actor button clickable threshold should be 1.0 when focused.",
                1.0f,
                mGlicActorButton.getClickableOpacityThreshold(),
                MathUtils.EPSILON);

        // Unfocused state
        mCoordinator.updateGlicButtonOpacity(
                /* isAppInDesktopWindow= */ true, /* isTopResumedActivity= */ false);
        assertEquals(
                "Glic button opacity should be 0.65 when unfocused in desktop windowing mode.",
                0.65f,
                mGlicButton.getOpacity(),
                MathUtils.EPSILON);
        assertEquals(
                "Glic button clickable threshold should be 0.65 when unfocused.",
                0.65f,
                mGlicButton.getClickableOpacityThreshold(),
                MathUtils.EPSILON);
        assertEquals(
                "Glic Actor button opacity should be 0.65 when unfocused.",
                0.65f,
                mGlicActorButton.getOpacity(),
                MathUtils.EPSILON);
        assertEquals(
                "Glic Actor button clickable threshold should be 0.65 when unfocused.",
                0.65f,
                mGlicActorButton.getClickableOpacityThreshold(),
                MathUtils.EPSILON);
    }

    @Test
    public void testGlicButtons_VisibleInIncognito() {
        // Setup an active actor task so shouldGlicActorBeVisible() would normally return true.
        when(mActorKeyedService.getActiveTasks()).thenReturn(List.of(mActorTask));
        assertTrue("Glic button should be visible initially.", mCoordinator.shouldGlicBeVisible());
        assertTrue(
                "Glic Actor button should be visible initially when tasks are active.",
                mCoordinator.shouldGlicActorBeVisible());

        mIsIncognito = true;
        mCoordinator.onTabModelSwitched(true);

        assertTrue(
                "Glic primary button should be visible even when supplier indicates incognito"
                        + " window.",
                mCoordinator.shouldGlicBeVisible());
        assertFalse(
                "Glic Actor button should NOT be visible in incognito window, even with active"
                        + " tasks.",
                mCoordinator.shouldGlicActorBeVisible());
    }

    @Test
    public void testOnTabModelSwitched() {
        // 1. Trigger the nudge in normal mode.
        mCoordinator
                .getGlicSplitButtonDelegateForTesting()
                .onTriggerGlicNudgeUi("Glic Nudge Text", "", "");

        // Verify initial state.
        assertEquals(
                "Glic button text should match nudge text.",
                "Glic Nudge Text",
                mGlicButton.getText());
        assertTrue("Dismiss button should be visible.", mGlicDismissButton.isVisible());

        int initialTint = mGlicButton.getTint();
        int initialBgTint = mGlicButton.getBackgroundTint();

        // 2. Switch to incognito mode.
        mIsIncognito = true;
        mCoordinator.onTabModelSwitched(true);

        // Verify that:
        // - Glic button text is reset to default in incognito mode.
        assertEquals(
                "Glic button text should be restored to default in incognito mode.",
                mActivity.getString(R.string.glic_button_entrypoint_ask_gemini_label),
                mGlicButton.getText());
        // - Dismiss nudge button is hidden in incognito mode.
        assertFalse(
                "Glic dismiss nudge button should be hidden in incognito mode.",
                mGlicDismissButton.isVisible());
        // - Tints are updated for incognito mode.
        assertTrue("Tint should change in incognito mode.", initialTint != mGlicButton.getTint());
        assertTrue(
                "Background tint should change in incognito mode.",
                initialBgTint != mGlicButton.getBackgroundTint());

        // 3. Switch back to normal mode.
        mIsIncognito = false;
        mCoordinator.onTabModelSwitched(false);

        // Verify that:
        // - Glic button text is restored to nudge text when returning to normal mode.
        assertEquals(
                "Glic button text should be restored to nudge text.",
                "Glic Nudge Text",
                mGlicButton.getText());
        // - Dismiss nudge button is visible again when returning to normal mode.
        assertTrue("Dismiss button should be visible again.", mGlicDismissButton.isVisible());
        // - Tints are restored.
        assertEquals("Tint should be restored.", initialTint, mGlicButton.getTint());
        assertEquals(
                "Background tint should be restored.",
                initialBgTint,
                mGlicButton.getBackgroundTint());
    }

    @Test
    public void testGlicButtons_HiddenWhenSidePanelNotShowable() {
        assertTrue("Glic button should be visible initially.", mCoordinator.shouldGlicBeVisible());

        when(mSideUiStateProvider.canShowSideUi(SideUiId.SIDE_PANEL)).thenReturn(false);

        // Notify observer of updates
        ArgumentCaptor<SideUiObserver> observerCaptor =
                ArgumentCaptor.forClass(SideUiObserver.class);
        verify(mSideUiStateProvider).addObserver(observerCaptor.capture());
        observerCaptor
                .getValue()
                .onShowableSideUisUpdated(
                        new SideUiShowability(List.of(), List.of(SideUiId.SIDE_PANEL)));

        assertFalse(
                "Glic button should be hidden when side panel is not showable.",
                mCoordinator.shouldGlicBeVisible());
        assertFalse(
                "Glic Actor button should also be hidden when side panel is not showable.",
                mCoordinator.shouldGlicActorBeVisible());
    }

    @Test
    public void testStandardClick_TrailingButtons() {
        showGlicActorButton();
        assertNotNull("Glic Actor button should be created.", mGlicActorButton);

        // 1. Test click routing on Glic Button coordinates
        float glicX = mGlicButton.getDrawX() + mGlicButton.getWidth() / 2;
        float glicY = mGlicButton.getDrawY() + mGlicButton.getHeight() / 2;
        boolean glicHandled = mCoordinator.click(0L, glicX, glicY, 0, 0);
        assertTrue("Click on Glic coordinates should be handled.", glicHandled);
        verify(mGlicClickHandler, Mockito.times(1))
                .onClick(
                        /* preventClose= */ false,
                        GlicKeyedService.GlicInvocationSource.TOP_CHROME_BUTTON);

        // 2. Test click routing on Glic Actor Button coordinates
        float actorX = mGlicActorButton.getDrawX() + mGlicActorButton.getWidth() / 2;
        float actorY = mGlicActorButton.getDrawY() + mGlicActorButton.getHeight() / 2;
        boolean actorHandled = mCoordinator.click(0L, actorX, actorY, 0, 0);
        assertTrue("Click on Glic Actor coordinates should be handled.", actorHandled);
    }

    @Test
    public void testOnDown_TrailingButtons() {
        assertNotNull("Glic button should be created.", mGlicButton);
        showGlicActorButton();
        assertNotNull("Glic Actor button should be created.", mGlicActorButton);

        // 1. Simulate tactile touch-down on Glic button
        float glicX = mGlicButton.getDrawX() + mGlicButton.getWidth() / 2;
        float glicY = mGlicButton.getDrawY() + mGlicButton.getHeight() / 2;
        boolean glicPressed = mCoordinator.onDown(glicX, glicY, 0);
        assertTrue("Touch down on Glic coordinates should be handled.", glicPressed);
        assertTrue("Glic button should be pressed.", mGlicButton.isPressed());

        // 2. Simulate tactile touch-down on Glic Actor button
        float actorX = mGlicActorButton.getDrawX() + mGlicActorButton.getWidth() / 2;
        float actorY = mGlicActorButton.getDrawY() + mGlicActorButton.getHeight() / 2;
        boolean actorPressed = mCoordinator.onDown(actorX, actorY, 0);
        assertTrue("Touch down on Glic Actor coordinates should be handled.", actorPressed);
        assertTrue("Glic Actor button should be pressed.", mGlicActorButton.isPressed());
    }

    @Test
    public void testHoverLifecycle_TrailingButtons() {
        assertNotNull("Glic button should be created.", mGlicButton);
        showGlicActorButton();
        assertNotNull("Glic Actor button should be created.", mGlicActorButton);

        float glicX = mGlicButton.getDrawX() + mGlicButton.getWidth() / 2;
        float glicY = mGlicButton.getDrawY() + mGlicButton.getHeight() / 2;
        float actorX = mGlicActorButton.getDrawX() + mGlicActorButton.getWidth() / 2;
        float actorY = mGlicActorButton.getDrawY() + mGlicActorButton.getHeight() / 2;

        // 1. Pointer moves into Glic primary bounds
        boolean handled1 = mCoordinator.onHoverEvent(glicX, glicY);
        assertTrue("Hovering Glic should be handled.", handled1);
        assertTrue("Glic should be in hovered state.", mGlicButton.isHovered());
        assertFalse("Glic Actor should not be hovered yet.", mGlicActorButton.isHovered());
        verify(mRenderHost, Mockito.atLeastOnce()).requestRender();
        Mockito.clearInvocations(mRenderHost);

        // 2. Pointer sweeps over onto Companion Actor bounds directly
        boolean handled2 = mCoordinator.onHoverEvent(actorX, actorY);
        assertTrue("Hovering Glic Actor bounds should be handled.", handled2);
        assertFalse("Glic should clear hover state.", mGlicButton.isHovered());
        assertTrue("Glic Actor should acquire hover state.", mGlicActorButton.isHovered());
        verify(mRenderHost, Mockito.atLeastOnce()).requestRender();
        Mockito.clearInvocations(mRenderHost);

        // 3. Pointer completely leaves the trailing buttons area
        mCoordinator.onHoverExit();
        assertFalse("Glic should remain unhovered.", mGlicButton.isHovered());
        assertFalse(
                "Glic Actor hover state must reset to false upon exit.",
                mGlicActorButton.isHovered());
        verify(mRenderHost, Mockito.atLeastOnce()).requestRender();
    }

    @Test
    public void testGlicButtonsAnimations() {
        assertNotNull("Glic button should be created.", mGlicButton);
        assertNotNull("Glic Actor button should be created.", mGlicActorButton);

        when(mLayerTitleCache.getUpdatedGlicButtonText(any(), anyBoolean(), anyBoolean()))
                .thenReturn(123);
        when(mLayerTitleCache.getButtonTextWidth(any())).thenReturn(100);

        // Create a unified spy of the coordinator for sequential transition verification
        StripLayoutTrailingButtonsCoordinator coordinatorSpy = Mockito.spy(mCoordinator);

        // 1. Test Glic Button Expansion Transition (Simulating contextual cueing nudge)
        coordinatorSpy.setNudgeLabelForTesting("Glic Nudge");
        Mockito.verify(coordinatorSpy, Mockito.atLeastOnce())
                .startAnimations(mAnimatorsListCaptor.capture(), Mockito.any());
        assertEquals(
                "Glic button expansion should queue 3 animators concurrently (width, opacity,"
                        + " dismiss slide).",
                3,
                mAnimatorsListCaptor.getValue().size());

        Mockito.clearInvocations(coordinatorSpy);

        // 2. Test Glic Button Shrink/Collapse Transition (Simulating contextual cueing nudge
        // dismissal)
        coordinatorSpy.setNudgeLabelForTesting(null);
        Mockito.verify(coordinatorSpy, Mockito.atLeastOnce())
                .startAnimations(mAnimatorsListCaptor.capture(), Mockito.any());
        assertEquals(
                "Glic button shrink transition should queue 2 animators concurrently (width,"
                        + " opacity).",
                2,
                mAnimatorsListCaptor.getValue().size());

        Mockito.clearInvocations(coordinatorSpy);

        // 3. Test Glic Actor Button Expansion Transition (Simulating actor task nudge)
        coordinatorSpy.setGlicActorButtonText("Actor Nudge", /* forceUpdate= */ false);
        coordinatorSpy.updateButtonTextProperties(mGlicActorButton);
        Mockito.verify(coordinatorSpy, Mockito.atLeastOnce())
                .startAnimations(mAnimatorsListCaptor.capture(), Mockito.any());
        assertEquals(
                "Glic Actor button expansion should queue 2 animators concurrently (width,"
                        + " opacity).",
                2,
                mAnimatorsListCaptor.getValue().size());
    }

    @Test
    public void testShouldShowDivider() {
        // Initially mCoordinator is created with isAppInDesktopWindow = false,
        // and shouldShowDivider should return false.
        assertFalse(
                "Divider should not be shown when not in desktop windowing.",
                mCoordinator.shouldShowDivider());

        // Update isAppInDesktopWindow = true.
        mCoordinator.updateGlicButtonOpacity(
                /* isAppInDesktopWindow= */ true, /* isTopResumedActivity= */ true);
        assertTrue(
                "Divider should be shown when in desktop windowing.",
                mCoordinator.shouldShowDivider());

        // Hide Glic button.
        mCoordinator.setGlicButtonVisible(false);
        assertFalse(
                "Divider should not be shown when Glic button is not visible.",
                mCoordinator.shouldShowDivider());
    }

    // =========================================================================================
    // Private Helper Methods
    // =========================================================================================

    private void showGlicButton() {
        mCoordinator.setGlicButtonVisible(true);
        mGlicButton.setWidth(
                mActivity.getResources().getDimension(R.dimen.tab_strip_glic_button_bg_width));
        mGlicButton.setOpacity(1.0f);
        mCoordinator.updateButtonPositions();
    }

    private void showGlicActorButton() {
        mCoordinator.setGlicActorButtonVisible(true, /* animate= */ false);
        mGlicActorButton.setWidth(
                mActivity.getResources().getDimension(R.dimen.tab_strip_glic_button_bg_width));
        mGlicActorButton.setOpacity(1.0f);
        mCoordinator.updateButtonPositions();
    }

    private void showModelSelectorButton() {
        mModelSelectorButton.setVisible(true);
        mModelSelectorButton.setOpacity(1.0f);
        mCoordinator.updateButtonPositions();
    }

    private void doTestSetButtonText(
            TintedCompositorTextButton button, String text, boolean isActor) {
        assertNotNull("Button should be created.", button);

        // Start with no-text state button
        boolean textChanged = button.getText() != null;
        button.setText(null);
        if (textChanged) {
            mCoordinator.updateButtonTextProperties(button);
        }
        float initialWidth = button.getWidth();
        when(mLayerTitleCache.getUpdatedGlicButtonText(any(), anyBoolean(), anyBoolean()))
                .thenReturn(123);
        when(mLayerTitleCache.getButtonTextWidth(any())).thenReturn(100);

        // Set text
        button.setText(text);
        mCoordinator.updateButtonTextProperties(button);

        // Assert the button has expanded in width
        verify(mLayerTitleCache, Mockito.atLeastOnce())
                .getUpdatedGlicButtonText(Mockito.eq(text), Mockito.eq(isActor), anyBoolean());
        assertTrue(
                "Button width should increase to accommodate text.",
                button.getWidth() > initialWidth);

        // Set text back to null
        button.setText(null);
        mCoordinator.updateButtonTextProperties(button);

        // Assert the button has shrunk back to original width
        assertEquals(
                "Button width should return to original singular icon width.",
                initialWidth,
                button.getWidth(),
                MathUtils.EPSILON);
    }
}
