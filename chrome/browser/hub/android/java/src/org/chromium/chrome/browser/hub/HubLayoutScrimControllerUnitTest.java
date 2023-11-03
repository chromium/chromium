// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Color;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link HubLayoutScrimController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubLayoutScrimControllerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;
    private FrameLayout mRootView;
    private View mAnchorView;

    @Mock private ScrimCoordinator.SystemUiScrimDelegate mScrimDelegate;
    private ScrimCoordinator mScrimCoordinator;

    private HubLayoutScrimController mScrimController;
    private boolean mIsIncognito;

    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelArgumentCaptor;

    @Before
    public void setUp() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mActivity = activity;
                            mRootView = new FrameLayout(mActivity);
                            mActivity.setContentView(mRootView);

                            mAnchorView = new View(mActivity);
                            mRootView.addView(mAnchorView);

                            mScrimCoordinator =
                                    spy(
                                            new ScrimCoordinator(
                                                    mActivity,
                                                    mScrimDelegate,
                                                    mRootView,
                                                    Color.RED));

                            mScrimController =
                                    new HubLayoutScrimController(
                                            mScrimCoordinator,
                                            () -> mAnchorView,
                                            () -> mIsIncognito);
                        });
    }

    @After
    public void tearDown() {
        mScrimCoordinator.destroy();
    }

    @Test
    @SmallTest
    public void testShowAndHide() {
        assertFalse(mScrimCoordinator.isShowingScrim());
        mScrimController.startShowingScrim();

        // This is not an exhaustive test of properties impacts as the ScrimTest already covers
        // this.
        assertTrue(mScrimCoordinator.isShowingScrim());
        verify(mScrimCoordinator).showScrim(mPropertyModelArgumentCaptor.capture());
        assertPropertyModel(mPropertyModelArgumentCaptor.getValue(), mIsIncognito);

        // Finish the animation.
        ShadowLooper.runUiThreadTasks();

        mScrimController.startHidingScrim();
        verify(mScrimCoordinator).hideScrim(eq(true), anyInt());
        ShadowLooper.runUiThreadTasks();

        assertFalse(mScrimCoordinator.isShowingScrim());
    }

    @Test
    @SmallTest
    public void testShowForceAnimationToFinish() {
        mIsIncognito = true;
        assertFalse(mScrimCoordinator.isShowingScrim());

        mScrimController.startShowingScrim();
        assertTrue(mScrimCoordinator.isShowingScrim());
        verify(mScrimCoordinator).showScrim(mPropertyModelArgumentCaptor.capture());
        assertPropertyModel(mPropertyModelArgumentCaptor.getValue(), mIsIncognito);

        // Force the animation to finish.
        mScrimController.forceAnimationToFinish();
        verify(mScrimCoordinator).forceAnimationToFinish();

        assertTrue(mScrimCoordinator.isShowingScrim());
    }

    private void assertPropertyModel(PropertyModel model, boolean isIncognito) {
        assertEquals(mAnchorView, model.get(ScrimProperties.ANCHOR_VIEW));
        assertFalse(model.get(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW));
        assertTrue(model.get(ScrimProperties.AFFECTS_STATUS_BAR));
        @ColorInt int scrimColor = ChromeColors.getPrimaryBackgroundColor(mActivity, isIncognito);
        assertEquals(scrimColor, model.get(ScrimProperties.BACKGROUND_COLOR));
    }
}
