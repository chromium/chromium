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
import android.graphics.Point;
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

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.Layout.ViewportMode;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.BlackHoleEventFilter;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayerJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.hub.ShrinkExpandImageView;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayerJni;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.CustomTabCount;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.top.ToggleTabStackButton;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;

/** Unit tests for {@link NewTabAnimationLayout}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 35)
@EnableFeatures({
    ChromeFeatureList.SENSITIVE_CONTENT,
    ChromeFeatureList.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS
})
public class NewTabAnimationLayoutUnitTest {
    private static final long FAKE_TIME = 0;
    private static final @TabId int CURRENT_TAB_ID = 321;
    private static final @TabId int NEW_TAB_ID = 123;
    private static final long FAKE_NATIVE_ADDRESS_1 = 498723734L;
    private static final long FAKE_NATIVE_ADDRESS_2 = 123210L;
    private static final Point sPoint = new Point(-1, -1);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private CompositorViewHolder mCompositorViewHolder;
    @Mock private ToolbarManager mToolbarManager;
    @Mock private CustomTabCount mCustomTabCount;
    @Mock private BrowserControlsManager mBrowserControlsManager;
    @Mock private BrowserStateBrowserControlsVisibilityDelegate mBrowserVisibilityDelegate;
    @Mock private SceneLayer.Natives mSceneLayerJni;
    @Mock private StaticTabSceneLayer.Natives mStaticTabSceneLayerJni;
    @Mock private LayoutUpdateHost mUpdateHost;
    @Mock private LayoutRenderHost mRenderHost;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private TabContentManager mTabContentManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Tab mCurrentTab;
    @Mock private Tab mNewTab;
    @Mock private LayoutTab mLayoutTab;
    @Mock private ToggleTabStackButton mTabSwitcherButton;
    @Mock private View mToolbar;
    @Mock private NewTabPage mNtp;

    private final ObservableSupplierImpl<Tab> mCurrentTabSupplier = new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<CompositorViewHolder> mCompositorViewHolderSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mScrimVisibilitySupplier =
            new ObservableSupplierImpl<>();
    private NewTabAnimationLayout mNewTabAnimationLayout;
    private FrameLayout mContentContainer;
    private FrameLayout mAnimationHostView;
    private UserDataHost mUserDataHost;
    private int mToken;

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

        when(mTabModelSelector.getCurrentTabSupplier()).thenReturn(mCurrentTabSupplier);
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
        mUserDataHost = new UserDataHost();
        when(mCurrentTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mNewTab.getId()).thenReturn(NEW_TAB_ID);
        when(mNtp.getLastTouchPosition()).thenReturn(sPoint);
        when(mBrowserControlsManager.getBrowserVisibilityDelegate())
                .thenReturn(mBrowserVisibilityDelegate);
        mToken = 0;
        when(mBrowserVisibilityDelegate.showControlsPersistent())
                .thenAnswer(invocation -> mToken++);
        when(mToolbarManager.getCustomTabCount()).thenReturn(mCustomTabCount);
        mCompositorViewHolderSupplier.set(mCompositorViewHolder);
        mScrimVisibilitySupplier.set(false);
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
        mContentContainer = new FrameLayout(activity);
        mAnimationHostView = spy(new FrameLayout(activity));
        activity.setContentView(mAnimationHostView);
        mNewTabAnimationLayout =
                spy(
                        new NewTabAnimationLayout(
                                activity,
                                mUpdateHost,
                                mRenderHost,
                                mLayoutStateProvider,
                                mContentContainer,
                                mCompositorViewHolderSupplier,
                                mAnimationHostView,
                                mToolbarManager,
                                mBrowserControlsManager,
                                mScrimVisibilitySupplier));
        mNewTabAnimationLayout.setTabModelSelector(mTabModelSelector);
        mNewTabAnimationLayout.setTabContentManager(mTabContentManager);
        when(mAnimationHostView.findViewById(R.id.tab_switcher_button))
                .thenReturn(mTabSwitcherButton);
        when(mAnimationHostView.findViewById(R.id.toolbar)).thenReturn(mToolbar);
        when(mAnimationHostView.getWidth()).thenReturn(40);
        when(mAnimationHostView.getHeight()).thenReturn(40);
        mNewTabAnimationLayout.onFinishNativeInitialization();
        mNewTabAnimationLayout.setRunOnNextLayoutImmediatelyForTesting(true);
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
        mContentContainer.setContentSensitivity(View.CONTENT_SENSITIVITY_SENSITIVE);
        mNewTabAnimationLayout.setNextTabIdForTesting(NEW_TAB_ID);

        mNewTabAnimationLayout.doneHiding();
        verify(mTabModel).setIndex(1, TabSelectionType.FROM_USER);

        assertEquals(
                View.CONTENT_SENSITIVITY_NOT_SENSITIVE, mContentContainer.getContentSensitivity());
    }

    @Test
    public void testOnTabCreating_ContentSensitivity() {
        when(mCurrentTab.getTabHasSensitiveContent()).thenReturn(true);

        mNewTabAnimationLayout.onTabCreating(CURRENT_TAB_ID);
        assertEquals(View.CONTENT_SENSITIVITY_SENSITIVE, mContentContainer.getContentSensitivity());
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
        assertEquals(View.CONTENT_SENSITIVITY_SENSITIVE, mContentContainer.getContentSensitivity());
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
        assertTrue(mNewTabAnimationLayout.isRunningAnimations());
        verify(mAnimationHostView, times(1)).addView(any(ShrinkExpandImageView.class));

        ShadowLooper.runUiThreadTasks();

        assertFalse(mNewTabAnimationLayout.isRunningAnimations());
        verify(mAnimationHostView, times(1)).removeView(any(ShrinkExpandImageView.class));
        verify(mTabModelSelector).selectModel(false);
        assertTrue(mNewTabAnimationLayout.isStartingToHide());
    }

    @Test
    public void testOnTabCreated_tabCreatedInBackground() {
        LayoutTab[] layoutTabs = mNewTabAnimationLayout.getLayoutTabsToRender();
        assertNull(layoutTabs);
        verify(mAnimationHostView, never()).addView(any());

        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ true,
                /* originX= */ 0f,
                /* originY= */ 0f);

        layoutTabs = mNewTabAnimationLayout.getLayoutTabsToRender();
        assertEquals(1, layoutTabs.length);
        assertEquals(CURRENT_TAB_ID, layoutTabs[0].getId());
        verify(mNewTabAnimationLayout, times(1)).forceNewTabAnimationToFinish();
        assertTrue(mNewTabAnimationLayout.isStartingToHide());
        verify(mBrowserVisibilityDelegate, times(1)).showControlsPersistent();
        verify(mAnimationHostView, times(1)).addView(any(NewBackgroundTabAnimationHostView.class));

        ShadowLooper.runUiThreadTasks();

        assertFalse(mNewTabAnimationLayout.isRunningAnimations());
        verify(mAnimationHostView, times(1))
                .removeView(any(NewBackgroundTabAnimationHostView.class));
        verify(mTabModelSelector, never()).selectModel(false);
        verify(mBrowserVisibilityDelegate, times(1)).releasePersistentShowingToken(0);
    }

    @Test
    public void testOnTabCreated_tabCreatedInBackground_ntpToken() {
        when(mCurrentTab.getUrl()).thenReturn(new GURL("chrome://newtab"));
        when(mCurrentTab.getNativePage()).thenReturn(mNtp);

        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ true,
                /* originX= */ 0f,
                /* originY= */ 0f);

        ShadowLooper.runUiThreadTasks();

        verify(mBrowserVisibilityDelegate, never()).showControlsPersistent();
    }

    @Test
    public void testOnTabCreated_tabCreatedInBackground_animationHaltToken() {
        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ true,
                /* originX= */ 0f,
                /* originY= */ 0f);

        verify(mBrowserVisibilityDelegate, times(1)).showControlsPersistent();

        // Halt animation with second animation
        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ true,
                /* originX= */ 0f,
                /* originY= */ 0f);

        verify(mBrowserVisibilityDelegate, times(1)).releasePersistentShowingToken(0);
        verify(mBrowserVisibilityDelegate, times(2)).showControlsPersistent();

        ShadowLooper.runUiThreadTasks();

        verify(mBrowserVisibilityDelegate, times(1)).releasePersistentShowingToken(1);
    }
}
