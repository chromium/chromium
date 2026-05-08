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

import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
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
    @Mock private Runnable mGlicClickHandler;
    @Mock private View mToolbarContainerView;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Profile mProfile;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefChangeRegistrar.Natives mPrefChangeRegistrarJniMock;
    @Mock private PrefService mPrefService;
    @Mock private StripLayoutTrailingButtonsObserver mObserver;
    @Mock private ChromeAndroidTaskTracker mTaskTracker;
    @Mock private ChromeAndroidTask mTask;

    private Activity mActivity;
    private StripLayoutTrailingButtonsCoordinator mCoordinator;
    private TintedCompositorTextButton mGlicButton;
    private TintedCompositorButton mGlicDismissButton;
    private final long mBwiPtr = 123L;

    @Before
    public void setUp() {
        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarJniMock);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP)).thenReturn(true);

        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

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
                        mObserver);
        mCoordinator.onProfileAvailable(mProfile);
        mCoordinator.setLayerTitleCache(mLayerTitleCache);
        mGlicButton = mCoordinator.getGlicButton();
        if (mGlicButton != null) mGlicDismissButton = mGlicButton.getDismissButton();
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
    public void testSetGlicButtonText() {
        assertNotNull("Glic button should be created.", mGlicButton);

        // Start with with no-text state button
        mCoordinator.setGlicButtonText(null);
        float initialWidth = mGlicButton.getWidth();
        when(mLayerTitleCache.getUpdatedGlicButtonText(any())).thenReturn(123);
        when(mLayerTitleCache.getButtonTextWidth(any())).thenReturn(100);
        mCoordinator.setGlicButtonText("Glic Text");

        verify(mLayerTitleCache).getUpdatedGlicButtonText("Glic Text");
        assertTrue(
                "Glic button width should increase to accommodate text.",
                mGlicButton.getWidth() > initialWidth);

        mCoordinator.setGlicButtonText(null);

        assertEquals(
                "Glic button width should return to original singular icon width.",
                initialWidth,
                mGlicButton.getWidth(),
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
        mCoordinator.setGlicButtonText("Glic Nudge Text");
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

        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));

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
}
