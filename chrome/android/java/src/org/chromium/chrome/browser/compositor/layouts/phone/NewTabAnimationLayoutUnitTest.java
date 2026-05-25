// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
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
import android.graphics.Rect;
import android.graphics.RectF;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.MathUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.cc.input.BrowserControlsState;
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
import org.chromium.chrome.browser.tabmodel.OverridableTabCount;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController;
import org.chromium.chrome.browser.toolbar.top.ToggleTabStackButton;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.TopInsetProvider;
import org.chromium.chrome.browser.ui.edge_to_edge.TransitiveTopInsetProvider;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;

import java.util.List;
import java.util.function.Supplier;

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
    @Mock private OverridableTabCount mOverridableTabCount;
    @Mock private BrowserControlsManager mBrowserControlsManager;
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
    @Mock private ToggleTabStackButton mTabSwitcherButton;
    @Mock private View mToolbar;
    @Mock private NewTabPage mNtp;
    @Mock private TopInsetProvider mTopInsetProvider;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    private SceneLayer mSceneLayer;

    private final SettableNullableObservableSupplier<Tab> mCurrentTabSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNonNullObservableSupplier<Boolean> mScrimVisibilitySupplier =
            ObservableSuppliers.createNonNull(false);
    private final TransitiveTopInsetProvider mTransitiveTopInsetProvider =
            new TransitiveTopInsetProvider();
    private final SettableNonNullObservableSupplier<Float>
            mNtpSearchBoxTransitionPercentageSupplier = ObservableSuppliers.createNonNull(0f);
    private final BrowserStateBrowserControlsVisibilityDelegate mBrowserVisibilityDelegate =
            new BrowserStateBrowserControlsVisibilityDelegate(ObservableSuppliers.alwaysFalse());
    private NewTabAnimationLayout mNewTabAnimationLayout;
    private FrameLayout mContentContainer;
    private FrameLayout mAnimationHostView;
    private UserDataHost mUserDataHost;

    @Before
    public void setUp() {
        SceneLayerJni.setInstanceForTesting(mSceneLayerJni);
        StaticTabSceneLayerJni.setInstanceForTesting(mStaticTabSceneLayerJni);
        when(mSceneLayerJni.init(any()))
                .thenReturn(FAKE_NATIVE_ADDRESS_1)
                .thenReturn(FAKE_NATIVE_ADDRESS_2);
        doAnswer(
                        invocation -> {
                            mSceneLayer = (SceneLayer) invocation.getArguments()[0];
                            mSceneLayer.setNativePtr(FAKE_NATIVE_ADDRESS_1);
                            return FAKE_NATIVE_ADDRESS_1;
                        })
                .when(mStaticTabSceneLayerJni)
                .init(any());
        doCallback(
                        /* index= */ 0,
                        (Long nativePointer) -> {
                            mSceneLayer.setNativePtr(0L);
                        })
                .when(mSceneLayerJni)
                .destroy(anyLong());

        when(mTabModelSelector.getCurrentTabSupplier()).thenReturn(mCurrentTabSupplier);
        when(mTabModelSelector.getModelForTabId(anyInt())).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(anyBoolean())).thenReturn(mTabModel);
        when(mTabModelSelector.getTabById(CURRENT_TAB_ID)).thenReturn(mCurrentTab);
        when(mTabModelSelector.getTabById(NEW_TAB_ID)).thenReturn(mNewTab);
        when(mTabModel.iterator())
                .thenAnswer(invocation -> List.of(mCurrentTab, mNewTab).iterator());
        when(mTabModel.getCount()).thenReturn(2);
        when(mTabModel.getTabAt(0)).thenReturn(mCurrentTab);
        when(mTabModel.getTabAt(1)).thenReturn(mNewTab);
        when(mTabModel.getTabById(CURRENT_TAB_ID)).thenReturn(mCurrentTab);
        when(mTabModel.getTabById(NEW_TAB_ID)).thenReturn(mNewTab);
        when(mCurrentTab.getId()).thenReturn(CURRENT_TAB_ID);
        mUserDataHost = new UserDataHost();
        when(mCurrentTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mNewTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mNewTab.getId()).thenReturn(NEW_TAB_ID);
        when(mNtp.getLastTouchPosition()).thenReturn(sPoint);
        when(mBrowserControlsManager.getBrowserVisibilityDelegate())
                .thenReturn(mBrowserVisibilityDelegate);
        when(mToolbarManager.getOverridableTabCount()).thenReturn(mOverridableTabCount);
        when(mToolbarManager.getNtpSearchBoxTransitionPercentageSupplier())
                .thenReturn(mNtpSearchBoxTransitionPercentageSupplier);
        mTransitiveTopInsetProvider.set(mTopInsetProvider);
        mScrimVisibilitySupplier.set(false);
        doAnswer(
                        invocation -> {
                            var args = invocation.getArguments();
                            return new LayoutTab((Integer) args[0], (Boolean) args[1], -1, -1);
                        })
                .when(mUpdateHost)
                .createLayoutTab(anyInt(), anyBoolean());
        // Mock TopInsetProvider to trigger observer callback when addObserver is called
        doAnswer(
                        invocation -> {
                            TopInsetProvider.Observer observer =
                                    (TopInsetProvider.Observer) invocation.getArgument(0);
                            // Trigger the callback immediately with systemTopInset=100
                            observer.onToEdgeChange(100, true, LayoutType.BROWSING);
                            return null;
                        })
                .when(mTopInsetProvider)
                .addObserver(any(TopInsetProvider.Observer.class));

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
                                mCompositorViewHolder,
                                mAnimationHostView,
                                mToolbarManager,
                                mBrowserControlsManager,
                                mScrimVisibilitySupplier,
                                mTransitiveTopInsetProvider));
        mNewTabAnimationLayout.setTabModelSelector(mTabModelSelector);
        mNewTabAnimationLayout.setTabContentManager(mTabContentManager);
        when(mAnimationHostView.findViewById(R.id.toolbar)).thenReturn(mToolbar);
        when(mToolbar.findViewById(R.id.tab_switcher_button)).thenReturn(mTabSwitcherButton);
        when(mAnimationHostView.getWidth()).thenReturn(40);
        when(mAnimationHostView.getHeight()).thenReturn(40);
        doAnswer(
                        invocation -> {
                            Rect rect = (Rect) invocation.getArgument(0);
                            rect.set(0, 0, 1080, 1920);
                            return true;
                        })
                .when(mAnimationHostView)
                .getGlobalVisibleRect(any(Rect.class));
        Supplier<EdgeToEdgeController> edgeToEdgeControllerSupplier = () -> mEdgeToEdgeController;
        when(mToolbarManager.getEdgeToEdgeControllerSupplier())
                .thenReturn(edgeToEdgeControllerSupplier);
        mNewTabAnimationLayout.onFinishNativeInitialization();
        mNewTabAnimationLayout.setRunOnNextLayoutImmediatelyForTesting(true);
    }

    @After
    public void tearDown() throws Exception {
        mNewTabAnimationLayout.destroy();
        java.lang.reflect.Field field =
                ToolbarPositionController.class.getDeclaredField("sToolbarShouldShowOnTop");
        field.setAccessible(true);
        field.set(null, null);
    }

    @Test
    public void testConstants() {
        assertEquals(
                ViewportMode.USE_PREVIOUS_BROWSER_CONTROLS_STATE,
                mNewTabAnimationLayout.getViewportMode());
        assertTrue(mNewTabAnimationLayout.handlesTabCreating());
        assertFalse(mNewTabAnimationLayout.handlesTabClosing());
        assertThat(mNewTabAnimationLayout.getEventFilter())
                .isInstanceOf(BlackHoleEventFilter.class);
        assertThat(mNewTabAnimationLayout.getSceneLayer()).isInstanceOf(StaticTabSceneLayer.class);
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
        verify(mNewTabAnimationLayout, times(1)).forceAnimationToFinish();
        assertTrue(mNewTabAnimationLayout.isRunningAnimations());
        verify(mAnimationHostView, times(1)).addView(any(NewForegroundTabAnimationHostView.class));

        RobolectricUtil.runAllBackgroundAndUi();

        assertFalse(mNewTabAnimationLayout.isRunningAnimations());
        verify(mAnimationHostView, times(1))
                .removeView(any(NewForegroundTabAnimationHostView.class));
        verify(mTabModelSelector).selectModel(false);
        assertTrue(mNewTabAnimationLayout.isStartingToHide());
    }

    @Test
    public void testOnTabCreated_tabCreatedInForeground_topPadding() {
        when(mNewTab.isNativePage()).thenReturn(true);
        when(mNewTab.getNativePage()).thenReturn(mNtp);
        when(mNtp.supportsEdgeToEdgeOnTop()).thenReturn(true);
        when(mBrowserControlsManager.getContentOffset()).thenReturn(50);

        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ false,
                /* originX= */ 0f,
                /* originY= */ 0f);

        mNewTabAnimationLayout.updateSceneLayer(null, null, null, null, mBrowserControlsManager);

        LayoutTab layoutTab = mNewTabAnimationLayout.getLayoutTabsToRender()[0];
        assertEquals(
                "Top padding should be applied.",
                150,
                layoutTab.get(LayoutTab.CONTENT_OFFSET_Y),
                MathUtils.EPSILON);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testOnTabCreated_tabCreatedInForeground_bottomBarEnabled() {
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
        verify(mNewTabAnimationLayout, times(1)).forceAnimationToFinish();
        assertTrue(mNewTabAnimationLayout.isRunningAnimations());
        verify(mAnimationHostView, times(1)).addView(any(NewForegroundTabAnimationHostView.class));

        RobolectricUtil.runAllBackgroundAndUi();

        assertFalse(mNewTabAnimationLayout.isRunningAnimations());
        verify(mAnimationHostView, times(1))
                .removeView(any(NewForegroundTabAnimationHostView.class));
        verify(mTabModelSelector).selectModel(false);
        assertTrue(mNewTabAnimationLayout.isStartingToHide());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR,
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/true"
    })
    public void testOnTabCreated_NtpToWebPage_bottomBarCoordination() throws Exception {
        // Transition: NTP (no bottom bar) -> Web (has bottom bar)
        // Setup old tab as regular NTP (no bottom bar)
        when(mCurrentTab.getUrl()).thenReturn(new GURL("chrome://newtab"));
        when(mCurrentTab.isIncognitoBranded()).thenReturn(false);

        // Setup new tab as regular web page (has bottom bar)
        when(mNewTab.getUrl()).thenReturn(new GURL("https://google.com"));
        when(mNewTab.isIncognitoBranded()).thenReturn(false);

        // Viewport of NTP is full screen
        Rect compositorRect = new Rect(0, 0, 1080, 1920);
        RectF compositorRectF = new RectF(compositorRect);
        doAnswer(
                        invocation -> {
                            RectF rectF = (RectF) invocation.getArgument(0);
                            rectF.set(compositorRectF);
                            return null;
                        })
                .when(mCompositorViewHolder)
                .getVisibleViewport(any(RectF.class));

        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ false,
                /* originX= */ 0f,
                /* originY= */ 0f);

        // Capture NewForegroundTabAnimationHostView
        ArgumentCaptor<NewForegroundTabAnimationHostView> viewCaptor =
                ArgumentCaptor.forClass(NewForegroundTabAnimationHostView.class);
        verify(mAnimationHostView).addView(viewCaptor.capture());
        NewForegroundTabAnimationHostView hostView = viewCaptor.getValue();

        // Use reflection to access private mInitialRect
        java.lang.reflect.Field initialRectField =
                NewForegroundTabAnimationHostView.class.getDeclaredField("mInitialRect");
        initialRectField.setAccessible(true);
        Rect initialRect = (Rect) initialRectField.get(hostView);

        // Symmetrical centering checks (539 due to -1px LTR left adjustment)
        assertEquals(539, initialRect.centerX());
        // Bottom of initialRect should start at the absolute bottom edge of the screen (1920 + 1
        // overlap)
        assertEquals(1921, initialRect.bottom);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR,
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/true"
    })
    public void testOnTabCreated_WebPageToNtp_bottomBarCoordination() throws Exception {
        // Transition: Web (has bottom bar) -> NTP (no bottom bar)
        // Setup old tab as regular web page (has bottom bar)
        when(mCurrentTab.getUrl()).thenReturn(new GURL("https://google.com"));
        when(mCurrentTab.isIncognitoBranded()).thenReturn(false);

        // Setup new tab as regular NTP (no bottom bar)
        when(mNewTab.getUrl()).thenReturn(new GURL("chrome://newtab"));
        when(mNewTab.isIncognitoBranded()).thenReturn(false);

        int bottomBarHeight =
                mNewTabAnimationLayout
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_bar_height);

        // Viewport of Web page excludes bottom controls
        Rect compositorRect = new Rect(0, 0, 1080, 1920 - bottomBarHeight);
        RectF compositorRectF = new RectF(compositorRect);
        doAnswer(
                        invocation -> {
                            RectF rectF = (RectF) invocation.getArgument(0);
                            rectF.set(compositorRectF);
                            return null;
                        })
                .when(mCompositorViewHolder)
                .getVisibleViewport(any(RectF.class));

        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ false,
                /* originX= */ 0f,
                /* originY= */ 0f);

        // Capture NewForegroundTabAnimationHostView
        ArgumentCaptor<NewForegroundTabAnimationHostView> viewCaptor =
                ArgumentCaptor.forClass(NewForegroundTabAnimationHostView.class);
        verify(mAnimationHostView).addView(viewCaptor.capture());
        NewForegroundTabAnimationHostView hostView = viewCaptor.getValue();

        // Use reflection to access private mInitialRect
        java.lang.reflect.Field initialRectField =
                NewForegroundTabAnimationHostView.class.getDeclaredField("mInitialRect");
        initialRectField.setAccessible(true);
        Rect initialRect = (Rect) initialRectField.get(hostView);

        // Symmetrical centering checks (539 due to -1px LTR left adjustment)
        assertEquals(539, initialRect.centerX());
        // Bottom of initialRect should start sitting on top of the bottom bar (1920 -
        // bottomBarHeight + 1 overlap)
        assertEquals(1921 - bottomBarHeight, initialRect.bottom);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR,
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/true"
    })
    public void testOnTabCreated_NtpToNtp_noBottomBarCoordination() throws Exception {
        // Transition: NTP (no bottom bar) -> NTP (no bottom bar)
        // Setup old tab as regular NTP (no bottom bar)
        when(mCurrentTab.getUrl()).thenReturn(new GURL("chrome://newtab"));
        when(mCurrentTab.isIncognitoBranded()).thenReturn(false);

        // Setup new tab as regular NTP (no bottom bar)
        when(mNewTab.getUrl()).thenReturn(new GURL("chrome://newtab"));
        when(mNewTab.isIncognitoBranded()).thenReturn(false);

        // Viewport of NTP is full screen
        Rect compositorRect = new Rect(0, 0, 1080, 1920);
        RectF compositorRectF = new RectF(compositorRect);
        doAnswer(
                        invocation -> {
                            RectF rectF = (RectF) invocation.getArgument(0);
                            rectF.set(compositorRectF);
                            return null;
                        })
                .when(mCompositorViewHolder)
                .getVisibleViewport(any(RectF.class));

        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ false,
                /* originX= */ 0f,
                /* originY= */ 0f);

        // Capture NewForegroundTabAnimationHostView
        ArgumentCaptor<NewForegroundTabAnimationHostView> viewCaptor =
                ArgumentCaptor.forClass(NewForegroundTabAnimationHostView.class);
        verify(mAnimationHostView).addView(viewCaptor.capture());
        NewForegroundTabAnimationHostView hostView = viewCaptor.getValue();

        // Use reflection to access private mInitialRect
        java.lang.reflect.Field initialRectField =
                NewForegroundTabAnimationHostView.class.getDeclaredField("mInitialRect");
        initialRectField.setAccessible(true);
        Rect initialRect = (Rect) initialRectField.get(hostView);

        // Symmetrical centering checks (539 due to -1px LTR left adjustment)
        assertEquals(539, initialRect.centerX());
        // Bottom of initialRect should start at the absolute bottom edge of the screen (1920 + 1
        // overlap)
        // since both pages have the bottom bar disabled (no viewport mismatch)
        assertEquals(1921, initialRect.bottom);
    }

    @Test
    public void testOnTabCreated_NtpToWebPage_bottomToolbarCoordination() throws Exception {
        // Configure bottom toolbar preference
        java.lang.reflect.Field field =
                ToolbarPositionController.class.getDeclaredField("sToolbarShouldShowOnTop");
        field.setAccessible(true);
        field.set(null, false); // Set static field to false (bottom toolbar)

        // Transition: NTP (no bottom controls) -> Web (has bottom toolbar)
        // Setup old tab as regular NTP (toolbar is top)
        when(mCurrentTab.getUrl()).thenReturn(new GURL("chrome://newtab"));
        when(mCurrentTab.isIncognitoBranded()).thenReturn(false);

        // Setup new tab as regular web page (has bottom toolbar)
        when(mNewTab.getUrl()).thenReturn(new GURL("https://google.com"));
        when(mNewTab.isIncognitoBranded()).thenReturn(false);

        // Viewport of NTP is full screen
        Rect compositorRect = new Rect(0, 0, 1080, 1920);
        RectF compositorRectF = new RectF(compositorRect);
        doAnswer(
                        invocation -> {
                            RectF rectF = (RectF) invocation.getArgument(0);
                            rectF.set(compositorRectF);
                            return null;
                        })
                .when(mCompositorViewHolder)
                .getVisibleViewport(any(RectF.class));

        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ false,
                /* originX= */ 0f,
                /* originY= */ 0f);

        // Capture NewForegroundTabAnimationHostView
        ArgumentCaptor<NewForegroundTabAnimationHostView> viewCaptor =
                ArgumentCaptor.forClass(NewForegroundTabAnimationHostView.class);
        verify(mAnimationHostView).addView(viewCaptor.capture());
        NewForegroundTabAnimationHostView hostView = viewCaptor.getValue();

        // Use reflection to access private mInitialRect
        java.lang.reflect.Field initialRectField =
                NewForegroundTabAnimationHostView.class.getDeclaredField("mInitialRect");
        initialRectField.setAccessible(true);
        Rect initialRect = (Rect) initialRectField.get(hostView);

        // Corner-anchored checks (107 due to left=-1, width=216 in test config)
        assertEquals(107, initialRect.centerX());
        // Starts at the top edge of the screen (-1 overlap)
        assertEquals(-1, initialRect.top);
    }

    @Test
    public void testOnTabCreated_WebPageToNtp_bottomToolbarCoordination() throws Exception {
        // Configure bottom toolbar preference
        java.lang.reflect.Field field =
                ToolbarPositionController.class.getDeclaredField("sToolbarShouldShowOnTop");
        field.setAccessible(true);
        field.set(null, false); // Set static field to false (bottom toolbar)

        // Transition: Web (has bottom toolbar) -> NTP (no bottom controls)
        // Setup old tab as regular web page (has bottom toolbar)
        when(mCurrentTab.getUrl()).thenReturn(new GURL("https://google.com"));
        when(mCurrentTab.isIncognitoBranded()).thenReturn(false);

        // Setup new tab as regular NTP (toolbar is top)
        when(mNewTab.getUrl()).thenReturn(new GURL("chrome://newtab"));
        when(mNewTab.isIncognitoBranded()).thenReturn(false);

        int controlContainerHeight =
                mNewTabAnimationLayout
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.control_container_height);

        // Viewport of Web page excludes bottom controls
        Rect compositorRect = new Rect(0, 0, 1080, 1920 - controlContainerHeight);
        RectF compositorRectF = new RectF(compositorRect);
        doAnswer(
                        invocation -> {
                            RectF rectF = (RectF) invocation.getArgument(0);
                            rectF.set(compositorRectF);
                            return null;
                        })
                .when(mCompositorViewHolder)
                .getVisibleViewport(any(RectF.class));

        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ false,
                /* originX= */ 0f,
                /* originY= */ 0f);

        // Capture NewForegroundTabAnimationHostView
        ArgumentCaptor<NewForegroundTabAnimationHostView> viewCaptor =
                ArgumentCaptor.forClass(NewForegroundTabAnimationHostView.class);
        verify(mAnimationHostView).addView(viewCaptor.capture());
        NewForegroundTabAnimationHostView hostView = viewCaptor.getValue();

        // Use reflection to access private mInitialRect
        java.lang.reflect.Field initialRectField =
                NewForegroundTabAnimationHostView.class.getDeclaredField("mInitialRect");
        initialRectField.setAccessible(true);
        Rect initialRect = (Rect) initialRectField.get(hostView);

        // Corner-anchored checks (107 due to left=-1, width=216 in test config)
        assertEquals(107, initialRect.centerX());
        // Bottom of initialRect should start sitting on top of the bottom toolbar (1921 -
        // controlContainerHeight overlap)
        assertEquals(1921 - controlContainerHeight, initialRect.bottom);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR,
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/false"
    })
    public void testOnTabCreated_WebPageToNtp_bottomBarEnabledOnNtp_noViewportMismatch()
            throws Exception {
        // Transition: Web (has bottom bar) -> NTP (has bottom bar because disable_on_ntp is false)
        // Setup old tab as regular web page (has bottom bar)
        when(mCurrentTab.getUrl()).thenReturn(new GURL("https://google.com"));
        when(mCurrentTab.isIncognitoBranded()).thenReturn(false);

        // Setup new tab as regular NTP (also has bottom bar)
        when(mNewTab.getUrl()).thenReturn(new GURL("chrome://newtab"));
        when(mNewTab.isIncognitoBranded()).thenReturn(false);

        int bottomBarHeight =
                mNewTabAnimationLayout
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_bar_height);

        // Viewport of Web page excludes bottom controls
        Rect compositorRect = new Rect(0, 0, 1080, 1920 - bottomBarHeight);
        RectF compositorRectF = new RectF(compositorRect);
        doAnswer(
                        invocation -> {
                            RectF rectF = (RectF) invocation.getArgument(0);
                            rectF.set(compositorRectF);
                            return null;
                        })
                .when(mCompositorViewHolder)
                .getVisibleViewport(any(RectF.class));

        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ false,
                /* originX= */ 0f,
                /* originY= */ 0f);

        // Capture NewForegroundTabAnimationHostView
        ArgumentCaptor<NewForegroundTabAnimationHostView> viewCaptor =
                ArgumentCaptor.forClass(NewForegroundTabAnimationHostView.class);
        verify(mAnimationHostView).addView(viewCaptor.capture());
        NewForegroundTabAnimationHostView hostView = viewCaptor.getValue();

        // Use reflection to access private mInitialRect
        java.lang.reflect.Field initialRectField =
                NewForegroundTabAnimationHostView.class.getDeclaredField("mInitialRect");
        initialRectField.setAccessible(true);
        Rect initialRect = (Rect) initialRectField.get(hostView);

        // Symmetrical centering checks (539 due to -1px LTR left adjustment)
        assertEquals(539, initialRect.centerX());
        // Bottom of initialRect should start sitting on top of the bottom bar (1920 -
        // bottomBarHeight + 1 overlap)
        // Since both old and new tabs have bottom bar, no coordinate shifts are applied (Diff = 0)
        assertEquals(1921 - bottomBarHeight, initialRect.bottom);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR,
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/true"
    })
    public void testOnTabCreated_NtpToWebPage_bottomChinCoordination() throws Exception {
        // Transition: NTP (no bottom controls, chin hidden) -> Web (has bottom bar + chin)
        // Setup old tab as regular NTP (no bottom bar, supports E2E)
        when(mCurrentTab.getUrl()).thenReturn(new GURL("chrome://newtab"));
        when(mCurrentTab.isIncognitoBranded()).thenReturn(false);
        when(mCurrentTab.isNativePage()).thenReturn(true);
        when(mCurrentTab.getNativePage()).thenReturn(mNtp);
        when(mNtp.supportsEdgeToEdge()).thenReturn(true);

        // Setup new tab as regular web page (has bottom bar, does NOT support E2E)
        when(mNewTab.getUrl()).thenReturn(new GURL("https://google.com"));
        when(mNewTab.isIncognitoBranded()).thenReturn(false);
        when(mNewTab.isNativePage()).thenReturn(false);

        // Mock E2E gesture nav bottom chin is active
        when(mEdgeToEdgeController.isDrawingToEdge()).thenReturn(true);
        int bottomChinHeight = 60;
        when(mEdgeToEdgeController.getSystemBottomInsetPx()).thenReturn(bottomChinHeight);

        int bottomBarHeight =
                mNewTabAnimationLayout
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_bar_height);

        // Viewport of NTP is full screen
        Rect compositorRect = new Rect(0, 0, 1080, 1920);
        RectF compositorRectF = new RectF(compositorRect);
        doAnswer(
                        invocation -> {
                            RectF rectF = (RectF) invocation.getArgument(0);
                            rectF.set(compositorRectF);
                            return null;
                        })
                .when(mCompositorViewHolder)
                .getVisibleViewport(any(RectF.class));

        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ false,
                /* originX= */ 0f,
                /* originY= */ 0f);

        // Capture NewForegroundTabAnimationHostView
        ArgumentCaptor<NewForegroundTabAnimationHostView> viewCaptor =
                ArgumentCaptor.forClass(NewForegroundTabAnimationHostView.class);
        verify(mAnimationHostView).addView(viewCaptor.capture());
        NewForegroundTabAnimationHostView hostView = viewCaptor.getValue();

        // Use reflection to access private mInitialRect
        java.lang.reflect.Field initialRectField =
                NewForegroundTabAnimationHostView.class.getDeclaredField("mInitialRect");
        initialRectField.setAccessible(true);
        Rect initialRect = (Rect) initialRectField.get(hostView);

        // Symmetrical centering checks (539 due to -1px LTR left adjustment)
        assertEquals(539, initialRect.centerX());
        // Bottom of initialRect should start at the absolute bottom edge of the screen (1920 + 1
        // overlap)
        // since the NTP had no bottom controls or bottom chin visible.
        assertEquals(1921, initialRect.bottom);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR,
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/true"
    })
    public void testOnTabCreated_WebPageToNtp_bottomChinCoordination() throws Exception {
        // Transition: Web (has bottom bar + chin) -> NTP (no bottom controls, chin hidden)
        // Setup old tab as regular web page (has bottom bar, does NOT support E2E)
        when(mCurrentTab.getUrl()).thenReturn(new GURL("https://google.com"));
        when(mCurrentTab.isIncognitoBranded()).thenReturn(false);
        when(mCurrentTab.isNativePage()).thenReturn(false);

        // Setup new tab as regular NTP (no bottom bar, supports E2E)
        when(mNewTab.getUrl()).thenReturn(new GURL("chrome://newtab"));
        when(mNewTab.isIncognitoBranded()).thenReturn(false);
        when(mNewTab.isNativePage()).thenReturn(true);
        when(mNewTab.getNativePage()).thenReturn(mNtp);
        when(mNtp.supportsEdgeToEdge()).thenReturn(true);

        // Mock E2E gesture nav bottom chin is active
        when(mEdgeToEdgeController.isDrawingToEdge()).thenReturn(true);
        int bottomChinHeight = 60;
        when(mEdgeToEdgeController.getSystemBottomInsetPx()).thenReturn(bottomChinHeight);

        int bottomBarHeight =
                mNewTabAnimationLayout
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_bar_height);

        // Viewport of starting Web page excludes bottom controls & bottom chin
        Rect compositorRect = new Rect(0, 0, 1080, 1920 - bottomBarHeight - bottomChinHeight);
        RectF compositorRectF = new RectF(compositorRect);
        doAnswer(
                        invocation -> {
                            RectF rectF = (RectF) invocation.getArgument(0);
                            rectF.set(compositorRectF);
                            return null;
                        })
                .when(mCompositorViewHolder)
                .getVisibleViewport(any(RectF.class));

        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ false,
                /* originX= */ 0f,
                /* originY= */ 0f);

        // Capture NewForegroundTabAnimationHostView
        ArgumentCaptor<NewForegroundTabAnimationHostView> viewCaptor =
                ArgumentCaptor.forClass(NewForegroundTabAnimationHostView.class);
        verify(mAnimationHostView).addView(viewCaptor.capture());
        NewForegroundTabAnimationHostView hostView = viewCaptor.getValue();

        // Use reflection to access private mInitialRect
        java.lang.reflect.Field initialRectField =
                NewForegroundTabAnimationHostView.class.getDeclaredField("mInitialRect");
        initialRectField.setAccessible(true);
        Rect initialRect = (Rect) initialRectField.get(hostView);

        // Symmetrical centering checks (539 due to -1px LTR left adjustment)
        assertEquals(539, initialRect.centerX());
        // Bottom of initialRect should start sitting on top of the bottom bar & bottom chin (1920 -
        // bottomBarHeight - bottomChinHeight + 1 overlap)
        assertEquals(1921 - bottomBarHeight - bottomChinHeight, initialRect.bottom);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR,
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/true"
    })
    public void testOnTabCreated_NtpToIncognitoNtp_bottomChinCoordination() throws Exception {
        // Transition: Regular NTP (no bottom controls, chin hidden) -> Incognito NTP (has bottom
        // controls, chin visible)
        // Setup old tab as regular NTP (no bottom bar, supports E2E)
        when(mCurrentTab.getUrl()).thenReturn(new GURL("chrome://newtab"));
        when(mCurrentTab.isIncognitoBranded()).thenReturn(false);
        when(mCurrentTab.isNativePage()).thenReturn(true);
        when(mCurrentTab.getNativePage()).thenReturn(mNtp);
        when(mNtp.supportsEdgeToEdge()).thenReturn(true);

        // Setup new tab as Incognito NTP (has bottom bar, supports E2E, but has other controls
        // visible)
        when(mNewTab.getUrl()).thenReturn(new GURL("chrome://newtab"));
        when(mNewTab.isIncognitoBranded()).thenReturn(true);
        when(mNewTab.isNativePage()).thenReturn(true);
        when(mNewTab.getNativePage()).thenReturn(mNtp);
        when(mNtp.supportsEdgeToEdge()).thenReturn(true);

        // Configure bottom toolbar preference (remains on bottom on Incognito NTP)
        java.lang.reflect.Field prefField =
                ToolbarPositionController.class.getDeclaredField("sToolbarShouldShowOnTop");
        prefField.setAccessible(true);
        prefField.set(null, false); // Bottom toolbar

        // Mock E2E gesture nav bottom chin is active
        when(mEdgeToEdgeController.isDrawingToEdge()).thenReturn(true);
        int bottomChinHeight = 60;
        when(mEdgeToEdgeController.getSystemBottomInsetPx()).thenReturn(bottomChinHeight);

        int bottomBarHeight =
                mNewTabAnimationLayout
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_bar_height);
        int controlContainerHeight =
                mNewTabAnimationLayout
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.control_container_height);

        // Viewport of Regular NTP is full screen
        Rect compositorRect = new Rect(0, 0, 1080, 1920);
        RectF compositorRectF = new RectF(compositorRect);
        doAnswer(
                        invocation -> {
                            RectF rectF = (RectF) invocation.getArgument(0);
                            rectF.set(compositorRectF);
                            return null;
                        })
                .when(mCompositorViewHolder)
                .getVisibleViewport(any(RectF.class));

        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ true,
                /* background= */ false,
                /* originX= */ 0f,
                /* originY= */ 0f);

        // Capture NewForegroundTabAnimationHostView
        ArgumentCaptor<NewForegroundTabAnimationHostView> viewCaptor =
                ArgumentCaptor.forClass(NewForegroundTabAnimationHostView.class);
        verify(mAnimationHostView).addView(viewCaptor.capture());
        NewForegroundTabAnimationHostView hostView = viewCaptor.getValue();

        // Use reflection to access private mInitialRect
        java.lang.reflect.Field initialRectField =
                NewForegroundTabAnimationHostView.class.getDeclaredField("mInitialRect");
        initialRectField.setAccessible(true);
        Rect initialRect = (Rect) initialRectField.get(hostView);

        // Symmetrical centering checks (539 due to -1px LTR left adjustment)
        assertEquals(539, initialRect.centerX());
        // Bottom of initialRect should start at the absolute bottom edge of the screen (1920 + 1
        // overlap)
        // since the Regular NTP had no bottom controls or bottom chin visible.
        assertEquals(1921, initialRect.bottom);
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
        verify(mNewTabAnimationLayout, times(1)).forceAnimationToFinish();
        assertTrue(mNewTabAnimationLayout.isStartingToHide());
        assertThat(mBrowserVisibilityDelegate.get()).isEqualTo(BrowserControlsState.SHOWN);
        verify(mAnimationHostView, times(1)).addView(any(NewBackgroundTabAnimationHostView.class));

        RobolectricUtil.runAllBackgroundAndUi();

        verify(mAnimationHostView, times(1))
                .removeView(any(NewBackgroundTabAnimationHostView.class));
        verify(mTabModelSelector, never()).selectModel(false);
        assertThat(mBrowserVisibilityDelegate.get()).isEqualTo(BrowserControlsState.BOTH);
    }

    @Test
    public void testOnTabCreated_tabCreatedInBackground_forceHidingImmediatelyIfNeeded() {
        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ true,
                /* originX= */ 0f,
                /* originY= */ 0f);
        assertTrue(
                "Layout should be starting to hide, but not hidden.",
                mNewTabAnimationLayout.isStartingToHide());

        setNtp();
        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ true,
                /* originX= */ 0f,
                /* originY= */ 0f);
        assertFalse(
                "Layout should have immediately hidden.",
                mNewTabAnimationLayout.isStartingToHide());
    }

    @Test
    public void testOnTabCreated_tabCreatedInBackground_ntpToken() {
        setNtp();
        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ true,
                /* originX= */ 0f,
                /* originY= */ 0f);

        RobolectricUtil.runAllBackgroundAndUi();

        assertThat(mBrowserVisibilityDelegate.get()).isEqualTo(BrowserControlsState.BOTH);
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

        assertThat(mBrowserVisibilityDelegate.get()).isEqualTo(BrowserControlsState.SHOWN);

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

        assertThat(mBrowserVisibilityDelegate.get()).isEqualTo(BrowserControlsState.SHOWN);

        RobolectricUtil.runAllBackgroundAndUi();

        assertThat(mBrowserVisibilityDelegate.get()).isEqualTo(BrowserControlsState.BOTH);
    }

    private void setNtp() {
        when(mCurrentTab.getUrl()).thenReturn(new GURL("chrome://newtab"));
        when(mCurrentTab.getNativePage()).thenReturn(mNtp);
    }
}
