// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
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
import android.os.Build;
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

import org.chromium.base.MathUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
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
import org.chromium.chrome.browser.ui.bottombar.BottomBarUtils;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.TopInsetProvider;
import org.chromium.chrome.browser.ui.edge_to_edge.TransitiveTopInsetProvider;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;

import java.lang.reflect.Field;
import java.util.List;
import java.util.function.Supplier;

/** Unit tests for {@link NewTabAnimationLayout}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
@EnableFeatures({
    ChromeFeatureList.SENSITIVE_CONTENT,
    ChromeFeatureList.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS
})
public class NewTabAnimationLayoutUnitTest {
    private static class FakeBrowserStateBrowserControlsVisibilityDelegate
            extends BrowserStateBrowserControlsVisibilityDelegate {
        public int showControlsPersistentCallCount;
        public int releasePersistentShowingTokenCallCount;

        public FakeBrowserStateBrowserControlsVisibilityDelegate(
                NonNullObservableSupplier<Boolean> persistentFullscreenMode) {
            super(persistentFullscreenMode);
        }

        @Override
        public int showControlsPersistent() {
            showControlsPersistentCallCount++;
            return super.showControlsPersistent();
        }

        @Override
        public void releasePersistentShowingToken(int token) {
            releasePersistentShowingTokenCallCount++;
            super.releasePersistentShowingToken(token);
        }
    }

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
    private final FakeBrowserStateBrowserControlsVisibilityDelegate mBrowserVisibilityDelegate =
            new FakeBrowserStateBrowserControlsVisibilityDelegate(
                    ObservableSuppliers.alwaysFalse());
    private NewTabAnimationLayout mNewTabAnimationLayout;
    private FrameLayout mContentContainer;
    private FrameLayout mAnimationHostView;
    private CompositorViewHolder mCompositorViewHolder;
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
        doCallback(/* index= */ 0, (Long nativePointer) -> mSceneLayer.setNativePtr(0L))
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
        when(mTabModel.indexOf(mCurrentTab)).thenReturn(0);
        when(mTabModel.indexOf(mNewTab)).thenReturn(1);
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
                            TopInsetProvider.Observer observer = invocation.getArgument(0);
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
        mAnimationHostView = new FrameLayout(activity);
        mCompositorViewHolder = new CompositorViewHolder(activity, /* attrs= */ null);
        FrameLayout toolbar = new FrameLayout(activity);
        toolbar.setId(R.id.toolbar);
        View tabSwitcherButton = new View(activity);
        tabSwitcherButton.setId(R.id.tab_switcher_button);
        toolbar.addView(tabSwitcherButton);
        FrameLayout bottomBar = new FrameLayout(activity);
        bottomBar.setId(org.chromium.chrome.browser.ui.bottombar.R.id.bottom_bar_container);
        View bottomBarTabSwitcherButton = new View(activity);
        bottomBarTabSwitcherButton.setId(R.id.tab_switcher_button);
        bottomBar.addView(bottomBarTabSwitcherButton);
        mAnimationHostView.addView(toolbar);
        mAnimationHostView.addView(bottomBar);
        mAnimationHostView.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1920, View.MeasureSpec.EXACTLY));
        mAnimationHostView.layout(0, 0, 1080, 1920);
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
        Supplier<EdgeToEdgeController> edgeToEdgeControllerSupplier = () -> mEdgeToEdgeController;
        when(mToolbarManager.getEdgeToEdgeControllerSupplier())
                .thenReturn(edgeToEdgeControllerSupplier);
        mNewTabAnimationLayout.onFinishNativeInitialization();
        mNewTabAnimationLayout.setRunOnNextLayoutImmediatelyForTesting(true);
    }

    @After
    public void tearDown() throws Exception {
        mNewTabAnimationLayout.destroy();
        Field field = ToolbarPositionController.class.getDeclaredField("sToolbarShouldShowOnTop");
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
        assertNull(findChildView(NewForegroundTabAnimationHostView.class));

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
        assertNotNull(findChildView(NewForegroundTabAnimationHostView.class));

        RobolectricUtil.runAllBackgroundAndUi();

        assertFalse(mNewTabAnimationLayout.isRunningAnimations());
        assertNull(findChildView(NewForegroundTabAnimationHostView.class));
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
        assertNull(findChildView(NewForegroundTabAnimationHostView.class));

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
        assertNotNull(findChildView(NewForegroundTabAnimationHostView.class));

        RobolectricUtil.runAllBackgroundAndUi();

        assertFalse(mNewTabAnimationLayout.isRunningAnimations());
        assertNull(findChildView(NewForegroundTabAnimationHostView.class));
        verify(mTabModelSelector).selectModel(false);
        assertTrue(mNewTabAnimationLayout.isStartingToHide());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testOnTabCreated_NtpToWebPage_bottomToolbarCoordination() throws Exception {
        // Configure bottom toolbar preference
        Field field = ToolbarPositionController.class.getDeclaredField("sToolbarShouldShowOnTop");
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
        mCompositorViewHolder.layout(0, 0, 1080, 1920);

        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ false,
                /* originX= */ 0f,
                /* originY= */ 0f);

        NewForegroundTabAnimationHostView hostView =
                findChildView(NewForegroundTabAnimationHostView.class);
        assertNotNull(hostView);

        // Use reflection to access private mInitialRect
        Field initialRectField =
                NewForegroundTabAnimationHostView.class.getDeclaredField("mInitialRect");
        initialRectField.setAccessible(true);
        Rect initialRect = (Rect) initialRectField.get(hostView);

        // Corner-anchored checks (107 due to left=-1, width=216 in test config)
        assertEquals(107, initialRect.centerX());
        // Starts at the top edge of the screen (-1 overlap)
        assertEquals(-1, initialRect.top);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testOnTabCreated_WebPageToNtp_bottomToolbarCoordination() throws Exception {
        // Configure bottom toolbar preference
        Field field = ToolbarPositionController.class.getDeclaredField("sToolbarShouldShowOnTop");
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
        mCompositorViewHolder.layout(0, 0, 1080, 1920 - controlContainerHeight);

        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ false,
                /* originX= */ 0f,
                /* originY= */ 0f);

        NewForegroundTabAnimationHostView hostView =
                findChildView(NewForegroundTabAnimationHostView.class);
        assertNotNull(hostView);

        // Use reflection to access private mInitialRect
        Field initialRectField =
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
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testOnTabCreated_WebPageToNtp_bottomBarEnabledOnNtp_noViewportMismatch()
            throws Exception {
        // Transition: Web (has bottom bar + chin) -> NTP (has bottom bar + chin)
        // Setup old tab as regular web page (has bottom bar, does NOT support E2E)
        when(mCurrentTab.getUrl()).thenReturn(new GURL("https://google.com"));
        when(mCurrentTab.isIncognitoBranded()).thenReturn(false);
        when(mCurrentTab.isNativePage()).thenReturn(false);

        // Setup new tab as regular NTP (also has bottom bar, supports E2E)
        when(mNewTab.getUrl()).thenReturn(new GURL("chrome://newtab"));
        when(mNewTab.isIncognitoBranded()).thenReturn(false);
        when(mNewTab.isNativePage()).thenReturn(true);
        when(mNewTab.getNativePage()).thenReturn(mNtp);
        when(mNtp.supportsEdgeToEdgeOnBottom()).thenReturn(true);

        when(mEdgeToEdgeController.isDrawingToEdge()).thenReturn(true);
        int bottomChinHeight = 60;
        when(mEdgeToEdgeController.getSystemBottomInsetPx()).thenReturn(bottomChinHeight);

        int bottomBarHeight =
                BottomBarUtils.getBottomBarHeight(mNewTabAnimationLayout.getContext());

        // Viewport of Web page excludes bottom controls and bottom chin
        mCompositorViewHolder.layout(0, 0, 1080, 1920 - bottomBarHeight - bottomChinHeight);

        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ false,
                /* originX= */ 0f,
                /* originY= */ 0f);

        NewForegroundTabAnimationHostView hostView =
                findChildView(NewForegroundTabAnimationHostView.class);
        assertNotNull(hostView);

        // Use reflection to access private mInitialRect
        Field initialRectField =
                NewForegroundTabAnimationHostView.class.getDeclaredField("mInitialRect");
        initialRectField.setAccessible(true);
        Rect initialRect = (Rect) initialRectField.get(hostView);

        // Symmetrical centering checks (539 due to -1px LTR left adjustment)
        assertEquals(539, initialRect.centerX());
        // Since both old and new tabs have bottom bar and bottom chin, no coordinate shifts are
        // applied (Diff = 0)
        assertEquals(1921 - bottomBarHeight - bottomChinHeight, initialRect.bottom);
    }

    @Test
    public void testOnTabCreated_tabCreatedInBackground() {
        LayoutTab[] layoutTabs = mNewTabAnimationLayout.getLayoutTabsToRender();
        assertNull(layoutTabs);
        assertNull(findChildView(NewBackgroundTabAnimationHostView.class));

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
        assertEquals(1, mBrowserVisibilityDelegate.showControlsPersistentCallCount);
        assertThat(mBrowserVisibilityDelegate.get()).isEqualTo(BrowserControlsState.SHOWN);
        assertNotNull(findChildView(NewBackgroundTabAnimationHostView.class));

        RobolectricUtil.runAllBackgroundAndUi();

        assertNull(findChildView(NewBackgroundTabAnimationHostView.class));
        verify(mTabModelSelector, never()).selectModel(false);
        assertEquals(1, mBrowserVisibilityDelegate.releasePersistentShowingTokenCallCount);
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
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
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

        assertEquals(0, mBrowserVisibilityDelegate.showControlsPersistentCallCount);

        RobolectricUtil.runAllBackgroundAndUi();

        assertThat(mBrowserVisibilityDelegate.get()).isEqualTo(BrowserControlsState.BOTH);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testOnTabCreated_tabCreatedInBackground_ntp_bottomBarEnabled() {
        setNtp();
        when(mNtp.supportsEdgeToEdgeOnTop()).thenReturn(true);
        mNewTabAnimationLayout.onTabCreated(
                FAKE_TIME,
                NEW_TAB_ID,
                /* index= */ 1,
                CURRENT_TAB_ID,
                /* newIsIncognito= */ false,
                /* background= */ true,
                /* originX= */ 0f,
                /* originY= */ 0f);

        assertEquals(1, mBrowserVisibilityDelegate.showControlsPersistentCallCount);
        verify(mBrowserControlsManager).showAndroidControls(false);
        assertThat(mBrowserVisibilityDelegate.get()).isEqualTo(BrowserControlsState.SHOWN);

        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(1, mBrowserVisibilityDelegate.releasePersistentShowingTokenCallCount);
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

    private <T extends View> T findChildView(Class<T> clazz) {
        for (int i = 0; i < mAnimationHostView.getChildCount(); i++) {
            View child = mAnimationHostView.getChildAt(i);
            if (clazz.isInstance(child)) {
                return clazz.cast(child);
            }
        }
        return null;
    }

    private void setNtp() {
        when(mCurrentTab.getUrl()).thenReturn(new GURL("chrome://newtab"));
        when(mCurrentTab.getNativePage()).thenReturn(mNtp);
    }
}
