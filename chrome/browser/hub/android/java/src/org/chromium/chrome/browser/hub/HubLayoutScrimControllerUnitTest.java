// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.app.Activity;
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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link HubLayoutScrimController}. */
@RunWith(BaseRobolectricTestRunner.class)
// TODO(crbug.com/419289558): Re-enable color surface feature flags
@Features.DisableFeatures({
    ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_UPDATE
})
public class HubLayoutScrimControllerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelArgumentCaptor;

    private Activity mActivity;
    private View mAnchorView;
    private ScrimManager mScrimManager;
    private ObservableSupplierImpl<Boolean> mIsIncognitoSupplier;
    private HubLayoutScrimController mScrimController;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;
        FrameLayout rootView = new FrameLayout(mActivity);
        mActivity.setContentView(rootView);

        mAnchorView = new View(mActivity);
        rootView.addView(mAnchorView);

        mScrimManager = spy(new ScrimManager(mActivity, rootView));

        mIsIncognitoSupplier = new ObservableSupplierImpl<>(false);

        mScrimController =
                new HubLayoutScrimController(
                        mScrimManager, () -> mAnchorView, mIsIncognitoSupplier);
    }

    @After
    public void tearDown() {
        mScrimManager.destroy();
    }

    @Test
    @SmallTest
    public void testShowAndHide() {
        assertFalse(mScrimManager.isShowingScrim());
        mScrimController.startShowingScrim();

        // This is not an exhaustive test of properties impacts as the ScrimTest already covers
        // this.
        assertTrue(mScrimManager.isShowingScrim());
        verify(mScrimManager).showScrim(mPropertyModelArgumentCaptor.capture());
        assertPropertyModel(mIsIncognitoSupplier.get());

        // Finish the animation.
        ShadowLooper.runUiThreadTasks();

        mScrimController.startHidingScrim();
        verify(mScrimManager).hideScrim(any(), eq(true), anyInt());
        ShadowLooper.runUiThreadTasks();

        assertFalse(mScrimManager.isShowingScrim());

        mIsIncognitoSupplier.set(true);
        assertPropertyModel(false);
    }

    @Test
    @SmallTest
    public void testShowForceAnimationToFinish() {
        mIsIncognitoSupplier.set(true);
        assertFalse(mScrimManager.isShowingScrim());

        mScrimController.startShowingScrim();
        assertTrue(mScrimManager.isShowingScrim());
        verify(mScrimManager).showScrim(mPropertyModelArgumentCaptor.capture());
        assertPropertyModel(mIsIncognitoSupplier.get());

        // Force the animation to finish.
        mScrimController.forceAnimationToFinish();
        verify(mScrimManager).forceAnimationToFinish(any());

        assertTrue(mScrimManager.isShowingScrim());

        mIsIncognitoSupplier.set(false);
        assertPropertyModel(mIsIncognitoSupplier.get());
    }

    private void assertPropertyModel(boolean isIncognito) {
        PropertyModel model = mPropertyModelArgumentCaptor.getValue();
        assertEquals(mAnchorView, model.get(ScrimProperties.ANCHOR_VIEW));
        assertFalse(model.get(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW));
        assertTrue(model.get(ScrimProperties.AFFECTS_STATUS_BAR));
        final @ColorInt int scrimColor =
                ChromeColors.getPrimaryBackgroundColor(mActivity, isIncognito);
        assertEquals(scrimColor, model.get(ScrimProperties.BACKGROUND_COLOR));
    }
}
