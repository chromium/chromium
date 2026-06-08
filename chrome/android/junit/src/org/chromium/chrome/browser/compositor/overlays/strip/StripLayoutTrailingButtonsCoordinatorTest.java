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

import org.chromium.base.MathUtils;
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
import org.chromium.chrome.browser.glic.GlicPrefNames;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskTracker;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefChangeRegistrarJni;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.util.Collections;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.GLIC)
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
    @Captor private ArgumentCaptor<List<Animator>> mAnimatorsListCaptor;

    private Activity mActivity;
    private StripLayoutTrailingButtonsCoordinator mCoordinator;
    private TintedCompositorTextButton mGlicButton;
    private TintedCompositorButton mGlicDismissButton;
    private TintedCompositorTextButton mGlicActorButton;
    private static final float BUTTON_WIDTH = 42.0f;
    private final long mBwiPtr = 123L;
    private boolean mIsIncognito;

    @Before
    public void setUp() {
        GlicEnabling.setEnabledForTesting(ChromeFeatureList.isEnabled(ChromeFeatureList.GLIC));

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
        when(mToolbarContainerView.getRootView()).thenReturn(mToolbarContainerView);
        when(mToolbarContainerView.getResources()).thenReturn(mActivity.getResources());

        when(mTaskTracker.get(anyInt())).thenReturn(mTask);
        when(mTask.getNativeBrowserWindowPtr(any(), any())).thenReturn(mBwiPtr);

        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);

        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarJniMock);
        when(mPrefChangeRegistrarJniMock.init(any(), any())).thenReturn(1L);

        mCoordinator =
                new StripLayoutTrailingButtonsCoordinator(
                        mActivity,
                        mUpdateHost,
                        mRenderHost,
                        mWindowAndroid,
                        mGlicClickHandler,
                        /* density= */ 1.0f,
                        mToolbarContainerView,
                        /* keyboardFocusHandler= */ null,
                        /* isAppInDesktopWindow= */ false,
                        /* isTopResumedActivity= */ false,
                        mTaskTracker,
                        () -> mIsIncognito,
                        () -> null,
                        mObserver);
        mCoordinator.onProfileAvailable(mProfile);
        mCoordinator.setLayerTitleCache(mLayerTitleCache);
        mCoordinator.onSizeChanged(1000.f, 0.f, 0.f, 0.f);
        mGlicButton = mCoordinator.getGlicButton();
        if (mGlicButton != null) mGlicDismissButton = mGlicButton.getDismissButton();
        mGlicActorButton = mCoordinator.getGlicActorButton();
    }

    @After
    public void tearDown() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
        }
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
    public void testGlicButton_HiddenInIncognito() {
        assertTrue("Glic button should be visible initially.", mCoordinator.shouldGlicBeVisible());

        mIsIncognito = true;

        assertFalse(
                "Glic button should be hidden when supplier indicates incognito window.",
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
        when(mLayerTitleCache.getUpdatedGlicButtonText(any(), anyBoolean())).thenReturn(123);
        when(mLayerTitleCache.getButtonTextWidth(any())).thenReturn(100);
        mCoordinator.setGlicButtonText("Actor Text", /* isActor= */ true);
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
        when(mLayerTitleCache.getUpdatedGlicButtonText(any(), anyBoolean())).thenReturn(123);
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
        verify(mLayerTitleCache, Mockito.atLeastOnce()).getUpdatedGlicButtonText(text, isActor);
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

        // Unfocused state
        mCoordinator.updateGlicButtonOpacity(
                /* isAppInDesktopWindow= */ true, /* isTopResumedActivity= */ false);
        assertEquals(
                "Glic button opacity should be 0.65 when unfocused in desktop windowing mode.",
                0.65f,
                mGlicButton.getOpacity(),
                MathUtils.EPSILON);
    }

    @Test
    public void testGlicDismissNudgeButton() {
        mCoordinator.setGlicButtonText("Glic Nudge Text", /* isActor= */ false);
        mCoordinator.setGlicDismissNudgeButtonVisible(true);

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
    public void testOnLongPress_OnGlicButton() {
        mCoordinator.onSizeChanged(
                /* width= */ 1000f,
                /* rightPadding= */ 10f,
                /* leftPadding= */ 10f,
                /* topPadding= */ 10f);

        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));

        float x = mGlicButton.getDrawX() + mGlicButton.getWidth() / 2;
        float y = mGlicButton.getDrawY() + mGlicButton.getHeight() / 2;

        boolean handled = mCoordinator.onLongPress(x, y, /* tabWidthDp= */ 100f);
        assertTrue(handled);
        assertFalse(
                "Glic button should not be pressed after long press menu is shown.",
                mGlicButton.isPressed());
        assertTrue("Glic context menu should be showing", mCoordinator.isMenuShowing());
    }

    @Test
    // TODO(crbug.com/483475735): Combine into testSecondaryClick after launch
    public void testSecondaryClick_OnGlicButton() {
        mCoordinator.onSizeChanged(
                /* width= */ 1000f,
                /* rightPadding= */ 10f,
                /* leftPadding= */ 10f,
                /* topPadding= */ 10f);

        float x = mGlicButton.getDrawX() + mGlicButton.getWidth() / 2;
        float y = mGlicButton.getDrawY() + mGlicButton.getHeight() / 2;

        boolean handled =
                mCoordinator.click(
                        0L, x, y, MotionEvent.BUTTON_SECONDARY, 0, /* tabWidthDp= */ 100f);
        assertTrue(handled);
        assertFalse(
                "Glic button should not be pressed after long press menu is shown.",
                mGlicButton.isPressed());
        assertTrue("Glic context menu should be showing", mCoordinator.isMenuShowing());
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

        when(mLayerTitleCache.getUpdatedGlicButtonText(any(), anyBoolean())).thenReturn(123);
        when(mLayerTitleCache.getButtonTextWidth(any())).thenReturn(100);

        // Create a unified spy of the coordinator for sequential transition verification
        StripLayoutTrailingButtonsCoordinator coordinatorSpy = Mockito.spy(mCoordinator);

        // 1. Test Glic Button Expansion Transition (Simulating contextual cueing nudge)
        coordinatorSpy.setGlicDismissNudgeButtonVisible(true);
        coordinatorSpy.setGlicButtonText("Glic Nudge", /* isActor= */ false);
        coordinatorSpy.updateButtonTextProperties(mGlicButton);
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
        coordinatorSpy.setGlicDismissNudgeButtonVisible(false);
        coordinatorSpy.setGlicButtonText(null, /* isActor= */ false);
        coordinatorSpy.updateButtonTextProperties(mGlicButton);
        Mockito.verify(coordinatorSpy, Mockito.atLeastOnce())
                .startAnimations(mAnimatorsListCaptor.capture(), Mockito.any());
        assertEquals(
                "Glic button shrink transition should queue 2 animators concurrently (width,"
                        + " opacity).",
                2,
                mAnimatorsListCaptor.getValue().size());

        Mockito.clearInvocations(coordinatorSpy);

        // 3. Test Glic Actor Button Expansion Transition (Simulating actor task nudge)
        coordinatorSpy.setGlicButtonText("Actor Nudge", /* isActor= */ true);
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
                "Actor button text should become 'Done'.",
                mActivity.getString(R.string.glic_button_status_done),
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

    private void showGlicActorButton() {
        mCoordinator.setGlicActorButtonVisible(true, /* animate= */ false);
        mGlicActorButton.setWidth(BUTTON_WIDTH);
        mGlicActorButton.setOpacity(1.0f);
        mCoordinator.updateGlicButtonPosition();
    }
}
