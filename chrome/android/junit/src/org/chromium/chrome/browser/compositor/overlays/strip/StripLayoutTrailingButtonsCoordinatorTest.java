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

import android.app.Activity;
import android.view.MotionEvent;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
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
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlobalShowHideObserver;
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

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.GLIC)
public class StripLayoutTrailingButtonsCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LayoutUpdateHost mUpdateHost;
    @Mock private LayoutRenderHost mRenderHost;
    @Mock private LayerTitleCache mLayerTitleCache;
    @Mock private GlicKeyedService mGlicKeyedService;
    @Mock private Callback<Boolean> mGlicClickHandler;
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

    private Activity mActivity;
    private StripLayoutTrailingButtonsCoordinator mCoordinator;
    private TintedCompositorTextButton mGlicButton;
    private TintedCompositorButton mGlicDismissButton;
    private TintedCompositorTextButton mGlicActorButton;
    private final long mBwiPtr = 123L;
    private boolean mIsIncognito;

    @Before
    public void setUp() {
        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarJniMock);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP)).thenReturn(true);

        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);
        when(mActorKeyedService.getActiveTasks()).thenReturn(java.util.Collections.emptyList());

        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));

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
                        /* stripEndPadding= */ 0.0f,
                        mToolbarContainerView,
                        /* keyboardFocusHandler= */ null,
                        /* isAppInDesktopWindow= */ false,
                        /* isTopResumedActivity= */ false,
                        mGlicKeyedService,
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
        verifySetButtonText(mGlicActorButton, "Actor Text", /* isActor= */ true);
    }

    private void verifySetButtonText(
            TintedCompositorTextButton button, String text, boolean isActor) {
        assertNotNull("Button should be created.", button);

        // Start with no-text state button
        mCoordinator.setGlicButtonText(null, isActor);
        float initialWidth = button.getWidth();
        when(mLayerTitleCache.getUpdatedGlicButtonText(any(), anyBoolean())).thenReturn(123);
        when(mLayerTitleCache.getButtonTextWidth(any())).thenReturn(100);

        mCoordinator.setGlicButtonText(text, isActor);

        verify(mLayerTitleCache).getUpdatedGlicButtonText(text, isActor);
        assertTrue(
                "Button width should increase to accommodate text.",
                button.getWidth() > initialWidth);

        mCoordinator.setGlicButtonText(null, isActor);

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
    public void testGlicHighlightedState_GlicUiShowHide() {
        assertNotNull("Glic button should be created.", mGlicButton);

        ArgumentCaptor<GlobalShowHideObserver> observerCaptor =
                ArgumentCaptor.forClass(GlobalShowHideObserver.class);
        Mockito.verify(mGlicKeyedService).addGlobalShowHideObserver(observerCaptor.capture());

        // Verify initial state: button is not highlighted.
        assertFalse(
                "Glic button should not be highlighted initially.", mGlicButton.isHighlighted());

        // Simulate Glic UI opening event.
        when(mGlicKeyedService.isPanelShowingForBrowser(mBwiPtr)).thenReturn(true);
        observerCaptor.getValue().onGlobalShowHide();

        // Verify button is in highlighted state.
        assertTrue(
                "Glic button should be highlighted when UI is shown globally.",
                mGlicButton.isHighlighted());

        // Simulate Glic UI hiding event.
        when(mGlicKeyedService.isPanelShowingForBrowser(mBwiPtr)).thenReturn(false);
        observerCaptor.getValue().onGlobalShowHide();

        // Verify button returns to non-highlighted state.
        assertFalse(
                "Glic button should not be highlighted when UI is hidden globally.",
                mGlicButton.isHighlighted());
    }

    @Test
    public void testGlicHighlightedState_WindowPtrZero() {
        assertNotNull("Glic button should be created.", mGlicButton);

        ArgumentCaptor<GlobalShowHideObserver> observerCaptor =
                ArgumentCaptor.forClass(GlobalShowHideObserver.class);
        Mockito.verify(mGlicKeyedService).addGlobalShowHideObserver(observerCaptor.capture());

        // Simulate getNativeBrowserWindowPtr returning 0
        when(mTask.getNativeBrowserWindowPtr(any(), any())).thenReturn(0L);

        // Simulate Glic UI opening event.
        when(mGlicKeyedService.isPanelShowingForBrowser(mBwiPtr)).thenReturn(true);
        observerCaptor.getValue().onGlobalShowHide();

        // Verify button is NOT highlighted because ptr was 0.
        assertFalse(
                "Glic button should not be highlighted when window ptr is 0.",
                mGlicButton.isHighlighted());
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
        mCoordinator.setGlicActorButtonVisible(true);
        assertNotNull("Glic Actor button should be created.", mGlicActorButton);

        // 1. Test click routing on Glic Button coordinates
        float glicX = mGlicButton.getDrawX() + mGlicButton.getWidth() / 2;
        float glicY = mGlicButton.getDrawY() + mGlicButton.getHeight() / 2;
        boolean glicHandled = mCoordinator.click(0L, glicX, glicY, 0, 0, /* tabWidthDp= */ 100f);
        assertTrue("Click on Glic coordinates should be handled.", glicHandled);
        verify(mGlicClickHandler, Mockito.times(1)).onResult(/* result= */ false);

        // 2. Test click routing on Glic Actor Button coordinates
        float actorX = mGlicActorButton.getDrawX() + mGlicActorButton.getWidth() / 2;
        float actorY = mGlicActorButton.getDrawY() + mGlicActorButton.getHeight() / 2;
        boolean actorHandled = mCoordinator.click(0L, actorX, actorY, 0, 0, /* tabWidthDp= */ 100f);
        assertTrue("Click on Glic Actor coordinates should be handled.", actorHandled);
    }

    @Test
    public void testOnDown_TrailingButtons() {
        assertNotNull("Glic button should be created.", mGlicButton);
        mCoordinator.setGlicActorButtonVisible(true);
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
        mCoordinator.setGlicActorButtonVisible(true);
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
}
