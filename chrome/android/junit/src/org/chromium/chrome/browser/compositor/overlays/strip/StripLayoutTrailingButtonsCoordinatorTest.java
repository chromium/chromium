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
import android.animation.AnimatorListenerAdapter;
import android.app.Activity;
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

import org.chromium.base.MathUtils;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
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
import org.chromium.chrome.browser.glic.GlicNudgeDelegate;
import org.chromium.chrome.browser.glic.GlicNudgeDelegateBridge;
import org.chromium.chrome.browser.glic.GlicNudgeDelegateBridgeJni;
import org.chromium.chrome.browser.glic.GlicPrefNames;
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
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

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
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Profile mProfile;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefChangeRegistrar.Natives mPrefChangeRegistrarJniMock;
    @Mock private PrefService mPrefService;
    @Mock private StripLayoutTrailingButtonsObserver mObserver;
    @Mock private ChromeAndroidTaskTracker mTaskTracker;
    @Mock private ChromeAndroidTask mTask;
    @Mock private ActorKeyedService mActorKeyedService;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private GlicNudgeDelegateBridge.Natives mGlicNudgeDelegateBridgeJniMock;
    private final OneshotSupplierImpl<SideUiStateProvider> mSideUiStateProviderSupplier =
            new OneshotSupplierImpl<>();
    @Mock private SideUiStateProvider mSideUiStateProvider;
    @Captor private ArgumentCaptor<List<Animator>> mAnimatorsListCaptor;

    private Activity mActivity;
    private StripLayoutTrailingButtonsCoordinator mCoordinator;
    private TintedCompositorButton mModelSelectorButton;
    private TintedCompositorTextButton mGlicButton;
    private TintedCompositorButton mGlicDismissButton;
    private TintedCompositorTextButton mGlicActorButton;
    private static final float BUTTON_WIDTH = 42.0f;
    private final long mBwiPtr = 123L;
    private boolean mIsIncognito;
    private boolean mGlicIphShowing;

    @Before
    public void setUp() {
        GlicEnabling.setEnabledForTesting(ChromeFeatureList.isEnabled(ChromeFeatureList.GLIC));
        GlicNudgeDelegateBridgeJni.setInstanceForTesting(mGlicNudgeDelegateBridgeJniMock);

        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarJniMock);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP)).thenReturn(true);

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

        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);

        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarJniMock);
        when(mPrefChangeRegistrarJniMock.init(any(), any())).thenReturn(1L);
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
                        () -> {},
                        (isFocused, view) -> {},
                        mGlicClickHandler,
                        /* glicKeyboardFocusHandler= */ null,
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
        if (mCoordinator != null) {
            mCoordinator.destroy();
        }
    }

    @Test
    public void testModelSelectorButtonDrawX() {
        // Set model selector button position.
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        when(mPrefService.getBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP)).thenReturn(false);
        mCoordinator.onSizeChanged(
                /* width= */ 800f,
                /* rightPadding= */ 0f,
                /* leftPadding= */ 0f,
                /* topPadding= */ 0f);

        // Verify model selector button x-position.
        // width(800) - endPadding(8) - width(32) = 760
        assertEquals(
                "Model selector button x-position is not as expected",
                760.f,
                mModelSelectorButton.getDrawX(),
                0.0);
    }

    @Test
    public void testModelSelectorButtonDrawX_Rtl() {
        // Set model selector button position.
        LocalizationUtils.setRtlForTesting(true);
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        when(mPrefService.getBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP)).thenReturn(false);
        mCoordinator.onSizeChanged(
                /* width= */ 800f,
                /* rightPadding= */ 0f,
                /* leftPadding= */ 0f,
                /* topPadding= */ 0f);

        // Verify model selector button position.
        assertEquals(
                "Model selector button x-position is not as expected",
                8.f, // BUTTON_END_PADDING
                mModelSelectorButton.getDrawX(),
                0.0);
    }

    @Test
    public void testModelSelectorButtonDrawY() {
        // Set model selector button position.
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        mCoordinator.onSizeChanged(
                /* width= */ 800f,
                /* rightPadding= */ 0f,
                /* leftPadding= */ 0f,
                /* topPadding= */ 0f);

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
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        mCoordinator.onSizeChanged(
                /* width= */ 800f,
                /* rightPadding= */ 0f,
                /* leftPadding= */ 0f,
                /* topPadding= */ 0f);

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
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        mCoordinator.onSizeChanged(
                /* width= */ 800f,
                /* rightPadding= */ 0f,
                /* leftPadding= */ 0f,
                /* topPadding= */ 0f);

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
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        mCoordinator.onSizeChanged(
                /* width= */ 800f,
                /* rightPadding= */ 0f,
                /* leftPadding= */ 0f,
                /* topPadding= */ 0f);

        // Verify model selector button is in pressed state, not hover state, when click is from
        // mouse.
        mCoordinator.onDown(mModelSelectorButton.getDrawX() + 1, 0, 1);
        assertFalse(
                "Model selector button should not be hovered", mModelSelectorButton.isHovered());
        assertTrue(
                "Model selector button should be pressed from mouse",
                mModelSelectorButton.isPressedFromMouse());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.GLIC)
    public void testGlicButtonDisabled() {
        assertNull("Glic button should not be created.", mGlicButton);
    }

    @Test
    public void testGlicButtonEnabled() {
        assertNotNull("Glic button should be created.", mGlicButton);
    }

    @Test
    public void testGlicButton_VisibleInIncognito() {
        assertTrue("Glic button should be visible initially.", mCoordinator.shouldGlicBeVisible());

        mIsIncognito = true;

        assertTrue(
                "Glic button should be visible even when supplier indicates incognito window.",
                mCoordinator.shouldGlicBeVisible());
    }

    @Test
    public void testGlicButton_HiddenWhenSidePanelNotShowable() {
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
    }

    @Test
    public void testGlicActorButtonTextCollapsesOnSmallScreen() {
        assertNotNull("Actor button should be created.", mGlicActorButton);

        // Start with a large screen width >= 700
        mCoordinator.onSizeChanged(
                /* width= */ 1000f,
                /* rightPadding= */ 0f,
                /* leftPadding= */ 0f,
                /* topPadding= */ 0f);

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
    public void testSetGlicButtonText() {
        verifySetButtonText(mGlicButton, "Glic Text", /* isActor= */ false);
    }

    @Test
    public void testSetGlicActorButtonText() {
        showGlicActorButton();
        verifySetButtonText(mGlicActorButton, "Actor Text", /* isActor= */ true);
    }

    private void verifySetButtonText(
            TintedCompositorTextButton button, String text, boolean isActor) {
        assertNotNull("Button should be created.", button);

        // Start with no-text state button
        StripLayoutTrailingButtonsCoordinator coordinatorSpy = Mockito.spy(mCoordinator);
        boolean textChanged = button.getText() != null;
        button.setText(null);
        if (textChanged) {
            coordinatorSpy.updateButtonTextProperties(button);
            Mockito.verify(coordinatorSpy)
                    .startAnimations(mAnimatorsListCaptor.capture(), Mockito.any());
            for (Animator animator : mAnimatorsListCaptor.getValue()) {
                animator.end();
            }
            Mockito.clearInvocations(coordinatorSpy);
        }
        float initialWidth = button.getWidth();
        when(mLayerTitleCache.getUpdatedGlicButtonText(any(), anyBoolean(), anyBoolean()))
                .thenReturn(123);
        when(mLayerTitleCache.getButtonTextWidth(any())).thenReturn(100);

        // Set text
        button.setText(text);
        coordinatorSpy.updateButtonTextProperties(button);
        Mockito.verify(coordinatorSpy)
                .startAnimations(mAnimatorsListCaptor.capture(), Mockito.any());

        // Fast forward animations to completion
        for (Animator animator : mAnimatorsListCaptor.getValue()) {
            animator.end();
        }
        Mockito.clearInvocations(coordinatorSpy);

        // Assert the button has expanded in width
        verify(mLayerTitleCache, Mockito.atLeastOnce())
                .getUpdatedGlicButtonText(Mockito.eq(text), Mockito.eq(isActor), anyBoolean());
        assertTrue(
                "Button width should increase to accommodate text.",
                button.getWidth() > initialWidth);

        // Set text back to null
        button.setText(null);
        coordinatorSpy.updateButtonTextProperties(button);
        Mockito.verify(coordinatorSpy)
                .startAnimations(mAnimatorsListCaptor.capture(), Mockito.any());

        for (Animator animator : mAnimatorsListCaptor.getValue()) {
            animator.end();
        }

        // Assert the button has shrunk back to original width
        assertEquals(
                "Button width should return to original singular icon width.",
                initialWidth,
                button.getWidth(),
                MathUtils.EPSILON);
    }

    @Test
    public void testGlicButtonUnfocusedOpacity() {
        assertNotNull("Glic button should be created.", mGlicButton);

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
    }

    @Test
    public void testGlicDismissNudgeButton() {
        mCoordinator
                .getGlicNudgeDelegateForTesting()
                .onTriggerGlicNudgeUi("Glic Nudge Text", "", "");

        // Verify initial state: Dismiss button visible, Glic button text correct.
        assertNotNull("Dismiss button should exist", mGlicDismissButton);
        assertTrue("Dismiss button should be visible", mGlicDismissButton.isVisible());
        assertEquals("Glic text should match setup text", "Glic Nudge Text", mGlicButton.getText());

        // Simulate pressing the dismiss button.
        mGlicDismissButton.handleClick(0, 0, 0);

        // Verify dismiss button hides and Glic button text restores to default.
        assertFalse("Dismiss button should have hidden", mGlicDismissButton.isVisible());
        assertEquals(
                "Glic button text should have been restored to default",
                mActivity.getString(R.string.glic_button_entrypoint_ask_gemini_label),
                mGlicButton.getText());
    }

    @Test
    public void testOnTabModelSwitched() {
        // 1. Trigger the nudge in normal mode.
        mCoordinator
                .getGlicNudgeDelegateForTesting()
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
    public void testOnLongPress_OnGlicButton() {
        verifyGlicButtonContextMenuTriggered(/* viaSecondaryClick= */ false);
    }

    @Test
    public void testSecondaryClick_OnGlicButton() {
        verifyGlicButtonContextMenuTriggered(/* viaSecondaryClick= */ true);
    }

    private void verifyGlicButtonContextMenuTriggered(boolean viaSecondaryClick) {
        mCoordinator.onSizeChanged(
                /* width= */ 1000f,
                /* rightPadding= */ 10f,
                /* leftPadding= */ 10f,
                /* topPadding= */ 10f);

        float x = mGlicButton.getDrawX() + mGlicButton.getWidth() / 2;
        float y = mGlicButton.getDrawY() + mGlicButton.getHeight() / 2;

        boolean handled =
                viaSecondaryClick
                        ? mCoordinator.click(
                                0L, x, y, MotionEvent.BUTTON_SECONDARY, 0, /* tabWidthDp= */ 100f)
                        : mCoordinator.onLongPress(x, y, /* tabWidthDp= */ 100f);
        assertTrue("Context menu trigger should be handled.", handled);
        assertFalse(
                "Glic button should not be pressed after context menu is shown.",
                mGlicButton.isPressed());
        assertTrue("Glic context menu should be showing.", mCoordinator.isMenuShowing());
    }

    @Test
    public void testStandardClick_TrailingButtons() {
        showGlicActorButton();
        assertNotNull("Glic Actor button should be created.", mGlicActorButton);

        // 1. Test click routing on Glic Button coordinates
        float glicX = mGlicButton.getDrawX() + mGlicButton.getWidth() / 2;
        float glicY = mGlicButton.getDrawY() + mGlicButton.getHeight() / 2;
        boolean glicHandled = mCoordinator.click(0L, glicX, glicY, 0, 0, /* tabWidthDp= */ 100f);
        assertTrue("Click on Glic coordinates should be handled.", glicHandled);
        verify(mGlicClickHandler, Mockito.times(1))
                .onClick(
                        /* preventClose= */ false,
                        GlicKeyedService.GlicInvocationSource.TOP_CHROME_BUTTON);

        // 2. Test click routing on Glic Actor Button coordinates
        float actorX = mGlicActorButton.getDrawX() + mGlicActorButton.getWidth() / 2;
        float actorY = mGlicActorButton.getDrawY() + mGlicActorButton.getHeight() / 2;
        boolean actorHandled = mCoordinator.click(0L, actorX, actorY, 0, 0, /* tabWidthDp= */ 100f);
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
    public void testActorButtonStateChangedLifecycle() {
        StripLayoutTrailingButtonsCoordinator coordinatorSpy = Mockito.spy(mCoordinator);
        TintedCompositorTextButton actorButton = coordinatorSpy.getGlicActorButton();
        TintedCompositorTextButton glicButton = coordinatorSpy.getGlicButton();

        // Stub startAnimations to immediately complete synchronously.
        Mockito.doAnswer(
                        invocation -> {
                            AnimatorListenerAdapter listener = invocation.getArgument(1);
                            if (listener != null) {
                                listener.onAnimationEnd(null);
                            }
                            return null;
                        })
                .when(coordinatorSpy)
                .startAnimations(Mockito.any(), Mockito.any());

        // --- 1. Start State: Inactive ---
        assertFalse("Initially, Glic Actor button should be hidden.", actorButton.isVisible());
        assertEquals(
                "Initially, Glic primary button text should be default.",
                mActivity.getString(R.string.glic_button_entrypoint_ask_gemini_label),
                glicButton.getText());

        // --- 2. Transition: Active (task starts acting) ---
        Mockito.doReturn(true).when(coordinatorSpy).shouldGlicActorBeVisible();

        coordinatorSpy.onGlicActorButtonStateChanged(ButtonState.WORKING, false);

        // Verify actor button is shown (with no text) and primary button text is cleared.
        assertTrue("Actor button should become visible.", actorButton.isVisible());
        assertNull("Actor button text should be null in active state.", actorButton.getText());
        assertNull(
                "Primary Glic button text should be null when actor button is active.",
                glicButton.getText());

        // --- 3. Transition: Done (task finishes) ---
        coordinatorSpy.onGlicActorButtonStateChanged(ButtonState.DONE, false);

        // Verify actor button is still visible and text becomes "Done".
        assertTrue("Actor button should remain visible.", actorButton.isVisible());
        assertEquals(
                "Actor button text should become 'Task done'.",
                mActivity
                        .getResources()
                        .getQuantityString(R.plurals.actor_task_nudge_task_complete_label, 1),
                actorButton.getText());
        assertNull(
                "Primary Glic button text should remain null in done state.", glicButton.getText());

        // --- 4. Transition: Return to Inactive (task is dismissed/cancelled) ---
        Mockito.doReturn(false).when(coordinatorSpy).shouldGlicActorBeVisible();

        coordinatorSpy.onGlicActorButtonStateChanged(ButtonState.DEFAULT, false);

        // Verify actor button hides and primary Glic button text is restored.
        assertFalse("Actor button should collapse and hide.", actorButton.isVisible());
        assertEquals(
                "Primary Glic button text should be restored to default.",
                mActivity.getString(R.string.glic_button_entrypoint_ask_gemini_label),
                glicButton.getText());
    }

    @Test
    public void testGlicNudgeDelegate() {
        assertNotNull("Glic button should be created.", mGlicButton);
        assertNotNull("Glic dismiss button should be created.", mGlicDismissButton);

        GlicNudgeDelegate delegate = mCoordinator.getGlicNudgeDelegateForTesting();
        assertNotNull("Glic nudge delegate should be created.", delegate);
        assertFalse("Nudge should not be showing initially.", delegate.getIsShowingGlicNudge());

        // 1. Trigger the nudge via delegate method
        delegate.onTriggerGlicNudgeUi("Nudge Text", "", "");

        assertTrue("Nudge should be showing after triggering.", delegate.getIsShowingGlicNudge());
        assertEquals("Glic text should match trigger label.", "Nudge Text", mGlicButton.getText());
        assertTrue("Dismiss button should be visible.", mGlicDismissButton.isVisible());

        // 2. Hide the nudge via delegate method
        delegate.onHideGlicNudgeUi();

        assertFalse("Nudge should not be showing after hiding.", delegate.getIsShowingGlicNudge());
        assertFalse("Dismiss button should not be visible.", mGlicDismissButton.isVisible());
        assertEquals(
                "Glic text should have been restored to default.",
                mActivity.getString(R.string.glic_button_entrypoint_ask_gemini_label),
                mGlicButton.getText());
    }

    private void showGlicActorButton() {
        mCoordinator.setGlicActorButtonVisible(true, /* animate= */ false);
        mGlicActorButton.setWidth(BUTTON_WIDTH);
        mGlicActorButton.setOpacity(1.0f);
        mCoordinator.updateButtonPositions();
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
}
