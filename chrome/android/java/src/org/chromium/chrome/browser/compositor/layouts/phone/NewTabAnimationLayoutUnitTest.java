// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.compositor.layouts.Layout.ViewportMode;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.BlackHoleEventFilter;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayerJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayerJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link NewTabAnimationLayout}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 35)
@EnableFeatures({
    ChromeFeatureList.SENSITIVE_CONTENT,
    ChromeFeatureList.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS
})
public class NewTabAnimationLayoutUnitTest {
    private static final long FAKE_TIME = 0;
    private static final int CURRENT_TAB_ID = 321;
    private static final int NEW_TAB_ID = 123;
    private static final long FAKE_NATIVE_ADDRESS_1 = 498723734L;
    private static final long FAKE_NATIVE_ADDRESS_2 = 123210L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private SceneLayer.Natives mSceneLayerJni;
    @Mock private StaticTabSceneLayer.Natives mStaticTabSceneLayerJni;
    @Mock private LayoutUpdateHost mUpdateHost;
    @Mock private LayoutRenderHost mRenderHost;
    @Mock private TabContentManager mTabContentManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Tab mCurrentTab;
    @Mock private Tab mNewTab;
    @Mock private LayoutTab mLayoutTab;

    private NewTabAnimationLayout mNewTabAnimationLayout;
    private FrameLayout mAnimationHostView;

    @Before
    public void setUp() {
        SceneLayerJni.setInstanceForTesting(mSceneLayerJni);
        StaticTabSceneLayerJni.setInstanceForTesting(mStaticTabSceneLayerJni);
        when(mSceneLayerJni.init(any()))
                .thenReturn(FAKE_NATIVE_ADDRESS_1)
                .thenReturn(FAKE_NATIVE_ADDRESS_2);
        doCallback(
                        /* index= */ 1,
                        (SceneLayer sceneLayer) -> {
                            sceneLayer.setNativePtr(0L);
                        })
                .when(mSceneLayerJni)
                .destroy(anyLong(), any());
        doAnswer(
                        invocation -> {
                            ((SceneLayer) invocation.getArguments()[0])
                                    .setNativePtr(FAKE_NATIVE_ADDRESS_1);
                            return FAKE_NATIVE_ADDRESS_1;
                        })
                .when(mStaticTabSceneLayerJni)
                .init(any());

        when(mTabModelSelector.getModelForTabId(anyInt())).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModelSelector.getTabById(CURRENT_TAB_ID)).thenReturn(mCurrentTab);
        when(mTabModelSelector.getTabById(NEW_TAB_ID)).thenReturn(mNewTab);
        when(mTabModel.getCount()).thenReturn(2);
        when(mTabModel.getTabAt(0)).thenReturn(mCurrentTab);
        when(mTabModel.getTabAt(1)).thenReturn(mNewTab);
        when(mTabModel.getTabById(CURRENT_TAB_ID)).thenReturn(mCurrentTab);
        when(mTabModel.getTabById(NEW_TAB_ID)).thenReturn(mNewTab);
        when(mCurrentTab.getId()).thenReturn(CURRENT_TAB_ID);
        when(mNewTab.getId()).thenReturn(NEW_TAB_ID);

        when(mLayoutTab.isInitFromHostNeeded()).thenReturn(true);
        when(mUpdateHost.createLayoutTab(anyInt(), anyBoolean(), anyFloat(), anyFloat()))
                .thenReturn(mLayoutTab);

        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    public void onActivity(Activity activity) {
        mAnimationHostView = new FrameLayout(activity);
        activity.setContentView(mAnimationHostView);
        mNewTabAnimationLayout =
                new NewTabAnimationLayout(activity, mUpdateHost, mRenderHost, mAnimationHostView);
        mNewTabAnimationLayout.setTabModelSelector(mTabModelSelector);
        mNewTabAnimationLayout.setTabContentManager(mTabContentManager);
        mNewTabAnimationLayout.onFinishNativeInitialization();
    }

    @After
    public void tearDown() {
        mNewTabAnimationLayout.destroy();
    }

    @Test
    public void testConstants() {
        assertEquals(
                mNewTabAnimationLayout.getViewportMode(),
                ViewportMode.USE_PREVIOUS_BROWSER_CONTROLS_STATE);
        assertTrue(mNewTabAnimationLayout.handlesTabCreating());
        assertFalse(mNewTabAnimationLayout.handlesTabClosing());
        assertThat(mNewTabAnimationLayout.getEventFilter(), instanceOf(BlackHoleEventFilter.class));
        assertThat(mNewTabAnimationLayout.getSceneLayer(), instanceOf(StaticTabSceneLayer.class));
        assertEquals(mNewTabAnimationLayout.getLayoutType(), LayoutType.SIMPLE_ANIMATION);
    }

    @Test
    public void testShowWithNativePage() {
        when(mTabModelSelector.getCurrentTab()).thenReturn(mCurrentTab);
        when(mCurrentTab.isNativePage()).thenReturn(true);

        mNewTabAnimationLayout.show(FAKE_TIME, /* animate= */ true);
        verify(mTabContentManager).cacheTabThumbnail(mCurrentTab);
    }

    @Test
    public void testShowWithoutNativePage() {
        // No tab.
        mNewTabAnimationLayout.show(FAKE_TIME, /* animate= */ true);

        // Tab is not native page.
        when(mTabModelSelector.getCurrentTab()).thenReturn(mCurrentTab);
        mNewTabAnimationLayout.show(FAKE_TIME, /* animate= */ true);

        verify(mTabContentManager, never()).cacheTabThumbnail(mCurrentTab);
    }

    @Test
    public void testDoneHiding() {
        mAnimationHostView.setContentSensitivity(View.CONTENT_SENSITIVITY_SENSITIVE);
        mNewTabAnimationLayout.setNextTabIdForTesting(NEW_TAB_ID);

        mNewTabAnimationLayout.doneHiding();
        verify(mTabModel).setIndex(1, TabSelectionType.FROM_USER);

        assertEquals(
                mAnimationHostView.getContentSensitivity(), View.CONTENT_SENSITIVITY_NOT_SENSITIVE);
    }

    @Test
    public void testOnTabCreating_ContentSensitivity() {
        when(mCurrentTab.getTabHasSensitiveContent()).thenReturn(true);

        mNewTabAnimationLayout.onTabCreating(CURRENT_TAB_ID);
        assertEquals(
                mAnimationHostView.getContentSensitivity(), View.CONTENT_SENSITIVITY_SENSITIVE);
    }

    @Test
    public void testOnTabCreated_ContentSensitivty() {
        when(mCurrentTab.getTabHasSensitiveContent()).thenReturn(true);

        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ false,
                /* originX= */ 0f,
                /* originY= */ 0f);
        assertEquals(
                mAnimationHostView.getContentSensitivity(), View.CONTENT_SENSITIVITY_SENSITIVE);
    }

    // TODO(crbug.com/40282469): Tests for forceAnimationToFinish, updateLayout, and
    // updateSceneLayer depend on the implementation of onTabCreated being finished so that
    // mLayoutTabs gets populated.
}
