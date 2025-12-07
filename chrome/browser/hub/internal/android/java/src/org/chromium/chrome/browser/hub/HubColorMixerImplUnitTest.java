// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.hub.HubColorMixer.StateChange.HUB_CLOSED;
import static org.chromium.chrome.browser.hub.HubColorMixer.StateChange.HUB_SHOWN;
import static org.chromium.chrome.browser.hub.HubColorMixer.StateChange.TRANSLATE_DOWN_TABLET_ANIMATION_START;
import static org.chromium.chrome.browser.hub.HubColorMixer.StateChange.TRANSLATE_UP_TABLET_ANIMATION_END;

import android.graphics.Color;

import androidx.annotation.ColorInt;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.animation.AnimationHandler;
import org.chromium.ui.util.ColorUtils;

/** Unit tests for {@link HubColorMixerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubColorMixerImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Pane mPane1;
    @Mock private Pane mPane2;
    @Mock private HubViewColorBlend mColorBlend;

    private ObservableSupplierImpl<Boolean> mHubVisibilitySupplier = new ObservableSupplierImpl<>();
    private ObservableSupplierImpl<Pane> mFocusedPaneSupplier = new ObservableSupplierImpl<>();

    @Spy
    private HubColorBlendAnimatorSetHelper mAnimatorSetBuilder =
            new HubColorBlendAnimatorSetHelper();

    @Spy private AnimationHandler mAnimationHandler = new AnimationHandler();

    private HubColorMixerImpl mHubColorMixer;

    private void initialize(boolean isTablet) {
        mHubColorMixer =
                new HubColorMixerImpl(
                        mHubVisibilitySupplier,
                        mFocusedPaneSupplier,
                        mAnimatorSetBuilder,
                        mAnimationHandler,
                        HubColorMixerImplUnitTest::getBackgroundColorForTests,
                        isTablet);
        ShadowLooper.runUiThreadTasks();
    }

    @Before
    public void setUp() {
        when(mPane1.getColorScheme()).thenReturn(HubColorScheme.DEFAULT);
        when(mPane2.getColorScheme()).thenReturn(HubColorScheme.INCOGNITO);

        initialize(/* isTablet= */ false);
    }

    @Test
    public void testDestroy() {
        mHubVisibilitySupplier = spy(new ObservableSupplierImpl<>());
        mFocusedPaneSupplier = spy(new ObservableSupplierImpl<>());

        mHubColorMixer =
                new HubColorMixerImpl(
                        mHubVisibilitySupplier,
                        mFocusedPaneSupplier,
                        mAnimatorSetBuilder,
                        mAnimationHandler,
                        HubColorMixerImplUnitTest::getBackgroundColorForTests,
                        false);
        mHubColorMixer.destroy();

        verify(mHubVisibilitySupplier).removeObserver(any());
        verify(mFocusedPaneSupplier).removeObserver(any());
    }

    @Test
    public void testInit() {
        reset(mAnimatorSetBuilder);

        mHubVisibilitySupplier = spy(new ObservableSupplierImpl<>());
        mFocusedPaneSupplier = spy(new ObservableSupplierImpl<>());

        mHubColorMixer =
                new HubColorMixerImpl(
                        mHubVisibilitySupplier,
                        mFocusedPaneSupplier,
                        mAnimatorSetBuilder,
                        mAnimationHandler,
                        HubColorMixerImplUnitTest::getBackgroundColorForTests,
                        false);

        assertEquals(Color.TRANSPARENT, (int) mHubColorMixer.getOverviewColorSupplier().get());
        assertFalse(mHubColorMixer.getOverviewMode());

        verify(mHubVisibilitySupplier).addObserver(any());
        verify(mFocusedPaneSupplier).addObserver(any());
        verify(mAnimatorSetBuilder).registerBlend(any());
    }

    @Test
    public void testProcessStateChange_phone_show() {
        mHubColorMixer.processStateChange(HUB_SHOWN);
        assertTrue(mHubColorMixer.getOverviewMode());
    }

    @Test
    public void testProcessStateChange_phone_close() {
        mHubColorMixer.processStateChange(HUB_CLOSED);
        assertFalse(mHubColorMixer.getOverviewMode());
    }

    @Test
    public void testProcessStateChange_tablet_show() {
        initialize(/* isTablet= */ true);

        assertFalse(mHubColorMixer.getOverviewMode());
        mHubColorMixer.processStateChange(HUB_SHOWN);
        assertFalse(mHubColorMixer.getOverviewMode());
    }

    @Test
    public void testProcessStateChange_tablet_setColorForBlendBeforeAnimation() {
        initialize(/* isTablet= */ true);

        mHubColorMixer.registerBlend(mColorBlend);
        verify(mColorBlend, never()).createAnimationForTransition(anyInt(), anyInt());

        doNothing().when(mAnimationHandler).startAnimation(any());
        mFocusedPaneSupplier.set(mPane1);
        reset(mColorBlend);
        ShadowLooper.runUiThreadTasks();

        assertFalse(mHubColorMixer.getOverviewMode());
        mHubColorMixer.processStateChange(HUB_SHOWN);
        assertFalse(mHubColorMixer.getOverviewMode());

        verify(mColorBlend).createAnimationForTransition(anyInt(), anyInt());
    }

    @Test
    public void testProcessStateChange_tablet_close() {
        initialize(/* isTablet= */ true);

        enableOverviewMode();
        assertTrue(mHubColorMixer.getOverviewMode());

        mHubColorMixer.processStateChange(HUB_CLOSED);
        assertTrue(mHubColorMixer.getOverviewMode());
    }

    @Test
    public void testProcessStateChange_tablet_translateUpEnd() {
        initialize(/* isTablet= */ true);

        assertFalse(mHubColorMixer.getOverviewMode());

        mHubColorMixer.processStateChange(TRANSLATE_UP_TABLET_ANIMATION_END);
        assertTrue(mHubColorMixer.getOverviewMode());
    }

    @Test
    public void testProcessStateChange_tablet_translateDownStart() {
        initialize(/* isTablet= */ true);
        mHubColorMixer.processStateChange(TRANSLATE_DOWN_TABLET_ANIMATION_START);
        assertFalse(mHubColorMixer.getOverviewMode());
    }

    @Test
    public void testOnHubVisibilityChange_phone_visible() {
        mHubVisibilitySupplier.set(true);
        ShadowLooper.runUiThreadTasks();
        assertTrue(mHubColorMixer.getOverviewMode());
    }

    @Test
    public void testOnHubVisibilityChange_phone_hidden() {
        mHubVisibilitySupplier.set(false);
        ShadowLooper.runUiThreadTasks();
        assertFalse(mHubColorMixer.getOverviewMode());
    }

    @Test
    public void testOnHubVisibilityChange_tablet_visible() {
        initialize(/* isTablet= */ true);

        mHubVisibilitySupplier.set(true);
        ShadowLooper.runUiThreadTasks();
        assertFalse(mHubColorMixer.getOverviewMode());
    }

    @Test
    public void testOnHubVisibilityChange_tablet_hidden() {
        initialize(/* isTablet= */ true);

        mHubVisibilitySupplier.set(false);
        ShadowLooper.runUiThreadTasks();
        assertFalse(mHubColorMixer.getOverviewMode());
    }

    @Test
    public void testOnFocusedPaneChange_default() {
        enableOverviewMode();
        mFocusedPaneSupplier.set(mPane1);
        ShadowLooper.runUiThreadTasks();
        assertTrue(mHubColorMixer.getOverviewMode());

        ObservableSupplier<Integer> overviewColorSupplier =
                mHubColorMixer.getOverviewColorSupplier();
        @ColorInt Integer expectedColor = getBackgroundColorForTests(HubColorScheme.DEFAULT);
        assertEquals(expectedColor, overviewColorSupplier.get());
    }

    @Test
    public void testOnFocusedPaneChange_incognito() {
        enableOverviewMode();
        mFocusedPaneSupplier.set(mPane2);
        ShadowLooper.runUiThreadTasks();
        assertTrue(mHubColorMixer.getOverviewMode());

        ObservableSupplier<Integer> overviewColorSupplier =
                mHubColorMixer.getOverviewColorSupplier();
        @ColorInt Integer expectedColor = getBackgroundColorForTests(HubColorScheme.INCOGNITO);
        assertEquals(expectedColor, overviewColorSupplier.get());
    }

    @Test
    public void testEnableOverviewMode() {
        enableOverviewMode();
        ShadowLooper.runUiThreadTasks();
        assertTrue(mHubColorMixer.getOverviewMode());

        ObservableSupplier<Integer> overviewColorSupplier =
                mHubColorMixer.getOverviewColorSupplier();
        @ColorInt Integer expectedColor = getBackgroundColorForTests(HubColorScheme.DEFAULT);
        assertEquals(expectedColor, overviewColorSupplier.get());
    }

    @Test
    public void testDisableOverviewMode() {
        mFocusedPaneSupplier.set(mPane1);

        enableOverviewMode();
        disableOverviewMode();

        assertFalse(mHubColorMixer.getOverviewMode());
        @ColorInt Integer expectedColor = Color.TRANSPARENT;
        assertEquals(expectedColor, mHubColorMixer.getOverviewColorSupplier().get());
    }

    @Test
    public void testUpdateColorScheme() {
        enableOverviewMode();

        reset(mAnimatorSetBuilder, mAnimationHandler);
        mFocusedPaneSupplier.set(mPane1);
        ShadowLooper.runUiThreadTasks();
        verify(mAnimatorSetBuilder).setNewColorScheme(HubColorScheme.DEFAULT);
        verify(mAnimatorSetBuilder).setPreviousColorScheme(HubColorScheme.DEFAULT);
        verify(mAnimatorSetBuilder).build();
        verify(mAnimationHandler).startAnimation(any());

        reset(mAnimatorSetBuilder, mAnimationHandler);
        mFocusedPaneSupplier.set(mPane2);
        ShadowLooper.runUiThreadTasks();
        verify(mAnimatorSetBuilder).setNewColorScheme(HubColorScheme.INCOGNITO);
        verify(mAnimatorSetBuilder).setPreviousColorScheme(HubColorScheme.DEFAULT);
        verify(mAnimatorSetBuilder).build();
        verify(mAnimationHandler).startAnimation(any());
    }

    @Test
    public void testOverviewAlphaObserver_inOverviewMode() {
        ObservableSupplier<Integer> overviewColorSupplier =
                mHubColorMixer.getOverviewColorSupplier();

        enableOverviewMode();
        @ColorInt int expectedColor = overviewColorSupplier.get();
        mHubColorMixer.getOverviewModeAlphaObserver().accept(0.5f);
        expectedColor = ColorUtils.setAlphaComponentWithFloat(expectedColor, 0.5f);
        ShadowLooper.runUiThreadTasks();

        assertEquals(
                Integer.valueOf(expectedColor), mHubColorMixer.getOverviewColorSupplier().get());
    }

    @Test
    public void testOverviewAlphaObserver_notInOverviewMode() {
        ObservableSupplier<Integer> overviewColorSupplier =
                mHubColorMixer.getOverviewColorSupplier();

        @ColorInt int expectedColor = overviewColorSupplier.get();
        mHubColorMixer.getOverviewModeAlphaObserver().accept(0.5f);
        expectedColor = ColorUtils.setAlphaComponentWithFloat(expectedColor, 0.5f);
        ShadowLooper.runUiThreadTasks();

        assertNotEquals(Integer.valueOf(expectedColor), overviewColorSupplier.get());
    }

    @Test
    public void testRegisterBlend() {
        reset(mAnimatorSetBuilder, mAnimationHandler);

        mHubColorMixer.registerBlend(mColorBlend);
        verify(mAnimatorSetBuilder).registerBlend(mColorBlend);

        verify(mColorBlend, never()).createAnimationForTransition(anyInt(), anyInt());
    }

    @Test
    public void testRegisterBlend_update() {
        doNothing().when(mAnimationHandler).startAnimation(any());
        mHubColorMixer.registerBlend(mColorBlend);
        enableOverviewMode();
        verify(mAnimatorSetBuilder).registerBlend(mColorBlend);

        reset(mAnimatorSetBuilder, mAnimationHandler, mColorBlend);
        doNothing().when(mAnimationHandler).startAnimation(any());

        mFocusedPaneSupplier.set(mPane2);
        ShadowLooper.runUiThreadTasks();
        verify(mColorBlend).createAnimationForTransition(anyInt(), anyInt());
        verify(mAnimationHandler).startAnimation(any());
    }

    private void enableOverviewMode() {
        mHubColorMixer.processStateChange(TRANSLATE_UP_TABLET_ANIMATION_END);
    }

    private void disableOverviewMode() {
        mHubColorMixer.processStateChange(TRANSLATE_DOWN_TABLET_ANIMATION_START);
    }

    private static @ColorInt int getBackgroundColorForTests(@HubColorScheme int colorScheme) {
        return switch (colorScheme) {
            case HubColorScheme.DEFAULT -> Color.BLUE;
            case HubColorScheme.INCOGNITO -> Color.RED;
            default -> {
                fail("Should never be called.");
                yield Color.TRANSPARENT;
            }
        };
    }
}
