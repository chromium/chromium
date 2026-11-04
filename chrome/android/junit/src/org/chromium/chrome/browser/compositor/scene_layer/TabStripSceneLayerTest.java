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
import android.content.res.Resources;
import android.graphics.Color;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import com.google.android.material.color.MaterialColors;

import org.jni_zero.JniRawPtr;
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
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton.ButtonType;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton.TooltipHandler;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorTextButton;
import org.chromium.chrome.browser.compositor.overlays.strip.NtbWebUiCoordinator;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelper;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTrailingButtonsCoordinator;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView.StripLayoutViewOnClickHandler;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView.StripLayoutViewOnKeyboardFocusHandler;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView.StripLayoutViewOnLongClickHandler;
import org.chromium.chrome.browser.compositor.overlays.strip.TabLoadTracker.TabLoadTrackerCallback;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.tabs.TabAlert;
import org.chromium.ui.resources.ResourceManager;

/** Tests for {@link TabStripSceneLayer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "sw600dp")
public class TabStripSceneLayerTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabStripSceneLayer.Natives mTabStripSceneMock;
    @Mock private StripLayoutHelperManager mStripLayoutHelperManager;
    @Mock private ResourceManager mResourceManager;
    @Mock private LayerTitleCache mLayerTitleCache;
    @Mock private SceneLayer mSceneLayer;
    @Mock private TooltipHandler mTooltipHandler;
    @Mock private StripLayoutViewOnClickHandler mOnClickHandler;
    @Mock private StripLayoutViewOnLongClickHandler mOnLongClickHandler;
    @Mock private StripLayoutViewOnKeyboardFocusHandler mKeyboardFocusHandler;
    @Mock private TabLoadTrackerCallback mTabLoadTrackerCallback;
    @Mock private LayoutUpdateHost mLayoutUpdateHost;
    private TintedCompositorButton mGlicDismissButton;
    @Mock private TintedCompositorButton mCloseButton;
    @Mock private StripLayoutGroupTitle mStripGroupTitle;
    @Mock private StripLayoutTrailingButtonsCoordinator mTrailingButtonsCoordinator;
    @Mock private StripLayoutHelper mStripLayoutHelper;
    @Mock private TintedCompositorButton mTabSearchButton;

    private static final float DP_TO_PX = 1.f;

    private TintedCompositorTextButton mGlicButton;
    private TintedCompositorButton mModelSelectorButton;
    private TintedCompositorButton mNewTabButton;
    private Context mContext;
    private TabStripSceneLayer mTabStripSceneLayer;
    private JniRawPtr<TabStripSceneLayer.NativeTabStripSceneLayer> mNativePtr;
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
        mNativePtr.release();
        TabStripSceneLayer.setTestFlag(false);
    }

    private void initializeTest() {
        mNativePtr = JniRawPtr.createForTesting(1L);
        mTabStripSceneLayer = new TabStripSceneLayer(DP_TO_PX, /* activityCreationUptimeMs= */ 0L);
        when(mTabStripSceneMock.init(mTabStripSceneLayer)).thenReturn(mNativePtr);
        mGlicDismissButton =
                new TintedCompositorButton(
                        mContext,
                        false,
                        ButtonType.GLIC_DISMISS_NUDGE,
                        null,
                        24.f,
                        24.f,
                        mTooltipHandler,
                        mOnClickHandler,
                        mKeyboardFocusHandler,
                        R.drawable.btn_close,
                        Resources.ID_NULL,
                        8.f);
        mGlicButton =
                new TintedCompositorTextButton(
                        mContext,
                        false,
                        ButtonType.GLIC,
                        null,
                        32.f,
                        32.f,
                        mTooltipHandler,
                        mOnClickHandler,
                        mKeyboardFocusHandler,
                        R.drawable.ic_spark_4c_16dp,
                        8.f,
                        /* hasLongClickAction= */ false,
                        mGlicDismissButton);
        mModelSelectorButton =
                new TintedCompositorButton(
                        mContext,
                        false,
                        ButtonType.INCOGNITO_SWITCHER,
                        null,
                        32.f,
                        32.f,
                        mTooltipHandler,
                        mOnClickHandler,
                        mKeyboardFocusHandler,
                        R.drawable.ic_incognito,
                        R.drawable.bg_circle_tab_strip_button,
                        8.f);
        mNewTabButton =
                new TintedCompositorButton(
                        mContext,
                        false,
                        ButtonType.NEW_TAB,
                        null,
                        32.f,
                        32.f,
                        mTooltipHandler,
                        mOnClickHandler,
                        mKeyboardFocusHandler,
                        R.drawable.ic_new_tab_button,
                        R.drawable.bg_circle_tab_strip_button,
                        8.f);
        mStripLayoutTab =
                spy(
                        new StripLayoutTab(
                                mContext,
                                1,
                                mOnClickHandler,
                                mOnLongClickHandler,
                                mKeyboardFocusHandler,
                                /* accessibilityFocusHandler= */ null,
                                mTabLoadTrackerCallback,
                                mLayoutUpdateHost,
                                false,
                                false,
                                /* alertState= */ TabAlert.NONE));
        mTabStripSceneLayer.initializeNativeForTesting();
        mStripLayoutTabs = new StripLayoutTab[] {mStripLayoutTab};
        mStripGroupTitles = new StripLayoutGroupTitle[] {mStripGroupTitle};
        when(mStripLayoutHelperManager.getNewTabButton()).thenReturn(mNewTabButton);
        when(mStripLayoutHelperManager.getActiveStripLayoutHelper()).thenReturn(mStripLayoutHelper);
        when(mStripLayoutHelper.getTabSearchButton()).thenReturn(mTabSearchButton);
        when(mTrailingButtonsCoordinator.getGlicButton()).thenReturn(mGlicButton);
        when(mTrailingButtonsCoordinator.getModelSelectorButton()).thenReturn(mModelSelectorButton);
        when(mStripLayoutHelperManager.getContext()).thenReturn(mContext);
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
        verify(mTabStripSceneMock).setContentTree(mNativePtr, mSceneLayer);
    }

    @Test
    public void testPushAndUpdateStrip() {
        float leftPadding = 10f;
        float rightPadding = 20f;
        float topPadding = 5f;
        // Call the method being tested.
        mTabStripSceneLayer.pushAndUpdateStrip(
                mStripLayoutHelperManager,
                mTrailingButtonsCoordinator,
                mLayerTitleCache,
                mResourceManager,
                mStripLayoutTabs,
                null,
                1.f,
                0,
                Color.YELLOW,
                0.3f,
                leftPadding,
                rightPadding,
                topPadding);

        // Verify JNI calls.
        verify(mTabStripSceneMock)
                .beginBuildingFrame(
                        eq(mNativePtr), anyBoolean(), eq(mResourceManager), eq(mLayerTitleCache));
        verify(mTabStripSceneMock)
                .updateModelSelectorButton(
                        mNativePtr,
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
                                mContext, R.attr.colorPrimary, /* defaultValue= */ 0));
        verify(mTabStripSceneMock)
                .updateNewTabButton(
                        eq(mNativePtr),
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
                                        mContext, R.attr.colorPrimary, /* defaultValue= */ 0)));
        verify(mTabStripSceneMock)
                .updateTabStripFade(
                        /* self= */ mNativePtr,
                        /* isLeft= */ true,
                        /* fadeColor= */ 0,
                        /* opacity= */ 0.f,
                        /* gradientWidthPx= */ 0.f,
                        /* opaqueWidthPx= */ 0.f,
                        leftPadding);
        verify(mTabStripSceneMock)
                .updateTabStripFade(
                        /* self= */ mNativePtr,
                        /* isLeft= */ false,
                        /* fadeColor= */ 0,
                        /* opacity= */ 0.f,
                        /* gradientWidthPx= */ 0.f,
                        /* opaqueWidthPx= */ 0.f,
                        rightPadding);
        verify(mTabStripSceneMock)
                .updateTabStripLayer(
                        eq(mNativePtr),
                        anyInt(),
                        anyInt(),
                        /* yOffset= */ eq(1.f),
                        anyInt(),
                        /* scrimColor= */ eq(Color.YELLOW),
                        /* scrimOpacity= */ eq(0.3f),
                        eq(leftPadding),
                        eq(rightPadding),
                        eq(topPadding));
        verify(mTabStripSceneMock).finishBuildingFrame(mNativePtr);
    }

    @Test
    public void testUpdateStrip_tabNotFocusedTabInTabGroup_keyboardFocused() {
        when(mStripLayoutTab.isKeyboardFocused()).thenReturn(true);
        mTabStripSceneLayer.pushStripTabs(
                mStripLayoutHelperManager, new StripLayoutTab[] {mStripLayoutTab}, 0);
        verify(mTabStripSceneMock, times(1))
                .putStripTabLayer(
                        eq(mNativePtr),
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
                        anyBoolean(),
                        anyBoolean(),
                        anyInt(),
                        anyInt(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
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
                        anyBoolean(),
                        anyFloat(),
                        anyFloat(),
                        anyInt(),
                        anyInt(),
                        anyInt());
    }

    @Test
    public void testUpdateStrip_tabNotFocusedTabInTabGroup_keyboardFocused_incognito() {
        when(mStripLayoutTab.isIncognito()).thenReturn(true);
        when(mStripLayoutTab.isKeyboardFocused()).thenReturn(true);
        mTabStripSceneLayer.pushStripTabs(
                mStripLayoutHelperManager, new StripLayoutTab[] {mStripLayoutTab}, 0);
        verify(mTabStripSceneMock, times(1))
                .putStripTabLayer(
                        eq(mNativePtr),
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
                        anyBoolean(),
                        anyBoolean(),
                        anyInt(),
                        anyInt(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
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
                        anyBoolean(),
                        anyFloat(),
                        anyFloat(),
                        anyInt(),
                        anyInt(),
                        anyInt());
    }

    @Test
    public void testUpdateStrip_focusedTabInTabGroup_keyboardFocused() {
        when(mStripLayoutTab.isKeyboardFocused()).thenReturn(true);
        when(mStripLayoutHelperManager.shouldShowTabOutline(mStripLayoutTab)).thenReturn(true);
        mTabStripSceneLayer.pushStripTabs(
                mStripLayoutHelperManager,
                new StripLayoutTab[] {mStripLayoutTab},
                mStripLayoutTab.getTabId());
        verify(mTabStripSceneMock, times(1))
                .putStripTabLayer(
                        eq(mNativePtr),
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
                        anyBoolean(),
                        anyBoolean(),
                        anyInt(),
                        anyInt(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
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
                        anyBoolean(),
                        anyFloat(),
                        anyFloat(),
                        anyInt(),
                        anyInt(),
                        anyInt());
    }

    @Test
    public void testUpdateStrip_closeButton_keyboardFocused() {
        when(mCloseButton.isKeyboardFocused()).thenReturn(true);
        mTabStripSceneLayer.pushStripTabs(
                mStripLayoutHelperManager, new StripLayoutTab[] {mStripLayoutTab}, 0);
        verify(mTabStripSceneMock, times(1))
                .putStripTabLayer(
                        eq(mNativePtr),
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
                        anyBoolean(),
                        anyBoolean(),
                        anyInt(),
                        anyInt(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
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
                        anyBoolean(),
                        anyFloat(),
                        anyFloat(),
                        anyInt(),
                        anyInt(),
                        anyInt());
    }

    @Test
    public void testUpdateStrip_tabGroup_keyboardFocused() {
        when(mStripGroupTitle.isKeyboardFocused()).thenReturn(true);
        mTabStripSceneLayer.pushGroupIndicators(mStripGroupTitles);
        verify(mTabStripSceneMock, times(1))
                .putGroupIndicatorLayer(
                        eq(mNativePtr),
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
                                        .getDimensionPixelSize(R.dimen.tabstrip_strokewidth)));
    }

    @Test
    public void testUpdateNewTabButton() {
        mNewTabButton.setKeyboardFocused(true);
        mTabStripSceneLayer.pushButtonsAndBackground(
                mStripLayoutHelperManager,
                mTrailingButtonsCoordinator,
                0,
                0,
                0.0f,
                0.0f,
                0.0f,
                0.0f);
        verify(mTabStripSceneMock, times(1))
                .updateNewTabButton(
                        eq(mNativePtr),
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
                                        mContext, R.attr.colorPrimary, /* defaultValue= */ 0)));
    }

    @Test
    public void testUpdateNtbWebUi() {
        float topPadding = 5.f;
        mNewTabButton.setDrawX(100.f);
        mNewTabButton.setDrawY(3.f);
        mNewTabButton.setTouchTargetInsets(null, topPadding, null, -topPadding);
        JniRawPtr<NtbWebUiCoordinator.NativeTabStripNtbWebUiHost> ntbWebUiHost =
                JniRawPtr.createForTesting(2L);
        when(mStripLayoutHelperManager.getNtbWebUiNativePtr()).thenReturn(ntbWebUiHost);

        float topPaddingPx = topPadding * DP_TO_PX;
        mTabStripSceneLayer.pushButtonsAndBackground(
                mStripLayoutHelperManager,
                mTrailingButtonsCoordinator,
                0,
                0,
                0.0f,
                0.0f,
                0.0f,
                topPaddingPx);

        var touchTarget = mNewTabButton.getTouchTargetBounds();
        verify(mTabStripSceneMock, times(1))
                .updateNtbWebUi(
                        mNativePtr,
                        ntbWebUiHost,
                        Math.round(touchTarget.left * DP_TO_PX),
                        Math.round(touchTarget.top * DP_TO_PX - topPaddingPx),
                        Math.round(touchTarget.width() * DP_TO_PX),
                        Math.round(touchTarget.height() * DP_TO_PX));
        ntbWebUiHost.release();
    }

    @Test
    public void testUpdateGlicButton() {
        mGlicButton.setKeyboardFocused(true);
        mTabStripSceneLayer.pushButtonsAndBackground(
                mStripLayoutHelperManager,
                mTrailingButtonsCoordinator,
                0,
                0,
                0.0f,
                0.0f,
                0.0f,
                0.0f);
        verify(mTabStripSceneMock, times(1))
                .updateGlicButton(
                        eq(mNativePtr),
                        anyInt(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyBoolean(),
                        anyInt(),
                        anyBoolean(),
                        anyInt(),
                        anyFloat(),
                        eq(true),
                        eq(R.drawable.glic_button_keyfocus),
                        eq(
                                MaterialColors.getColor(
                                        mContext, R.attr.colorPrimary, /* defaultValue= */ 0)),
                        anyFloat(),
                        anyInt(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyInt(),
                        anyFloat(),
                        anyFloat(),
                        anyBoolean(),
                        anyInt(),
                        anyBoolean(),
                        anyInt(),
                        anyInt());
    }

    @Test
    public void testUpdateGlicActorButton() {
        when(mTrailingButtonsCoordinator.getGlicActorButton()).thenReturn(mGlicButton);
        mGlicButton.setKeyboardFocused(true);
        mTabStripSceneLayer.pushButtonsAndBackground(
                mStripLayoutHelperManager,
                mTrailingButtonsCoordinator,
                0,
                0,
                0.0f,
                0.0f,
                0.0f,
                0.0f);
        verify(mTabStripSceneMock, times(1))
                .updateGlicActorButton(
                        eq(mNativePtr),
                        anyInt(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyBoolean(),
                        anyInt(),
                        anyBoolean(),
                        anyInt(),
                        anyFloat(),
                        eq(true),
                        eq(R.drawable.glic_split_button_keyfocus),
                        eq(
                                MaterialColors.getColor(
                                        mContext, R.attr.colorPrimary, /* defaultValue= */ 0)),
                        anyFloat(),
                        anyInt(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat());
    }

    @Test
    public void testUpdateModelSelectorButton() {
        mModelSelectorButton.setKeyboardFocused(true);
        mTabStripSceneLayer.pushButtonsAndBackground(
                mStripLayoutHelperManager,
                mTrailingButtonsCoordinator,
                0,
                0,
                0.0f,
                0.0f,
                0.0f,
                0.0f);
        verify(mTabStripSceneMock, times(1))
                .updateModelSelectorButton(
                        eq(mNativePtr),
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
                                        mContext, R.attr.colorPrimary, /* defaultValue= */ 0)));
    }

    @Test
    public void testPushAndUpdateStrip_PushesLinkDropIndicator() {
        mTabStripSceneLayer.pushAndUpdateStrip(
                mStripLayoutHelperManager,
                mTrailingButtonsCoordinator,
                mLayerTitleCache,
                mResourceManager,
                mStripLayoutTabs,
                null,
                1.f,
                0,
                Color.YELLOW,
                0.f,
                0.f,
                0.f,
                0.f);
        verify(mTabStripSceneMock)
                .updateLinkDropIndicator(
                        eq(mNativePtr),
                        eq(R.drawable.ic_tab_strip_drop_arrow),
                        anyFloat(),
                        /* visible= */ eq(false),
                        anyInt());
    }

    @Test
    public void testPushLinkDropIndicator_Visible() {
        // Use a density other than 1 to verify the dp to px conversion.
        JniRawPtr<TabStripSceneLayer.NativeTabStripSceneLayer> ptr = JniRawPtr.createForTesting(2L);
        try {
            TabStripSceneLayer sceneLayer =
                    new TabStripSceneLayer(/* density= */ 2.f, /* activityCreationUptimeMs= */ 0L);
            when(mTabStripSceneMock.init(sceneLayer)).thenReturn(ptr);
            sceneLayer.initializeNativeForTesting();
            when(mStripLayoutHelper.isLinkDropIndicatorVisible()).thenReturn(true);
            when(mStripLayoutHelper.getLinkDropIndicatorXDp()).thenReturn(50.f);

            sceneLayer.pushLinkDropIndicator(mStripLayoutHelperManager);
            verify(mTabStripSceneMock)
                    .updateLinkDropIndicator(
                            ptr,
                            R.drawable.ic_tab_strip_drop_arrow,
                            /* centerXPx= */ 100.f,
                            /* visible= */ true,
                            /* tint= */ SemanticColorUtils.getColorOutline(mContext));
        } finally {
            ptr.release();
        }
    }

    @Test
    public void testPushLinkDropIndicator_Incognito() {
        when(mStripLayoutHelperManager.isIncognito()).thenReturn(true);
        when(mStripLayoutHelper.isLinkDropIndicatorVisible()).thenReturn(true);

        mTabStripSceneLayer.pushLinkDropIndicator(mStripLayoutHelperManager);
        verify(mTabStripSceneMock)
                .updateLinkDropIndicator(
                        eq(mNativePtr),
                        eq(R.drawable.ic_tab_strip_drop_arrow),
                        anyFloat(),
                        /* visible= */ eq(true),
                        /* tint= */ eq(mContext.getColor(R.color.baseline_neutral_variant_60)));
    }
}
