// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNotNull;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.animation.AnimatorSet;
import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.RectF;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.supplier.SyncOneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.Layout.ViewportMode;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.scene_layer.SolidColorSceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.SolidColorSceneLayerJni;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayerJni;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayerJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.resources.ResourceManager;

import java.util.Collections;
import java.util.function.DoubleConsumer;

/**
 * Unit tests for {@link HubLayout}.
 *
 * <p>TODO(crbug.com/40283200): Once integrated with LayoutManager we should also add integration
 * tests.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class HubLayoutUnitTest {
    private static final int DEFAULT_COLOR = 0xFFABCDEF;
    private static final int INCOGNITO_COLOR = 0xFF001122;
    private static final long FAKE_NATIVE_ADDRESS_1 = 498723734L;
    private static final long FAKE_NATIVE_ADDRESS_2 = 123210L;
    private static final float FLOAT_ERROR = 0.001f;
    private static final int TAB_ID = 5;
    private static final int NEW_TAB_ID = 6;
    private static final int NEW_TAB_INDEX = 0;
    // This animation doesn't depend on time from the LayoutManager.
    private static final long FAKE_TIME = 0L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private LayoutUpdateHost mUpdateHost;
    @Mock private LayoutRenderHost mRenderHost;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private ResourceManager mResourceManager;
    @Mock private SceneLayer.Natives mSceneLayerJni;
    @Mock private StaticTabSceneLayer.Natives mStaticTabSceneLayerJni;
    @Mock private SolidColorSceneLayer.Natives mSolidColorSceneLayerJni;
    @Mock private HubManager mHubManager;
    @Mock private HubController mHubController;
    @Mock private PaneManager mPaneManager;
    @Mock private HubLayoutScrimController mScrimController;
    @Mock private Pane mTabSwitcherPane;
    @Mock private Pane mIncognitoTabSwitcherPane;
    @Mock private Pane mTabGroupPane;
    @Mock private HubLayoutAnimator mHubLayoutAnimatorMock;
    @Mock private HubLayoutAnimatorProvider mHubLayoutAnimatorProviderMock;
    @Mock private Bitmap mBitmap;
    @Mock private Callback<Bitmap> mThumbnailCallback;
    @Mock private TabContentManager mTabContentManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Tab mTab;
    @Mock private DoubleConsumer mOnAlphaChange;
    @Mock private DesktopWindowStateProvider mDesktopWindowStateProvider;

    private UserActionTester mActionTester;

    private Activity mActivity;
    private FrameLayout mFrameLayout;

    private HubLayout mHubLayout;
    private HubContainerView mHubContainerView;

    private SyncOneshotSupplierImpl<HubLayoutAnimator> mHubLayoutAnimatorSupplier;
    private Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private ObservableSupplierImpl<Pane> mPaneSupplier = new ObservableSupplierImpl<>();
    private HubShowPaneHelper mHubShowPaneHelper;

    @Before
    public void setUp() {
        mJniMocker.mock(SceneLayerJni.TEST_HOOKS, mSceneLayerJni);
        mJniMocker.mock(StaticTabSceneLayerJni.TEST_HOOKS, mStaticTabSceneLayerJni);
        mJniMocker.mock(SolidColorSceneLayerJni.TEST_HOOKS, mSolidColorSceneLayerJni);

        mActionTester = new UserActionTester();
        ShadowLooper.runUiThreadTasks();

        when(mTabSwitcherPane.getPaneId()).thenReturn(PaneId.TAB_SWITCHER);
        when(mTabSwitcherPane.getColorScheme()).thenReturn(HubColorScheme.DEFAULT);
        when(mTabSwitcherPane.createShowHubLayoutAnimatorProvider(any()))
                .thenReturn(mHubLayoutAnimatorProviderMock);
        when(mTabSwitcherPane.createHideHubLayoutAnimatorProvider(any()))
                .thenReturn(mHubLayoutAnimatorProviderMock);
        when(mTabGroupPane.getPaneId()).thenReturn(PaneId.TAB_GROUPS);
        when(mTabGroupPane.getColorScheme()).thenReturn(HubColorScheme.DEFAULT);
        when(mTabGroupPane.createShowHubLayoutAnimatorProvider(any()))
                .thenReturn(mHubLayoutAnimatorProviderMock);
        when(mTabSwitcherPane.createHideHubLayoutAnimatorProvider(any()))
                .thenReturn(mHubLayoutAnimatorProviderMock);
        when(mIncognitoTabSwitcherPane.getPaneId()).thenReturn(PaneId.INCOGNITO_TAB_SWITCHER);
        when(mIncognitoTabSwitcherPane.getColorScheme()).thenReturn(HubColorScheme.INCOGNITO);
        when(mIncognitoTabSwitcherPane.createShowHubLayoutAnimatorProvider(any()))
                .thenReturn(mHubLayoutAnimatorProviderMock);
        when(mIncognitoTabSwitcherPane.createHideHubLayoutAnimatorProvider(any()))
                .thenReturn(mHubLayoutAnimatorProviderMock);

        when(mSceneLayerJni.init(any()))
                .thenReturn(FAKE_NATIVE_ADDRESS_1)
                .thenReturn(FAKE_NATIVE_ADDRESS_2);
        // Fake proper cleanup of the native ptr.
        doCallback(
                        /* index= */ 1,
                        (SceneLayer sceneLayer) -> {
                            sceneLayer.setNativePtr(0L);
                        })
                .when(mSceneLayerJni)
                .destroy(anyLong(), any());
        // Ensure each SceneLayer has a native ptr.
        doAnswer(
                        invocation -> {
                            ((SceneLayer) invocation.getArguments()[0])
                                    .setNativePtr(FAKE_NATIVE_ADDRESS_1);
                            return FAKE_NATIVE_ADDRESS_1;
                        })
                .when(mStaticTabSceneLayerJni)
                .init(any());
        doAnswer(
                        invocation -> {
                            ((SceneLayer) invocation.getArguments()[0])
                                    .setNativePtr(FAKE_NATIVE_ADDRESS_2);
                            return FAKE_NATIVE_ADDRESS_2;
                        })
                .when(mSolidColorSceneLayerJni)
                .init(any());

        when(mPaneManager.getFocusedPaneSupplier()).thenReturn(mPaneSupplier);
        doAnswer(
                        invocation -> {
                            int paneId = ((Integer) invocation.getArguments()[0]).intValue();
                            switch (paneId) {
                                case PaneId.TAB_SWITCHER:
                                    mPaneSupplier.set(mTabSwitcherPane);
                                    break;
                                case PaneId.INCOGNITO_TAB_SWITCHER:
                                    mPaneSupplier.set(mIncognitoTabSwitcherPane);
                                    break;
                                case PaneId.TAB_GROUPS:
                                    mPaneSupplier.set(mTabGroupPane);
                                    break;
                                default:
                                    fail("Invalid pane id" + paneId);
                            }
                            return true;
                        })
                .when(mPaneManager)
                .focusPane(anyInt());
        when(mHubManager.getPaneManager()).thenReturn(mPaneManager);
        when(mHubManager.getHubController()).thenReturn(mHubController);
        mHubShowPaneHelper = new HubShowPaneHelper();
        when(mHubManager.getHubShowPaneHelper()).thenReturn(mHubShowPaneHelper);
        doAnswer(
                        invocation -> {
                            Pane pane = (Pane) invocation.getArguments()[0];
                            if (pane == null) return DEFAULT_COLOR;

                            switch (pane.getColorScheme()) {
                                case HubColorScheme.DEFAULT:
                                    return DEFAULT_COLOR;
                                case HubColorScheme.INCOGNITO:
                                    return INCOGNITO_COLOR;
                                default:
                                    fail("Unexpected colorscheme " + pane.getColorScheme());
                                    return Color.TRANSPARENT;
                            }
                        })
                .when(mHubController)
                .getBackgroundColor(any());

        mActivityScenarioRule.getScenario().onActivity(this::onActivityCreated);

        doAnswer(
                        invocation -> {
                            var args = invocation.getArguments();
                            return new LayoutTab(
                                    (Integer) args[0],
                                    (Boolean) args[1],
                                    ((Float) args[2]).intValue(),
                                    ((Float) args[3]).intValue());
                        })
                .when(mUpdateHost)
                .createLayoutTab(anyInt(), anyBoolean(), anyFloat(), anyFloat());
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.isNativePage()).thenReturn(false);
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);

        mHubLayoutAnimatorSupplier = new SyncOneshotSupplierImpl<HubLayoutAnimator>();
        when(mHubLayoutAnimatorProviderMock.getAnimatorSupplier())
                .thenReturn(mHubLayoutAnimatorSupplier);
    }

    private void onActivityCreated(Activity activity) {
        mActivity = activity;
        mFrameLayout = new FrameLayout(mActivity);
        mHubContainerView = new HubContainerView(mActivity);
        View hubLayout = LayoutInflater.from(activity).inflate(R.layout.hub_layout, null);
        mHubContainerView.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));
        mHubContainerView.addView(hubLayout);
        mActivity.setContentView(mFrameLayout);

        View paneHostView = hubLayout.findViewById(R.id.hub_pane_host);
        when(mHubController.getContainerView()).thenReturn(mHubContainerView);
        when(mHubController.getPaneHostView()).thenReturn(paneHostView);

        LazyOneshotSupplier<HubManager> hubManagerSupplier =
                LazyOneshotSupplier.fromValue(mHubManager);
        LazyOneshotSupplier<ViewGroup> rootViewSupplier =
                LazyOneshotSupplier.fromValue(mFrameLayout);
        HubLayoutDependencyHolder dependencyHolder =
                new HubLayoutDependencyHolder(
                        hubManagerSupplier, rootViewSupplier, mScrimController, mOnAlphaChange);

        mTabModelSelectorSupplier = () -> mTabModelSelector;
        mHubLayout =
                spy(
                        new HubLayout(
                                mActivity,
                                mUpdateHost,
                                mRenderHost,
                                mLayoutStateProvider,
                                dependencyHolder,
                                mTabModelSelectorSupplier,
                                mDesktopWindowStateProvider));
        mHubLayout.setTabModelSelector(mTabModelSelector);
        mHubLayout.setTabContentManager(mTabContentManager);
        mHubLayout.onFinishNativeInitialization();
    }

    @After
    public void tearDown() {
        mHubLayout.destroy();
        mActionTester.tearDown();
    }

    @Test
    @SmallTest
    public void testFixedReturnValues() {
        // These are not expected to change. This is here to get unit test coverage.
        assertEquals(ViewportMode.ALWAYS_FULLSCREEN, mHubLayout.getViewportMode());
        assertTrue(mHubLayout.handlesTabClosing());
        assertTrue(mHubLayout.handlesTabCreating());
        assertNull(mHubLayout.getEventFilter());
        assertEquals(LayoutType.TAB_SWITCHER, mHubLayout.getLayoutType());

        // TODO(crbug.com/40283200): These may be dynamic after further development.
        assertFalse(mHubLayout.onBackPressed());
        assertTrue(mHubLayout.canHostBeFocusable());
    }

    @Test
    @SmallTest
    public void testUpdateSceneLayerAndLayoutTabsDuringShow() {
        setupHubLayoutAnimatorAndProvider(HubLayoutAnimationType.FADE_IN);
        animateCheckingSceneLayerAndLayoutTabs(
                () -> startShowing(LayoutType.BROWSING, true), TAB_ID);
        verify(mTabContentManager)
                .updateVisibleIds(eq(Collections.emptyList()), eq(Tab.INVALID_TAB_ID));
    }

    @Test
    @SmallTest
    public void testUpdateSceneLayerAndLayoutTabsDuringHide() {
        setupHubLayoutAnimatorAndProvider(HubLayoutAnimationType.FADE_OUT);
        animateCheckingSceneLayerAndLayoutTabs(
                () -> startHiding(LayoutType.BROWSING, NEW_TAB_ID), NEW_TAB_ID);
        verify(mTabContentManager, never())
                .updateVisibleIds(eq(Collections.emptyList()), eq(Tab.INVALID_TAB_ID));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testShowTablet() {
        show(LayoutType.BROWSING, true, HubLayoutAnimationType.TRANSLATE_UP);
        verify(mTabContentManager).cacheTabThumbnailWithCallback(any(), anyBoolean(), any());
    }

    @Test
    @SmallTest
    public void testShowWithNoSelectedPane() {
        setupHubLayoutAnimatorAndProvider(HubLayoutAnimationType.SHRINK_TAB);
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        show(LayoutType.BROWSING, true, HubLayoutAnimationType.SHRINK_TAB);
        verify(mTabContentManager).cacheTabThumbnailWithCallback(any(), anyBoolean(), any());
        verify(mPaneManager).focusPane(PaneId.TAB_SWITCHER);

        verify(mSolidColorSceneLayerJni).setBackgroundColor(FAKE_NATIVE_ADDRESS_2, DEFAULT_COLOR);
    }

    @Test
    @SmallTest
    public void testShowWithSelectedPane() {
        setupHubLayoutAnimatorAndProvider(HubLayoutAnimationType.SHRINK_TAB);

        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        mHubShowPaneHelper.setPaneToShow(PaneId.TAB_GROUPS);
        show(LayoutType.BROWSING, true, HubLayoutAnimationType.SHRINK_TAB);
        verify(mTabContentManager).cacheTabThumbnailWithCallback(any(), anyBoolean(), any());
        verify(mPaneManager).focusPane(PaneId.TAB_GROUPS);

        verify(mSolidColorSceneLayerJni).setBackgroundColor(FAKE_NATIVE_ADDRESS_2, DEFAULT_COLOR);
    }

    @Test
    @SmallTest
    public void testShowWithIncognitoPane() {
        setupHubLayoutAnimatorAndProvider(HubLayoutAnimationType.SHRINK_TAB);
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(true);
        show(LayoutType.BROWSING, true, HubLayoutAnimationType.SHRINK_TAB);
        verify(mTabContentManager).cacheTabThumbnailWithCallback(any(), anyBoolean(), any());
        verify(mPaneManager).focusPane(PaneId.INCOGNITO_TAB_SWITCHER);

        verify(mSolidColorSceneLayerJni).setBackgroundColor(FAKE_NATIVE_ADDRESS_2, INCOGNITO_COLOR);
    }

    @Test
    @SmallTest
    public void testShowFromBrowsingWithThumbnailCallback() {
        setupHubLayoutAnimatorAndProvider(HubLayoutAnimationType.SHRINK_TAB);
        when(mHubLayoutAnimatorProviderMock.getThumbnailCallback()).thenReturn(mThumbnailCallback);

        // Successfully capture a bitmap.
        doCallback(
                        /* index= */ 2,
                        (Callback<Bitmap> bitmapCallback) -> {
                            bitmapCallback.onResult(mBitmap);
                        })
                .when(mTabContentManager)
                .cacheTabThumbnailWithCallback(any(), eq(true), any());

        show(LayoutType.BROWSING, true, HubLayoutAnimationType.SHRINK_TAB);

        InOrder inOrder = inOrder(mTabContentManager, mHubController);
        inOrder.verify(mTabContentManager).cacheTabThumbnailWithCallback(any(), eq(true), any());
        inOrder.verify(mHubController).onHubLayoutShow();

        verify(mThumbnailCallback).bind(isNotNull());
    }

    @Test
    @SmallTest
    public void testShowFromBrowsingWithFallbackNativePageThumbnailCallback() {
        setupHubLayoutAnimatorAndProvider(HubLayoutAnimationType.SHRINK_TAB);
        when(mHubLayoutAnimatorProviderMock.getThumbnailCallback()).thenReturn(mThumbnailCallback);
        when(mTab.isNativePage()).thenReturn(true);

        // Fail to capture a bitmap.
        doCallback(
                        /* index= */ 2,
                        (Callback<Bitmap> bitmapCallback) -> {
                            bitmapCallback.onResult(null);
                        })
                .when(mTabContentManager)
                .cacheTabThumbnailWithCallback(any(), eq(true), any());

        // Succeed on the NativePage fallback thumbnail attempt.
        doCallback(
                        /* index= */ 1,
                        (Callback<Bitmap> bitmapCallback) -> {
                            bitmapCallback.onResult(mBitmap);
                        })
                .when(mTabContentManager)
                .getEtc1TabThumbnailWithCallback(eq(TAB_ID), any());

        show(LayoutType.BROWSING, true, HubLayoutAnimationType.SHRINK_TAB);

        InOrder inOrder = inOrder(mTabContentManager, mHubController);
        inOrder.verify(mTabContentManager).cacheTabThumbnailWithCallback(any(), eq(true), any());
        inOrder.verify(mHubController).onHubLayoutShow();

        verify(mThumbnailCallback).bind(isNotNull());
    }

    @Test
    @SmallTest
    public void testShowFromBrowsingWithoutFallbackThumbnailCallback() {
        setupHubLayoutAnimatorAndProvider(HubLayoutAnimationType.SHRINK_TAB);
        when(mHubLayoutAnimatorProviderMock.getThumbnailCallback()).thenReturn(mThumbnailCallback);

        // Fail to capture the bitmap and since this is not a native page there is no fallback.
        doCallback(
                        /* index= */ 2,
                        (Callback<Bitmap> bitmapCallback) -> {
                            bitmapCallback.onResult(null);
                        })
                .when(mTabContentManager)
                .cacheTabThumbnailWithCallback(any(), eq(true), any());

        show(LayoutType.BROWSING, true, HubLayoutAnimationType.SHRINK_TAB);

        InOrder inOrder = inOrder(mTabContentManager, mHubController);
        inOrder.verify(mTabContentManager).cacheTabThumbnailWithCallback(any(), eq(true), any());
        inOrder.verify(mHubController).onHubLayoutShow();

        verify(mThumbnailCallback).bind(isNull());
        verify(mTabContentManager, never()).getEtc1TabThumbnailWithCallback(anyInt(), any());
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testHideTablet() {
        hide(
                LayoutType.BROWSING,
                TAB_ID,
                /* skipStartHiding= */ false,
                HubLayoutAnimationType.TRANSLATE_DOWN);
        verify(mTabContentManager, never()).getEtc1TabThumbnailWithCallback(anyInt(), any());
    }

    @Test
    @SmallTest
    public void testHideWithNoPane() {
        hide(
                LayoutType.BROWSING,
                Tab.INVALID_TAB_ID,
                /* skipStartHiding= */ false,
                HubLayoutAnimationType.FADE_OUT);
        verify(mTabContentManager, never()).getEtc1TabThumbnailWithCallback(anyInt(), any());
    }

    @Test
    @SmallTest
    public void testHideViaNewTab() {
        forceLayout();
        mHubLayout.onTabCreated(FAKE_TIME, NEW_TAB_ID, NEW_TAB_INDEX, TAB_ID, false, false, 0, 0);
        hide(
                LayoutType.BROWSING,
                NEW_TAB_ID,
                /* skipStartHiding= */ true,
                HubLayoutAnimationType.EXPAND_NEW_TAB);
        verify(mTabContentManager, never()).getEtc1TabThumbnailWithCallback(anyInt(), any());
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testHideViaNewTabTablet() {
        mHubLayout.onTabCreated(FAKE_TIME, NEW_TAB_ID, NEW_TAB_INDEX, TAB_ID, false, false, 0, 0);
        hide(
                LayoutType.BROWSING,
                NEW_TAB_ID,
                /* skipStartHiding= */ true,
                HubLayoutAnimationType.TRANSLATE_DOWN);
        verify(mTabContentManager, never()).getEtc1TabThumbnailWithCallback(anyInt(), any());
    }

    @Test
    @SmallTest
    public void testHideToBrowsingThumbnailCallback() {
        setupHubLayoutAnimatorAndProvider(HubLayoutAnimationType.EXPAND_TAB);
        mPaneSupplier.set(mTabSwitcherPane);
        when(mHubLayoutAnimatorProviderMock.getThumbnailCallback()).thenReturn(mThumbnailCallback);
        when(mTab.isNativePage()).thenReturn(true);

        // Succeed on the thumbnail attempt
        doCallback(
                        /* index= */ 1,
                        (Callback<Bitmap> bitmapCallback) -> {
                            bitmapCallback.onResult(mBitmap);
                        })
                .when(mTabContentManager)
                .getEtc1TabThumbnailWithCallback(eq(TAB_ID), any());

        hide(
                LayoutType.BROWSING,
                TAB_ID,
                /* skipStartHiding= */ false,
                HubLayoutAnimationType.EXPAND_TAB);

        verify(mThumbnailCallback).onResult(isNotNull());
    }

    @Test
    @SmallTest
    public void testHideToBrowsingThumbnailCallbackNoTabIdInStartHiding() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID);

        setupHubLayoutAnimatorAndProvider(HubLayoutAnimationType.EXPAND_TAB);
        mPaneSupplier.set(mTabSwitcherPane);
        when(mHubLayoutAnimatorProviderMock.getThumbnailCallback()).thenReturn(mThumbnailCallback);
        doReturn(mHubLayoutAnimatorProviderMock).when(mHubLayout).createHideAnimatorProvider(any());
        when(mTab.isNativePage()).thenReturn(true);

        // Succeed on the thumbnail attempt
        doCallback(
                        /* index= */ 1,
                        (Callback<Bitmap> bitmapCallback) -> {
                            bitmapCallback.onResult(mBitmap);
                        })
                .when(mTabContentManager)
                .getEtc1TabThumbnailWithCallback(eq(TAB_ID), any());

        hide(
                LayoutType.BROWSING,
                Tab.INVALID_TAB_ID,
                /* skipStartHiding= */ false,
                HubLayoutAnimationType.EXPAND_TAB);

        verify(mThumbnailCallback).onResult(isNotNull());
    }

    @Test
    @SmallTest
    public void testShowInterruptedByHide() {
        mPaneSupplier.set(mTabSwitcherPane);
        assertFalse(mHubLayout.isRunningAnimations());
        assertFalse(mHubLayout.onUpdateAnimation(FAKE_TIME, false));

        setupHubLayoutAnimatorAndProvider(HubLayoutAnimationType.FADE_IN);
        startShowing(LayoutType.BROWSING, true);

        verify(mHubController, times(1)).onHubLayoutShow();
        assertEquals(1, mFrameLayout.getChildCount());

        assertEquals(HubLayoutAnimationType.FADE_IN, mHubLayout.getCurrentAnimationType());
        assertTrue(mHubLayout.isRunningAnimations());
        assertTrue(mHubLayout.onUpdateAnimation(FAKE_TIME, false));

        setupHubLayoutAnimatorAndProvider(HubLayoutAnimationType.FADE_OUT);
        startHiding(LayoutType.BROWSING, NEW_TAB_ID);
        verify(mHubLayout).doneShowing();
        verify(mTab, never()).hide(anyInt());
        verify(mScrimController).forceAnimationToFinish();

        assertEquals(HubLayoutAnimationType.FADE_OUT, mHubLayout.getCurrentAnimationType());
        assertTrue(mHubLayout.isRunningAnimations());
        assertTrue(mHubLayout.onUpdateAnimation(FAKE_TIME, false));

        ShadowLooper.runUiThreadTasks();

        assertFalse(mHubLayout.isRunningAnimations());
        assertFalse(mHubLayout.onUpdateAnimation(FAKE_TIME, false));

        verify(mHubController, times(1)).onHubLayoutDoneHiding();
        assertEquals(0, mFrameLayout.getChildCount());
        verify(mHubLayout).doneHiding();
        verify(mTab, never()).hide(anyInt());
    }

    private void show(
            @LayoutType int fromLayout,
            boolean animate,
            @HubLayoutAnimationType int expectedAnimationType) {
        assertFalse(mHubLayout.isRunningAnimations());
        assertFalse(mHubLayout.onUpdateAnimation(FAKE_TIME, false));
        assertFalse(mHubLayout.forceHideBrowserControlsAndroidView());

        startShowing(fromLayout, animate);

        verify(mHubController, times(1)).onHubLayoutShow();
        assertEquals(1, mFrameLayout.getChildCount());

        if (animate) {
            assertEquals(expectedAnimationType, mHubLayout.getCurrentAnimationType());
            assertTrue(mHubLayout.isRunningAnimations());
            assertTrue(mHubLayout.onUpdateAnimation(FAKE_TIME, false));
        } else {
            assertFalse(mHubLayout.isRunningAnimations());
        }

        ShadowLooper.runUiThreadTasks();

        assertFalse(mHubLayout.isRunningAnimations());
        assertFalse(mHubLayout.onUpdateAnimation(FAKE_TIME, false));
        verify(mHubLayout).doneShowing();
        assertTrue(mHubLayout.forceHideBrowserControlsAndroidView());
        assertEquals(1, mActionTester.getActionCount("MobileToolbarShowStackView"));
        verify(mTab).hide(eq(TabHidingType.TAB_SWITCHER_SHOWN));
        verify(mScrimController, never()).forceAnimationToFinish();
    }

    private void hide(
            @LayoutType int nextLayout,
            int nextTabId,
            boolean skipStartHiding,
            @HubLayoutAnimationType int expectedAnimationType) {
        if (skipStartHiding) {
            assertTrue(mHubLayout.isRunningAnimations());
            assertTrue(mHubLayout.onUpdateAnimation(FAKE_TIME, false));
        } else {
            assertFalse(mHubLayout.isRunningAnimations());
            assertFalse(mHubLayout.onUpdateAnimation(FAKE_TIME, false));
            startHiding(nextLayout, nextTabId);
            assertFalse(mHubLayout.forceHideBrowserControlsAndroidView());
        }

        assertEquals(expectedAnimationType, mHubLayout.getCurrentAnimationType());
        assertTrue(mHubLayout.isRunningAnimations());
        assertTrue(mHubLayout.onUpdateAnimation(FAKE_TIME, false));
        forceLayout();

        ShadowLooper.runUiThreadTasks();

        assertFalse(mHubLayout.isRunningAnimations());
        assertFalse(mHubLayout.onUpdateAnimation(FAKE_TIME, false));

        verify(mHubController, times(1)).onHubLayoutDoneHiding();
        assertEquals(0, mFrameLayout.getChildCount());
        verify(mHubLayout).doneHiding();
        assertFalse(mHubLayout.forceHideBrowserControlsAndroidView());
        assertEquals(1, mActionTester.getActionCount("MobileExitStackView"));

        verify(mScrimController, never()).forceAnimationToFinish();
    }

    private void startShowing(@LayoutType int fromLayout, boolean animate) {
        when(mLayoutStateProvider.getActiveLayoutType()).thenReturn(fromLayout);
        mHubLayout.contextChanged(mActivity);
        assertEquals(fromLayout, mHubLayout.getPreviousLayoutTypeSupplier().get().intValue());

        mHubLayout.show(FAKE_TIME, animate);
    }

    private void startHiding(@LayoutType int nextLayout, int nextTabId) {
        @LayoutType int layoutType = mHubLayout.getLayoutType();
        when(mLayoutStateProvider.getActiveLayoutType()).thenReturn(layoutType);
        when(mLayoutStateProvider.getNextLayoutType()).thenReturn(nextLayout);

        // This selection happens before anything else in selectTabAndHideHubLayout. Mock it before
        // the call.
        if (nextTabId != Tab.INVALID_TAB_ID) {
            when(mTabModelSelector.getCurrentTabId()).thenReturn(nextTabId);
        }
        mHubLayout.selectTabAndHideHubLayout(nextTabId);
    }

    private void animateCheckingSceneLayerAndLayoutTabs(
            Runnable startAnimationRunnable, int tabId) {
        assertThat(mHubLayout.getSceneLayer(), instanceOf(SolidColorSceneLayer.class));
        LayoutTab[] layoutTabs = mHubLayout.getLayoutTabsToRender();
        assertNull(layoutTabs);

        mHubLayout.updateLayout(FAKE_TIME, FAKE_TIME);
        verify(mUpdateHost, never()).requestUpdate();

        startAnimationRunnable.run();

        assertThat(mHubLayout.getSceneLayer(), instanceOf(StaticTabSceneLayer.class));
        layoutTabs = mHubLayout.getLayoutTabsToRender();
        assertEquals(1, layoutTabs.length);
        assertEquals(tabId, layoutTabs[0].getId());
        verify(mTabContentManager)
                .updateVisibleIds(eq(Collections.singletonList(tabId)), eq(Tab.INVALID_TAB_ID));

        assertEquals(0f, layoutTabs[0].get(LayoutTab.CONTENT_OFFSET), FLOAT_ERROR);

        float contentOffset = 100f;
        when(mBrowserControlsStateProvider.getContentOffset())
                .thenReturn(Math.round(contentOffset));
        mHubLayout.updateSceneLayer(
                new RectF(),
                new RectF(),
                mTabContentManager,
                mResourceManager,
                mBrowserControlsStateProvider);
        assertEquals(contentOffset, layoutTabs[0].get(LayoutTab.CONTENT_OFFSET), FLOAT_ERROR);

        // Change this so updateSnap() returns true.
        layoutTabs[0].set(LayoutTab.RENDER_X, 5);
        mHubLayout.updateLayout(FAKE_TIME, FAKE_TIME);
        verify(mUpdateHost).requestUpdate();

        mHubContainerView.runOnNextLayoutRunnables();
        ShadowLooper.runUiThreadTasks();

        assertThat(mHubLayout.getSceneLayer(), instanceOf(SolidColorSceneLayer.class));
        layoutTabs = mHubLayout.getLayoutTabsToRender();
        assertNull(layoutTabs);
    }

    private void setupHubLayoutAnimatorAndProvider(@HubLayoutAnimationType int animationType) {
        AnimatorSet animatorSet = new AnimatorSet();
        when(mHubLayoutAnimatorMock.getAnimationType()).thenReturn(animationType);
        when(mHubLayoutAnimatorMock.getAnimatorSet()).thenReturn(animatorSet);
        when(mHubLayoutAnimatorProviderMock.getPlannedAnimationType()).thenReturn(animationType);
        mHubLayoutAnimatorSupplier = new SyncOneshotSupplierImpl<>();
        mHubLayoutAnimatorSupplier.set(mHubLayoutAnimatorMock);
        when(mHubLayoutAnimatorProviderMock.getAnimatorSupplier())
                .thenReturn(mHubLayoutAnimatorSupplier);
    }

    private void forceLayout() {
        // Force any layout delayed animations to run.
        mHubContainerView.layout(0, 0, 100, 100);
        for (int i = 0; i < mHubContainerView.getChildCount(); i++) {
            mHubContainerView.getChildAt(i).layout(0, 0, 100, 100);
        }
    }
}
