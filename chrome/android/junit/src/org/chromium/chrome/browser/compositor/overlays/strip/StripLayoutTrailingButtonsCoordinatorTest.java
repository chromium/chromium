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
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

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
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorTextButton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlobalShowHideObserver;
import org.chromium.ui.base.TestActivity;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.GLIC)
public class StripLayoutTrailingButtonsCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LayoutUpdateHost mUpdateHost;
    @Mock private LayoutRenderHost mRenderHost;
    @Mock private LayerTitleCache mLayerTitleCache;
    @Mock private GlicKeyedService mGlicKeyedService;
    @Mock private Runnable mGlicClickHandler;

    private Activity mActivity;
    private StripLayoutTrailingButtonsCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    @After
    public void tearDown() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
        }
    }

    private void initializeTest() {
        mCoordinator =
                new StripLayoutTrailingButtonsCoordinator(
                        mActivity,
                        mUpdateHost,
                        mRenderHost,
                        mGlicClickHandler,
                        /* density= */ 1.0f,
                        /* stripEndPadding= */ 0.0f,
                        /* toolbarControlContainer= */ null,
                        /* keyboardFocusHandler= */ null,
                        /* isAppInDesktopWindow= */ false,
                        /* isTopResumedActivity= */ false,
                        mGlicKeyedService);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.GLIC)
    public void testGlicButtonDisabled() {
        initializeTest();
        assertNull("Glic button should not be created.", mCoordinator.getGlicButton());
    }

    @Test
    public void testGlicButtonEnabled() {
        initializeTest();
        assertNotNull("Glic button should be created.", mCoordinator.getGlicButton());
    }

    @Test
    public void testSetGlicButtonText() {
        initializeTest();
        assertNotNull("Glic button should be created.", mCoordinator.getGlicButton());

        float initialWidth = mCoordinator.getGlicButton().getWidth();
        when(mLayerTitleCache.getUpdatedGlicButtonText(any())).thenReturn(123);
        when(mLayerTitleCache.getTitleWidth(anyBoolean(), any())).thenReturn(100);

        mCoordinator.setGlicButtonText("Glic Text", mLayerTitleCache);

        verify(mLayerTitleCache).getUpdatedGlicButtonText("Glic Text");
        assertTrue(
                "Glic button width should increase to accommodate text.",
                mCoordinator.getGlicButton().getWidth() > initialWidth);

        mCoordinator.setGlicButtonText(null, mLayerTitleCache);

        assertEquals(
                "Glic button width should return to original singular icon width.",
                initialWidth,
                mCoordinator.getGlicButton().getWidth(),
                MathUtils.EPSILON);
    }

    @Test
    public void testGlicButtonUnfocusedOpacity() {
        initializeTest();
        TintedCompositorTextButton glicButton = mCoordinator.getGlicButton();
        assertNotNull("Glic button should be created.", glicButton);

        // Focused state
        mCoordinator.updateGlicButtonOpacity(
                /* isAppInDesktopWindow= */ true, /* isTopResumedActivity= */ true);
        assertEquals(
                "Glic button opacity should be 1.0 when focused in desktop windowing mode.",
                1.0f,
                glicButton.getOpacity(),
                MathUtils.EPSILON);

        // Unfocused state
        mCoordinator.updateGlicButtonOpacity(
                /* isAppInDesktopWindow= */ true, /* isTopResumedActivity= */ false);
        assertEquals(
                "Glic button opacity should be 0.65 when unfocused in desktop windowing mode.",
                0.65f,
                glicButton.getOpacity(),
                MathUtils.EPSILON);
    }

    @Test
    public void testGlicPressedState_GlicUiShowHide() {
        initializeTest();
        assertNotNull("Glic button should be created.", mCoordinator.getGlicButton());

        ArgumentCaptor<GlobalShowHideObserver> observerCaptor =
                ArgumentCaptor.forClass(GlobalShowHideObserver.class);
        Mockito.verify(mGlicKeyedService).addGlobalShowHideObserver(observerCaptor.capture());

        // Verify initial state: button is not pressed.
        assertFalse(
                "Glic button should not be pressed initially.",
                mCoordinator.getGlicButton().isPressed());

        // Simulate Glic UI opening event.
        observerCaptor.getValue().onGlobalShowHide(true);

        // Verify button is in pressed state.
        assertTrue(
                "Glic button should be pressed when UI is shown globally.",
                mCoordinator.getGlicButton().isPressed());

        // Simulate Glic UI hiding event.
        observerCaptor.getValue().onGlobalShowHide(false);

        // Verify button returns to non-pressed state.
        assertFalse(
                "Glic button should not be pressed when UI is hidden globally.",
                mCoordinator.getGlicButton().isPressed());
    }
}
