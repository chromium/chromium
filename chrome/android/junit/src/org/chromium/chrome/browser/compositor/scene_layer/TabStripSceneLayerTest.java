// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.nullable;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil.FOLIO_FOOT_LENGTH_DP;

import android.content.Context;
import android.graphics.Color;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import com.google.android.material.color.MaterialColors;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton.ButtonType;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView.StripLayoutViewOnClickHandler;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView.StripLayoutViewOnKeyboardFocusHandler;
import org.chromium.chrome.browser.compositor.overlays.strip.TabLoadTracker.TabLoadTrackerCallback;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.ui.resources.ResourceManager;

/** Tests for {@link TabStripSceneLayer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, qualifiers = "sw600dp")
public class TabStripSceneLayerTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabStripSceneLayer.Natives mTabStripSceneMock;
    @Mock private StripLayoutHelperManager mStripLayoutHelperManager;
    @Mock private ResourceManager mResourceManager;
    @Mock private LayerTitleCache mLayerTitleCache;
    @Mock private SceneLayer mSceneLayer;
    @Mock private StripLayoutViewOnClickHandler mOnClickHandler;
    @Mock private StripLayoutViewOnKeyboardFocusHandler mKeyboardFocusHandler;
    @Mock private TabLoadTrackerCallback mTabLoadTrackerCallback;
    @Mock private LayoutRenderHost mLayoutRenderHost;
    @Mock private LayoutUpdateHost mLayoutUpdateHost;
    @Mock private TintedCompositorButton mCloseButton;
    @Mock private StripLayoutGroupTitle mStripGroupTitle;

    private static final float DP_TO_PX = 1.f;

    private CompositorButton mModelSelectorButton;
    private TintedCompositorButton mNewTabButton;
    private Context mContext;
    private TabStripSceneLayer mTabStripSceneLayer;
    private StripLayoutTab mStripLayoutTab;
    private StripLayoutTab[] mStripLayoutTabs;
    private StripLayoutGroupTitle[] mStripGroupTitles;

    public TabStripSceneLayerTest() {}

    @Before
    public void beforeTest() {
        TabStripSceneLayerJni.setInstanceForTesting(mTabStripSceneMock);
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        TabStripSceneLayer.setTestFlag(true);
        initializeTest();
    }

    @After
    public void tearDown() {
        TabStripSceneLayer.setTestFlag(false);
    }

    private void initializeTest() {
        mTabStripSceneLayer = new TabStripSceneLayer(DP_TO_PX);
        when(mTabStripSceneMock.init(mTabStripSceneLayer)).thenReturn(1L);
        mModelSelectorButton =
                new TintedCompositorButton(
                        mContext,
                        ButtonType.INCOGNITO_SWITCHER,
                        null,
                        32.f,
                        32.f,
                        mOnClickHandler,
                        mKeyboardFocusHandler,
                        R.drawable.ic_incognito,
                        12.f);
        mNewTabButton =
                new TintedCompositorButton(
                        mContext,
                        ButtonType.NEW_TAB,
                        null,
                        32.f,
                        32.f,
                        mOnClickHandler,
                        mKeyboardFocusHandler,
                        R.drawable.ic_new_tab_button,
                        8.f);
        mStripLayoutTab =
                spy(
                        new StripLayoutTab(
                                mContext,
                                1,
                                mOnClickHandler,
                                mKeyboardFocusHandler,
                                mTabLoadTrackerCallback,
                                mLayoutUpdateHost,
                                false));
        mTabStripSceneLayer.initializeNativeForTesting();
        mStripLayoutTabs = new StripLayoutTab[] {mStripLayoutTab};
        mStripGroupTitles = new StripLayoutGroupTitle[] {mStripGroupTitle};
        when(mStripLayoutHelperManager.getNewTabButton()).thenReturn(mNewTabButton);
        when(mStripLayoutHelperManager.getModelSelectorButton()).thenReturn(mModelSelectorButton);
        when(mStripLayoutTab.getCloseButton()).thenReturn(mCloseButton);
        when(mStripGroupTitle.getKeyboardFocusRingColor())
                .thenReturn(
                        MaterialColors.getColor(
                                mContext, R.attr.colorPrimary, /* defaultValue= */ 0));
        when(mStripGroupTitle.getKeyboardFocusRingOffset())
                .thenReturn(
                        mContext.getResources()
                                .getDimensionPixelSize(R.dimen.tabstrip_keyfocus_offset));
        when(mStripGroupTitle.getKeyboardFocusRingWidth())
                .thenReturn(
                        mContext.getResources()
                                .getDimensionPixelSize(R.dimen.tabstrip_strokewidth));
    }

    @Test
    public void testSetContentTree() {
        mTabStripSceneLayer.setContentTree(mSceneLayer);
        verify(mTabStripSceneMock).setContentTree(1L, mTabStripSceneLayer, mSceneLayer);
    }

    @Test
    public void testPushAndUpdateStrip() {
        float leftPadding = 10f;
        float rightPadding = 20f;
        float topPadding = 5f;
        // Call the method being tested.
        mTabStripSceneLayer.pushAndUpdateStrip(
                mStripLayoutHelperManager,
                mLayerTitleCache,
                mResourceManager,
                mStripLayoutTabs,
                null,
                1.f,
                0,
                -1,
                Color.YELLOW,
                0.3f,
                leftPadding,
                rightPadding,
                topPadding);

        // Verify JNI calls.
        verify(mTabStripSceneMock)
                .updateModelSelectorButton(
                        1L,
                        mTabStripSceneLayer,
                        mModelSelectorButton.getResourceId(),
                        ((TintedCompositorButton) mModelSelectorButton).getBackgroundResourceId(),
                        mModelSelectorButton.getDrawX(),
                        mModelSelectorButton.getDrawY(),
                        true,
                        false,
                        ((TintedCompositorButton) mModelSelectorButton).getTint(),
                        ((TintedCompositorButton) mModelSelectorButton).getBackgroundTint(),
                        mModelSelectorButton.getOpacity(),
                        false,
                        R.drawable.circular_button_keyfocus,
                        MaterialColors.getColor(
                                mContext, R.attr.colorPrimary, /* defaultValue= */ 0),
                        mResourceManager);
        verify(mTabStripSceneMock)
                .updateNewTabButton(
                        eq(1L),
                        eq(mTabStripSceneLayer),
                        /* resourceId= */ anyInt(),
                        /* backgroundResourceId= */ anyInt(),
                        /* x= */ eq(mNewTabButton.getDrawX() * DP_TO_PX),
                        /* y= */ eq(mNewTabButton.getDrawY() * DP_TO_PX),
                        /* touchTargetOffset= */ anyFloat(),
                        /* visible= */ eq(mNewTabButton.isVisible()),
                        /* isHovered= */ eq(mNewTabButton.isHovered()),
                        /* tint= */ anyInt(),
                        /* backgroundTint= */ anyInt(),
                        /* buttonAlpha= */ anyFloat(),
                        /* isKeyboardFocused= */ eq(false),
                        /* keyboardFocusRingResourceId= */ eq(R.drawable.circular_button_keyfocus),
                        /* keyboardFocusRingColor= */ eq(
                                MaterialColors.getColor(
                                        mContext, R.attr.colorPrimary, /* defaultValue= */ 0)),
                        /* resourceManager= */ eq(mResourceManager));
        verify(mTabStripSceneMock)
                .updateTabStripLeftFade(
                        1L, mTabStripSceneLayer, 0, 0.f, mResourceManager, 0, leftPadding);
        verify(mTabStripSceneMock)
                .updateTabStripRightFade(
                        1L, mTabStripSceneLayer, 0, 0.f, mResourceManager, 0, rightPadding);
        verify(mTabStripSceneMock)
                .updateTabStripLayer(
                        eq(1L),
                        eq(mTabStripSceneLayer),
                        anyInt(),
                        anyInt(),
                        /* yOffset= */ eq(1.f),
                        anyInt(),
                        /* scrimColor= */ eq(Color.YELLOW),
                        /* scrimOpacity= */ eq(0.3f),
                        eq(leftPadding),
                        eq(rightPadding),
                        eq(topPadding));
    }

    @Test
    public void testUpdateStrip_tabNotFocusedTabInTabGroup_keyboardFocused() {
        when(mStripLayoutTab.isKeyboardFocused()).thenReturn(true);
        mTabStripSceneLayer.pushStripTabs(
                mStripLayoutHelperManager,
                mLayerTitleCache,
                mResourceManager,
                new StripLayoutTab[] {mStripLayoutTab},
                0,
                -1);
        verify(mTabStripSceneMock, times(1))
                .putStripTabLayer(
                        eq(1L),
                        eq(mTabStripSceneLayer),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(false),
                        eq(R.drawable.circular_button_keyfocus),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyBoolean(),
                        eq(false),
                        eq(false),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyBoolean(),
                        anyBoolean(),
                        anyBoolean(),
                        anyFloat(),
                        anyFloat(),
                        eq(true),
                        eq(R.drawable.tabstrip_keyfocus_8dp),
                        eq(
                                MaterialColors.getColor(
                                        mContext, R.attr.colorPrimary, /* defaultValue= */ 0)),
                        eq(
                                mContext.getResources()
                                        .getDimensionPixelSize(R.dimen.tabstrip_keyfocus_offset)),
                        eq(
                                mContext.getResources()
                                        .getDimensionPixelSize(R.dimen.tabstrip_strokewidth)),
                        eq(
                                FOLIO_FOOT_LENGTH_DP
                                        * mContext.getResources().getDisplayMetrics().density),
                        eq(mLayerTitleCache),
                        eq(mResourceManager));
    }

    @Test
    public void testUpdateStrip_tabNotFocusedTabInTabGroup_keyboardFocused_incognito() {
        when(mStripLayoutTab.isIncognito()).thenReturn(true);
        when(mStripLayoutTab.isKeyboardFocused()).thenReturn(true);
        mTabStripSceneLayer.pushStripTabs(
                mStripLayoutHelperManager,
                mLayerTitleCache,
                mResourceManager,
                new StripLayoutTab[] {mStripLayoutTab},
                0,
                -1);
        verify(mTabStripSceneMock, times(1))
                .putStripTabLayer(
                        eq(1L),
                        eq(mTabStripSceneLayer),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(false),
                        eq(R.drawable.circular_button_keyfocus),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyBoolean(),
                        eq(false),
                        eq(false),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyBoolean(),
                        anyBoolean(),
                        anyBoolean(),
                        anyFloat(),
                        anyFloat(),
                        eq(true),
                        eq(R.drawable.tabstrip_keyfocus_8dp),
                        eq(mContext.getColor(R.color.baseline_neutral_90)),
                        eq(
                                mContext.getResources()
                                        .getDimensionPixelSize(R.dimen.tabstrip_keyfocus_offset)),
                        eq(
                                mContext.getResources()
                                        .getDimensionPixelSize(R.dimen.tabstrip_strokewidth)),
                        eq(
                                FOLIO_FOOT_LENGTH_DP
                                        * mContext.getResources().getDisplayMetrics().density),
                        eq(mLayerTitleCache),
                        eq(mResourceManager));
    }

    @Test
    public void testUpdateStrip_focusedTabInTabGroup_keyboardFocused() {
        when(mStripLayoutTab.isKeyboardFocused()).thenReturn(true);
        when(mStripLayoutHelperManager.shouldShowTabOutline(mStripLayoutTab)).thenReturn(true);
        mTabStripSceneLayer.pushStripTabs(
                mStripLayoutHelperManager,
                mLayerTitleCache,
                mResourceManager,
                new StripLayoutTab[] {mStripLayoutTab},
                mStripLayoutTab.getTabId(),
                -1);
        verify(mTabStripSceneMock, times(1))
                .putStripTabLayer(
                        eq(1L),
                        eq(mTabStripSceneLayer),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(false),
                        eq(R.drawable.circular_button_keyfocus),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyBoolean(),
                        eq(true),
                        eq(false),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyBoolean(),
                        anyBoolean(),
                        anyBoolean(),
                        anyFloat(),
                        anyFloat(),
                        eq(true),
                        eq(R.drawable.tabstrip_keyfocus_10dp),
                        eq(
                                MaterialColors.getColor(
                                        mContext, R.attr.colorPrimary, /* defaultValue= */ 0)),
                        eq(
                                mContext.getResources()
                                        .getDimensionPixelSize(R.dimen.tabstrip_keyfocus_offset)),
                        eq(
                                mContext.getResources()
                                        .getDimensionPixelSize(R.dimen.tabstrip_strokewidth)),
                        eq(
                                FOLIO_FOOT_LENGTH_DP
                                        * mContext.getResources().getDisplayMetrics().density),
                        eq(mLayerTitleCache),
                        eq(mResourceManager));
    }

    @Test
    public void testUpdateStrip_closeButton_keyboardFocused() {
        when(mCloseButton.isKeyboardFocused()).thenReturn(true);
        mTabStripSceneLayer.pushStripTabs(
                mStripLayoutHelperManager,
                mLayerTitleCache,
                mResourceManager,
                new StripLayoutTab[] {mStripLayoutTab},
                0,
                -1);
        verify(mTabStripSceneMock, times(1))
                .putStripTabLayer(
                        eq(1L),
                        eq(mTabStripSceneLayer),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(true),
                        eq(R.drawable.circular_button_keyfocus),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyBoolean(),
                        eq(false),
                        eq(false),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyBoolean(),
                        anyBoolean(),
                        anyBoolean(),
                        anyFloat(),
                        anyFloat(),
                        eq(false),
                        eq(R.drawable.tabstrip_keyfocus_8dp),
                        eq(
                                MaterialColors.getColor(
                                        mContext, R.attr.colorPrimary, /* defaultValue= */ 0)),
                        eq(
                                mContext.getResources()
                                        .getDimensionPixelSize(R.dimen.tabstrip_keyfocus_offset)),
                        eq(
                                mContext.getResources()
                                        .getDimensionPixelSize(R.dimen.tabstrip_strokewidth)),
                        eq(
                                FOLIO_FOOT_LENGTH_DP
                                        * mContext.getResources().getDisplayMetrics().density),
                        eq(mLayerTitleCache),
                        eq(mResourceManager));
    }

    @Test
    public void testUpdateStrip_tabGroup_keyboardFocused() {
        when(mStripGroupTitle.isKeyboardFocused()).thenReturn(true);
        mTabStripSceneLayer.pushGroupIndicators(
                mStripGroupTitles, mLayerTitleCache, mResourceManager);
        verify(mTabStripSceneMock, times(1))
                .putGroupIndicatorLayer(
                        eq(1L),
                        eq(mTabStripSceneLayer),
                        anyBoolean(),
                        anyBoolean(),
                        anyBoolean(),
                        anyBoolean(),
                        nullable(Token.class),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        eq(true),
                        eq(R.drawable.tabstrip_keyfocus_11dp),
                        eq(
                                MaterialColors.getColor(
                                        mContext, R.attr.colorPrimary, /* defaultValue= */ 0)),
                        eq(
                                mContext.getResources()
                                        .getDimensionPixelSize(R.dimen.tabstrip_keyfocus_offset)),
                        eq(
                                mContext.getResources()
                                        .getDimensionPixelSize(R.dimen.tabstrip_strokewidth)),
                        eq(mLayerTitleCache),
                        eq(mResourceManager));
    }

    @Test
    public void testUpdateNewTabButton() {
        mNewTabButton.setKeyboardFocused(true);
        mTabStripSceneLayer.pushButtonsAndBackground(
                mStripLayoutHelperManager, mResourceManager, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
        verify(mTabStripSceneMock, times(1))
                .updateNewTabButton(
                        eq(1L),
                        eq(mTabStripSceneLayer),
                        anyInt(),
                        anyInt(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyBoolean(),
                        anyBoolean(),
                        anyInt(),
                        anyInt(),
                        anyFloat(),
                        eq(true),
                        eq(R.drawable.circular_button_keyfocus),
                        eq(
                                MaterialColors.getColor(
                                        mContext, R.attr.colorPrimary, /* defaultValue= */ 0)),
                        eq(mResourceManager));
    }

    @Test
    public void testUpdateModelSelectorButton() {
        mModelSelectorButton.setKeyboardFocused(true);
        mTabStripSceneLayer.pushButtonsAndBackground(
                mStripLayoutHelperManager, mResourceManager, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
        verify(mTabStripSceneMock, times(1))
                .updateModelSelectorButton(
                        eq(1L),
                        eq(mTabStripSceneLayer),
                        anyInt(),
                        anyInt(),
                        anyFloat(),
                        anyFloat(),
                        anyBoolean(),
                        anyBoolean(),
                        anyInt(),
                        anyInt(),
                        anyFloat(),
                        eq(true),
                        eq(R.drawable.circular_button_keyfocus),
                        eq(
                                MaterialColors.getColor(
                                        mContext, R.attr.colorPrimary, /* defaultValue= */ 0)),
                        eq(mResourceManager));
    }
}
