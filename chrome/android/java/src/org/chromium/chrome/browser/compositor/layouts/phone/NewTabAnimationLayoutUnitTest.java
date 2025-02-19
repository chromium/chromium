// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
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
import org.chromium.chrome.browser.tab.TabLaunchType;
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

    @Mock private ObservableSupplier<CompositorViewHolder> mCompositorViewHolderSupplier;
    @Mock private CompositorViewHolder mCompositorViewHolder;
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
        when(mCompositorViewHolderSupplier.get()).thenReturn(mCompositorViewHolder);

        when(mLayoutTab.isInitFromHostNeeded()).thenReturn(true);
        doAnswer(
                        invocation -> {
                            var args = invocation.getArguments();
                            return new LayoutTab((Integer) args[0], (Boolean) args[1], -1, -1);
                        })
                .when(mUpdateHost)
                .createLayoutTab(anyInt(), anyBoolean());

        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    public void onActivity(Activity activity) {
        mAnimationHostView = spy(new FrameLayout(activity));
        activity.setContentView(mAnimationHostView);
        mNewTabAnimationLayout =
                spy(
                        new NewTabAnimationLayout(
                                activity,
                                mUpdateHost,
                                mRenderHost,
                                mAnimationHostView,
                                mCompositorViewHolderSupplier));
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
                ViewportMode.USE_PREVIOUS_BROWSER_CONTROLS_STATE,
                mNewTabAnimationLayout.getViewportMode());
        assertTrue(mNewTabAnimationLayout.handlesTabCreating());
        assertFalse(mNewTabAnimationLayout.handlesTabClosing());
        assertThat(mNewTabAnimationLayout.getEventFilter(), instanceOf(BlackHoleEventFilter.class));
        assertThat(mNewTabAnimationLayout.getSceneLayer(), instanceOf(StaticTabSceneLayer.class));
        assertEquals(LayoutType.SIMPLE_ANIMATION, mNewTabAnimationLayout.getLayoutType());
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
                View.CONTENT_SENSITIVITY_NOT_SENSITIVE, mAnimationHostView.getContentSensitivity());
    }

    @Test
    public void testOnTabCreating_ContentSensitivity() {
        when(mCurrentTab.getTabHasSensitiveContent()).thenReturn(true);

        mNewTabAnimationLayout.onTabCreating(CURRENT_TAB_ID);
        assertEquals(
                View.CONTENT_SENSITIVITY_SENSITIVE, mAnimationHostView.getContentSensitivity());
    }

    @Test
    public void testOnTabCreated_ContentSensitivity() {
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
                View.CONTENT_SENSITIVITY_SENSITIVE, mAnimationHostView.getContentSensitivity());
        assertFalse(mNewTabAnimationLayout.isStartingToHide());
    }

    @Test
    public void testOnTabCreated_FromCollaborationBackgroundInGroup() {
        when(mNewTab.getLaunchType())
                .thenReturn(TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP);

        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ true,
                /* originX= */ 0f,
                /* originY= */ 0f);
        assertTrue(mNewTabAnimationLayout.isStartingToHide());

        mNewTabAnimationLayout.doneHiding();
        verify(mTabModel, never()).setIndex(anyInt(), anyInt());
    }

    @Test
    public void testOnTabCreated_tabCreatedInForeground() {
        LayoutTab[] layoutTabs = mNewTabAnimationLayout.getLayoutTabsToRender();
        assertNull(layoutTabs);
        verify(mAnimationHostView, never()).addView(any());

        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ false,
                /* originX= */ 0f,
                /* originY= */ 0f);

        layoutTabs = mNewTabAnimationLayout.getLayoutTabsToRender();
        assertEquals(2, layoutTabs.length);
        assertEquals(CURRENT_TAB_ID, layoutTabs[0].getId());
        assertEquals(NEW_TAB_ID, layoutTabs[1].getId());

        verify(mNewTabAnimationLayout, times(1)).forceNewTabAnimationToFinish();
        verify(mAnimationHostView, times(1)).addView(any());

        mNewTabAnimationLayout.getForegroundAnimatorSet().start();
        assertTrue(mNewTabAnimationLayout.isRunningAnimations());

        ShadowLooper.runUiThreadTasks();
        assertFalse(mNewTabAnimationLayout.isRunningAnimations());
        verify(mAnimationHostView, times(1)).removeView(any());
        verify(mTabModelSelector).selectModel(false);
        assertTrue(mNewTabAnimationLayout.isStartingToHide());
    }
    // TODO(crbug.com/40282469): Tests for forceAnimationToFinish, updateLayout, and
    // updateSceneLayer depend on the implementation of onTabCreated being finished so that
    // mLayoutTabs gets populated.
}
