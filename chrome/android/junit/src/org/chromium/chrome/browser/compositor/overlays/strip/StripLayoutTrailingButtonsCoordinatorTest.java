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

import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.getDimensionDp;

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

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.DeviceInfo;
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
import org.chromium.chrome.browser.incognito.IncognitoUtils;
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

import java.lang.ref.WeakReference;
import java.util.Collections;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.GLIC, ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL})
public class StripLayoutTrailingButtonsCoordinatorTest {
    private static final float DEFAULT_AVAILABLE_SPACE_DP = 1000f;

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
    @Mock private Callback<Boolean> mGlicPanelStateObserver;

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
    private float mAvailableSpaceDp = DEFAULT_AVAILABLE_SPACE_DP;

    @Before
    public void setUp() {
        GlicEnabling.setEnabledForTesting(ChromeFeatureList.isEnabled(ChromeFeatureList.GLIC));
        GlicSplitButtonDelegateBridgeJni.setInstanceForTesting(
                mGlicSplitButtonDelegateBridgeJniMock);
        CompositorAnimationHandler.setTestingMode(true);
        when(mUpdateHost.getAnimationHandler())
                .thenReturn(new CompositorAnimationHandler(CallbackUtils.emptyRunnable()));

        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarJniMock);
        when(mPrefChangeRegistrarJniMock.init(any(), any())).thenReturn(1L);

        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP)).thenReturn(true);

        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.emptyList());
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
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

        when(mLayerTitleCache.getUpdatedGlicButtonText(any(), anyBoolean(), anyBoolean()))
                .thenReturn(123);
        when(mLayerTitleCache.getButtonTextWidth(any())).thenReturn(100);

        initializeCoordinator();
    }

    private void initializeCoordinator() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
        }
        mAvailableSpaceDp = DEFAULT_AVAILABLE_SPACE_DP;
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
                        () -> mAvailableSpaceDp,
                        () -> 100f,
                        () -> {},
                        (isFocused, view) -> {},
                        mGlicClickHandler,
                        (isFocused, view) -> {},
                        () -> mGlicIphShowing,
                        mGlicPanelStateObserver,
                        mObserver);
        ShadowLooper.idleMainLooper();
        mCoordinator.onProfileAvailable(mProfile);
        mCoordinator.setLayerTitleCache(mLayerTitleCache);
        onSizeChanged(DEFAULT_AVAILABLE_SPACE_DP);
        mGlicButton = mCoordinator.getGlicButton();
        mGlicDismissButton = mGlicButton != null ? mGlicButton.getDismissButton() : null;
        mGlicActorButton = mCoordinator.getGlicActorButton();
        mModelSelectorButton = mCoordinator.getModelSelectorButton();
    }

    private void onSizeChanged(float width) {
        mAvailableSpaceDp = width;
        if (mCoordinator != null) {
            mCoordinator.onSizeChanged(width, 0f, 0f, 0f);
        }
    }

    @After
    public void tearDown() {
        DeviceInfo.resetIsDesktopForTesting();
        CompositorAnimationHandler.setTestingMode(false);
        if (mCoordinator != null) {
            mCoordinator.destroy();
        }
        DeviceInfo.resetIsDesktopForTesting();
    }

    // =========================================================================================
    // Model Selector Button (MSB) Unit Tests
    // =========================================================================================

    @Test
    public void testModelSelectorButton_VisibilityWhenIncognitoTabsExist() {
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        assertTrue(
                "MSB should be visible when incognito tabs exist",
                mCoordinator.shouldModelSelectorButtonBeVisible());
        mCoordinator.updateTrailingButtons();
        assertTrue("MSB view should be visible", mModelSelectorButton.isVisible());
    }

    @Test
    public void testModelSelectorButton_VisibilityWhenNoIncognitoTabs() {
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        assertFalse(
                "MSB should not be visible when no incognito tabs exist",
                mCoordinator.shouldModelSelectorButtonBeVisible());
        mCoordinator.updateTrailingButtons();
        assertFalse("MSB view should be hidden", mModelSelectorButton.isVisible());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testModelSelectorButton_NotCreatedWhenIncognitoAsWindowEnabled() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        initializeCoordinator();
        assertNull(
                "Model selector button should not be created when Incognito as window is enabled",
                mModelSelectorButton);
        assertFalse(
                "MSB should not be visible when Incognito as window is enabled",
                mCoordinator.shouldModelSelectorButtonBeVisible());
    }

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
        float initialWidth = mGlicButton.getWidth();

        mCoordinator.setNudgeLabelForTesting("Glic Text");
        onSizeChanged(DEFAULT_AVAILABLE_SPACE_DP);

        verify(mLayerTitleCache, Mockito.atLeastOnce())
                .getUpdatedGlicButtonText(Mockito.eq("Glic Text"), Mockito.eq(false), anyBoolean());
        assertTrue(
                "Glic button width should increase to accommodate text.",
                mGlicButton.getWidth() > initialWidth);

        mCoordinator.setNudgeLabelForTesting(null);
        float minCondensedWidth =
                mCoordinator.calculateMinRequiredWidthForGlicButton(
                        /* text= */ null, /* showDismissButton= */ false);
        onSizeChanged(minCondensedWidth);

        assertEquals(
                "Glic button width should return to original singular icon width.",
                initialWidth,
                mGlicButton.getWidth(),
                MathUtils.EPSILON);
    }

    @Test
    public void testGlicHighlightedState_GlicUiShowHide() {
        assertNotNull("Glic button should be created.", mGlicButton);
        assertFalse(
                "Glic button should not be highlighted initially.", mGlicButton.isHighlighted());

        // Simulate Glic UI opening event.
        mCoordinator.getGlicSplitButtonDelegateForTesting().setGlicPanelIsOpen(true);

        // Verify button is in highlighted state.
        assertTrue(
                "Glic button should be highlighted when UI is shown globally.",
                mGlicButton.isHighlighted());

        // Simulate Glic UI hiding event.
        mCoordinator.getGlicSplitButtonDelegateForTesting().setGlicPanelIsOpen(false);

        // Verify button returns to non-highlighted state.
        assertFalse(
                "Glic button should not be highlighted when UI is hidden globally.",
                mGlicButton.isHighlighted());
    }

    @Test
    public void testGlicActorHighlightedState_TaskMenuShowHide() {
        showGlicActorButton();
        assertNotNull("Glic Actor button should be created.", mGlicActorButton);
        assertFalse(
                "Glic Actor button should not be highlighted initially.",
                mGlicActorButton.isHighlighted());

        // Mock active tasks to ensure the menu actually opens
        when(mActorTask.getTitle()).thenReturn("Test Task");
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(mActorTask));

        // Simulate clicking the actor button to open the task menu
        float actorX = mGlicActorButton.getDrawX() + mGlicActorButton.getWidth() / 2;
        float actorY = mGlicActorButton.getDrawY() + mGlicActorButton.getHeight() / 2;
        mCoordinator.click(0L, actorX, actorY, 0, 0);

        // Verify button is in highlighted state and task menu is showing
        assertTrue(
                "Glic Actor button should be highlighted after task menu is shown.",
                mGlicActorButton.isHighlighted());
        assertTrue("Glic task menu should be showing.", mCoordinator.isMenuShowing());

        // Simulate dismissing the task menu
        mCoordinator.dismissTrailingButtonsMenu();

        // Verify button returns to non-highlighted state
        assertFalse(
                "Glic Actor button should not be highlighted after task menu is dismissed.",
                mGlicActorButton.isHighlighted());
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
    public void testGlicButton_DegradationOnNarrowScreen() {
        String askGeminiText =
                mActivity.getString(R.string.glic_button_entrypoint_ask_gemini_label);
        float minFullWidth =
                mCoordinator.calculateMinRequiredWidthForGlicButton(
                        askGeminiText, /* showDismissButton= */ false);
        float minCondensedWidth =
                mCoordinator.calculateMinRequiredWidthForGlicButton(
                        /* text= */ null, /* showDismissButton= */ false);

        // 1. Wide screen (>= minFullWidth): Glic button is visible with full text.
        onSizeChanged(minFullWidth);
        assertTrue("Glic button should be visible on wide screen.", mGlicButton.isVisible());
        assertEquals(
                "Glic button text should be set on wide screen.",
                askGeminiText,
                mGlicButton.getText());
        assertEquals(
                "Glic button accessibility description should match text on wide screen.",
                askGeminiText,
                mGlicButton.getAccessibilityDescription());

        // 2. Narrow screen (minCondensedWidth <= width < minFullWidth): Glic button is visible,
        // but text collapses.
        onSizeChanged(minFullWidth - 1.f);
        assertTrue("Glic button should remain visible when condensed.", mGlicButton.isVisible());
        assertNull(
                "Glic button text should collapse (become null) on narrow screen.",
                mGlicButton.getText());
        assertEquals(
                "Glic button width should collapse to bg width on narrow screen.",
                mActivity.getResources().getDimension(R.dimen.tab_strip_glic_button_bg_width),
                mGlicButton.getWidth(),
                0.0f);
        assertEquals(
                "Glic button accessibility description should fall back to default when condensed.",
                mActivity.getString(R.string.glic_tab_strip_button_tooltip),
                mGlicButton.getAccessibilityDescription());

        // 3. Very narrow screen (< minCondensedWidth): Glic button hides completely.
        onSizeChanged(minCondensedWidth - 1.f);
        assertFalse(
                "Glic button should be hidden when strip is too narrow for condensed button.",
                mGlicButton.isVisible());

        // 4. Resize back to wide screen: Glic button is restored with full text.
        onSizeChanged(minFullWidth);
        assertTrue("Glic button should be restored on wide screen.", mGlicButton.isVisible());
        assertEquals(
                "Glic button text should be restored on wide screen.",
                askGeminiText,
                mGlicButton.getText());
        assertEquals(
                "Glic button accessibility description should be restored on wide screen.",
                askGeminiText,
                mGlicButton.getAccessibilityDescription());
    }

    @Test
    public void testGlicNudge_FallsBackToDefaultTextOnNarrowScreen() {
        String askGeminiText =
                mActivity.getString(R.string.glic_button_entrypoint_ask_gemini_label);
        String nudgeText = "Summarize page";
        when(mLayerTitleCache.getButtonTextWidth(nudgeText)).thenReturn(150);
        when(mLayerTitleCache.getButtonTextWidth(askGeminiText)).thenReturn(80);

        mCoordinator.setNudgeLabelForTesting(nudgeText);
        float minNudgeWidth =
                mCoordinator.calculateMinRequiredWidthForGlicButton(
                        nudgeText, /* showDismissButton= */ true);
        float minFullWidth =
                mCoordinator.calculateMinRequiredWidthForGlicButton(
                        askGeminiText, /* showDismissButton= */ false);

        assertTrue(
                "Nudge with longer text should require more width than standard full button.",
                minNudgeWidth > minFullWidth);

        // 1. Width >= minNudgeWidth: Nudge label and dismiss button visible.
        onSizeChanged(minNudgeWidth);
        assertTrue(mGlicButton.isVisible());
        assertTrue(mGlicDismissButton.isVisible());
        assertEquals(nudgeText, mGlicButton.getText());

        // 2. minFullWidth <= Width < minNudgeWidth: Nudge dismisses, falls back to "Ask Gemini".
        onSizeChanged(minNudgeWidth - 1.f);
        assertTrue(mGlicButton.isVisible());
        assertFalse(mGlicDismissButton.isVisible());
        assertEquals(askGeminiText, mGlicButton.getText());

        // 3. Width < minFullWidth: "Ask Gemini" collapses to icon-only.
        onSizeChanged(minFullWidth - 1.f);
        assertTrue(mGlicButton.isVisible());
        assertFalse(mGlicDismissButton.isVisible());
        assertNull(mGlicButton.getText());

        // 4. Resize back to wide screen >= minNudgeWidth: Nudge label and dismiss button restored.
        onSizeChanged(minNudgeWidth);
        assertTrue(mGlicButton.isVisible());
        assertTrue(mGlicDismissButton.isVisible());
        assertEquals(nudgeText, mGlicButton.getText());
    }

    @Test
    public void testGlicNudge_HiddenAndSuppressedWhenActorTaskActive() {
        // 1. Display a Glic nudge initially with no active actor task.
        String initialNudgeText = "Summarize page";
        mCoordinator.setNudgeLabelForTesting(initialNudgeText);
        ShadowLooper.idleMainLooper();

        assertTrue("Glic button should be visible.", mGlicButton.isVisible());
        assertTrue(
                "Dismiss button should be visible when nudge is shown.",
                mGlicDismissButton.isVisible());
        assertEquals("Glic text should show nudge text.", initialNudgeText, mGlicButton.getText());
        assertFalse("Actor button should not be visible initially.", mGlicActorButton.isVisible());

        // 2. An actor task starts: actor button should appear, and nudge / dismiss button should
        // hide.
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(mActorTask));
        mCoordinator.onGlicActorButtonStateChanged(ButtonState.WORKING, false);
        ShadowLooper.idleMainLooper();

        assertTrue("Actor button should become visible.", mGlicActorButton.isVisible());
        assertNull(
                "Glic text should collapse (null) when actor task starts.", mGlicButton.getText());
        assertFalse(
                "Dismiss button should hide when actor task starts.",
                mGlicDismissButton.isVisible());

        // 3. Attempt to trigger a new Glic nudge while the Actor task is active: should remain
        // suppressed.
        mCoordinator.setNudgeLabelForTesting("Second nudge text");
        mCoordinator.onSizeChanged(1000f, 0f, 0f, 0f);
        ShadowLooper.idleMainLooper();

        assertNull(
                "Glic text should remain collapsed (null) when actor task is active.",
                mGlicButton.getText());
        assertFalse(
                "Dismiss button should remain hidden when actor task is active.",
                mGlicDismissButton.isVisible());
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

    @Test
    public void testGlicButtonWidth_NoText_DesktopDensity() {
        doTestButtonWidthNoText(/* isActor= */ false, /* isDesktopDensity= */ true);
    }

    @Test
    public void testGlicButtonWidth_NoText_NonDesktopDensity() {
        doTestButtonWidthNoText(/* isActor= */ false, /* isDesktopDensity= */ false);
    }

    @Test
    public void testGlicButton_PrefChangeUpdatesVisibility_Incognito() {
        mCoordinator.onTabModelSwitched(true);
        assertTrue(
                "Glic button should initially be visible when pinned.",
                mCoordinator.shouldGlicBeVisible());

        // Simulate unpinning in preferences.
        when(mPrefService.getBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP)).thenReturn(false);
        mCoordinator.onGlicPrefChanged();

        assertFalse(
                "Glic button should be hidden after unpinning.",
                mCoordinator.shouldGlicBeVisible());
        assertFalse("Glic button visible property should be false.", mGlicButton.isVisible());

        // Simulate re-pinning in preferences.
        when(mPrefService.getBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP)).thenReturn(true);
        mCoordinator.onGlicPrefChanged();

        assertTrue(
                "Glic button should be visible again after pinning.",
                mCoordinator.shouldGlicBeVisible());
        assertTrue("Glic button visible property should be true.", mGlicButton.isVisible());
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
    public void testGlicActorButton_DegradationOnNarrowScreen() {
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(mActorTask));
        showGlicActorButton();

        String actorNudgeText =
                mActivity
                        .getResources()
                        .getQuantityString(R.plurals.actor_task_nudge_task_complete_label, 1);
        float minActorNudgeWidth =
                mCoordinator.calculateMinRequiredWidthForGlicButton(
                        actorNudgeText, /* showDismissButton= */ false);

        // 1. Transition to DONE state on a wide screen >= minActorNudgeWidth
        onSizeChanged(minActorNudgeWidth);
        mCoordinator.onGlicActorButtonStateChanged(ButtonState.DONE, false);
        ShadowLooper.idleMainLooper();

        assertTrue("Actor button should be visible.", mGlicActorButton.isVisible());
        assertEquals(
                "Actor button text should be set on wide screen.",
                actorNudgeText,
                mGlicActorButton.getText());
        assertEquals(
                "Actor button accessibility description should match text on wide screen.",
                actorNudgeText,
                mGlicActorButton.getAccessibilityDescription());

        // 2. Narrow screen (minCondensedWidth <= width < minActorNudgeWidth): Actor text collapses.
        float minCondensedWidth =
                mCoordinator.calculateMinRequiredWidthForGlicButton(
                        /* text= */ null, /* showDismissButton= */ false);
        onSizeChanged(minActorNudgeWidth - 1.f);

        assertTrue(
                "Actor button should remain visible when condensed.", mGlicActorButton.isVisible());
        assertNull(
                "Actor button text should collapse (become null) on narrow screen.",
                mGlicActorButton.getText());
        assertEquals(
                "Actor button accessibility description should fall back to default when"
                        + " condensed.",
                mActivity.getString(R.string.actor_task_indicator_tooltip),
                mGlicActorButton.getAccessibilityDescription());

        // 3. Very narrow screen (< minCondensedWidth): Actor button hides completely.
        onSizeChanged(minCondensedWidth - 1.f);

        assertFalse(
                "Actor button should be hidden when strip is too narrow for condensed button.",
                mGlicActorButton.isVisible());

        // 4. Resize back to wide screen >= minActorNudgeWidth: Actor button is restored with text.
        onSizeChanged(minActorNudgeWidth);

        assertTrue("Actor button should be restored on wide screen.", mGlicActorButton.isVisible());
        assertEquals(
                "Actor button text should be restored on wide screen.",
                actorNudgeText,
                mGlicActorButton.getText());
        assertEquals(
                "Actor button accessibility description should be restored on wide screen.",
                actorNudgeText,
                mGlicActorButton.getAccessibilityDescription());
    }

    @Test
    public void testSetGlicActorButtonText() {
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(mActorTask));
        showGlicActorButton();
        float initialWidth = mGlicActorButton.getWidth();

        // Transition to DONE state on wide screen
        onSizeChanged(DEFAULT_AVAILABLE_SPACE_DP);
        mCoordinator.onGlicActorButtonStateChanged(ButtonState.DONE, false);

        verify(mLayerTitleCache, Mockito.atLeastOnce())
                .getUpdatedGlicButtonText(any(), Mockito.eq(true), anyBoolean());
        assertTrue(
                "Actor button width should increase to accommodate text.",
                mGlicActorButton.getWidth() > initialWidth);

        // Transition back to DEFAULT state
        mCoordinator.onGlicActorButtonStateChanged(ButtonState.DEFAULT, false);
        assertEquals(
                "Actor button width should return to original singular icon width.",
                initialWidth,
                mGlicActorButton.getWidth(),
                MathUtils.EPSILON);
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
    public void testGlicActorButtonWidth_NoText_DesktopDensity() {
        doTestButtonWidthNoText(/* isActor= */ true, /* isDesktopDensity= */ true);
    }

    @Test
    public void testGlicActorButtonWidth_NoText_NonDesktopDensity() {
        doTestButtonWidthNoText(/* isActor= */ true, /* isDesktopDensity= */ false);
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
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(mActorTask));
        showGlicActorButton();
        coordinatorSpy.onGlicActorButtonStateChanged(ButtonState.DONE, false);
        Mockito.verify(coordinatorSpy, Mockito.atLeastOnce())
                .startAnimations(mAnimatorsListCaptor.capture(), Mockito.any());
        assertEquals(
                "Glic Actor button expansion should queue 2 animators concurrently (width,"
                        + " opacity).",
                2,
                mAnimatorsListCaptor.getValue().size());
    }

    @Test
    public void testGetTrailingButtonsWidthWithPadding() {
        // 1. No buttons visible.
        mCoordinator.setGlicButtonVisible(false);
        assertEquals(
                "Width should be 0 when no buttons visible.",
                0.0f,
                mCoordinator.getTrailingButtonsWidthWithPadding(),
                0.0);

        // 2. MSB only (Tablet desired touch target = 48).
        showModelSelectorButton();
        assertEquals(
                "Width should match MSB touch target size on tablet.",
                48.0f,
                mCoordinator.getTrailingButtonsWidthWithPadding(),
                0.0);

        // 3. MSB (48) + Glic (width(42) + startSlop(4) + endSlop(6)) = 100.
        showGlicButton();
        assertEquals(
                "Width should match MSB + Glic.",
                100.0f,
                mCoordinator.getTrailingButtonsWidthWithPadding(),
                0.0);

        // 4. MSB (48) + Glic (52) + gap(2) + Actor(42) = 144.
        showGlicActorButton();
        assertEquals(
                "Width should match MSB + Glic + Actor with gap.",
                144.0f,
                mCoordinator.getTrailingButtonsWidthWithPadding(),
                0.0);

        // 5. Desktop density (MSB touch target shrinks to 32).
        DeviceInfo.setIsDesktopForTesting(true);
        mCoordinator.setGlicButtonVisible(false);
        mCoordinator.setGlicActorButtonVisible(false, /* animate= */ false);
        assertEquals(
                "Width should match MSB touch target size on desktop.",
                32.0f,
                mCoordinator.getTrailingButtonsWidthWithPadding(),
                0.0);
    }

    @Test
    public void testSetGlicPanelIsOpen_notifiesObserver() {
        GlicSplitButtonDelegate splitButtonDelegate =
                mCoordinator.getGlicSplitButtonDelegateForTesting();
        assertNotNull("Split button delegate should be created.", splitButtonDelegate);

        // Open the panel.
        splitButtonDelegate.setGlicPanelIsOpen(true);
        verify(mGlicPanelStateObserver).onResult(true);

        // Close the panel.
        splitButtonDelegate.setGlicPanelIsOpen(false);
        verify(mGlicPanelStateObserver).onResult(false);
    }

    // =========================================================================================
    // Private Helper Methods
    // =========================================================================================

    private void showGlicButton() {
        mCoordinator.setGlicButtonVisible(true);
        mGlicButton.setWidth(getDimensionDp(mActivity, R.dimen.tab_strip_glic_button_bg_width));
        mGlicButton.setOpacity(1.0f);
        mCoordinator.updateButtonPositions();
    }

    private void showGlicActorButton() {
        mCoordinator.setGlicActorButtonVisible(true, /* animate= */ false);
        mGlicActorButton.setWidth(
                getDimensionDp(mActivity, R.dimen.tab_strip_glic_button_bg_width));
        mGlicActorButton.setOpacity(1.0f);
        mCoordinator.updateButtonPositions();
    }

    private void showModelSelectorButton() {
        mModelSelectorButton.setVisible(true);
        mModelSelectorButton.setOpacity(1.0f);
        mCoordinator.updateButtonPositions();
    }

    private void doTestButtonWidthNoText(boolean isActor, boolean isDesktopDensity) {
        DeviceInfo.setIsDesktopForTesting(isDesktopDensity);
        initializeCoordinator();
        if (isActor) {
            when(mActorKeyedService.getActiveTasks())
                    .thenReturn(Collections.singletonList(mActorTask));
            showGlicActorButton();
        }
        TintedCompositorTextButton button = isActor ? mGlicActorButton : mGlicButton;
        assertNotNull("Button should be created.", button);

        // Collapse text on narrow screen.
        float minCondensedWidth =
                mCoordinator.calculateMinRequiredWidthForGlicButton(
                        /* text= */ null, /* showDismissButton= */ false);
        onSizeChanged(minCondensedWidth);

        float expectedWidth =
                isDesktopDensity
                        ? getDimensionDp(mActivity, R.dimen.tab_strip_button_bg_size)
                        : getDimensionDp(mActivity, R.dimen.tab_strip_glic_button_bg_width);
        String densityDesc = isDesktopDensity ? "desktop" : "non-desktop";
        assertEquals(
                "Button width without text should match expected width on "
                        + densityDesc
                        + " density.",
                expectedWidth,
                button.getWidth(),
                MathUtils.EPSILON);
    }
}
