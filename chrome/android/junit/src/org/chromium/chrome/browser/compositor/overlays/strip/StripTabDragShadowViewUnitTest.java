// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.util.Size;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabThumbnailView;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link StripTabDragShadowView}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.TAB_LINK_DRAG_DROP_ANDROID)
public class StripTabDragShadowViewUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Captor private ArgumentCaptor<FaviconImageCallback> mGetFaviconCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Bitmap>> mGetThumbnailCallbackCaptor;

    @Mock private Supplier<TabContentManager> mMockTabContentManagerSupplier;
    @Mock private Supplier<LayerTitleCache> mMockLayerTitleCacheSupplier;
    @Mock private StripTabDragShadowView.ShadowUpdateHost mMockShadowUpdateHost;

    @Mock private TabContentManager mMockTabContentManager;
    @Mock private LayerTitleCache mMockLayerTitleCache;
    @Mock private Tab mMockTab;
    @Mock private Bitmap mMockThumbnailBitmap;
    @Mock private Bitmap mMockOriginalFaviconBitmap;
    @Mock private Bitmap mMockHistoryFaviconBitmap;
    @Mock private WebContents mMockWebContents;

    private static final int TAB_ID = 10;

    private Activity mActivity;
    private StripTabDragShadowView mStripTabDragShadowView;

    private View mCardView;
    private TextView mTitleView;
    private ImageView mFaviconView;
    private TabThumbnailView mThumbnailView;

    @Before
    public void setUp() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mActivity = activity;
                            mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
                        });

        when(mMockTabContentManagerSupplier.get()).thenReturn(mMockTabContentManager);
        when(mMockLayerTitleCacheSupplier.get()).thenReturn(mMockLayerTitleCache);

        when(mMockTab.getId()).thenReturn(TAB_ID);

        mStripTabDragShadowView =
                (StripTabDragShadowView)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.strip_tab_drag_shadow_view, null);
        mStripTabDragShadowView.initialize(
                /* browserControlsStateProvider= */ null,
                mMockTabContentManagerSupplier,
                mMockLayerTitleCacheSupplier,
                mMockShadowUpdateHost);

        mCardView = mStripTabDragShadowView.findViewById(R.id.card_view);
        mTitleView = mStripTabDragShadowView.findViewById(R.id.tab_title);
        mFaviconView = mStripTabDragShadowView.findViewById(R.id.tab_favicon);
        mThumbnailView = mStripTabDragShadowView.findViewById(R.id.tab_thumbnail);

        mActivity.setContentView(mStripTabDragShadowView);
    }

    @Test
    public void testSetTab() {
        mStripTabDragShadowView.setTab(mMockTab);

        assertEquals(
                "Should have set Tab on drag start.",
                mMockTab,
                mStripTabDragShadowView.getTabForTesting());
        verify(mMockTab).addObserver(any());
    }

    @Test
    public void testClear() {
        mStripTabDragShadowView.setTab(mMockTab);

        mStripTabDragShadowView.clear();

        assertNull(
                "Should no longer be tied to Tab on drag end.",
                mStripTabDragShadowView.getTabForTesting());
        verify(mMockTab).removeObserver(any());
    }

    @Test
    public void testUpdate_LayoutSize() {
        mStripTabDragShadowView.setTab(mMockTab);

        int expectedWidth = StripTabDragShadowView.WIDTH_DP;
        int expectedHeight =
                TabUtils.deriveGridCardHeight(
                        expectedWidth, mActivity, /* browserControlsStateProvider= */ null);
        LayoutParams layoutParams = mStripTabDragShadowView.getLayoutParams();
        assertEquals("Unexpected view width.", expectedWidth, layoutParams.width);
        assertEquals("Unexpected view height.", expectedHeight, layoutParams.height);
    }

    @Test
    public void testUpdate_ThumbnailRequestSuccess() {
        mStripTabDragShadowView.setTab(mMockTab);

        verify(mMockTabContentManager)
                .getTabThumbnailWithCallback(
                        eq(TAB_ID),
                        any(Size.class),
                        mGetThumbnailCallbackCaptor.capture(),
                        eq(true),
                        eq(true));

        mGetThumbnailCallbackCaptor.getValue().onResult(mMockThumbnailBitmap);
        assertEquals(
                "Should be using returned thumbnail.",
                ((BitmapDrawable) mThumbnailView.getDrawable()).getBitmap(),
                mMockThumbnailBitmap);
    }

    @Test
    public void testUpdate_ThumbnailRequestFailure() {
        mStripTabDragShadowView.setTab(mMockTab);
        verify(mMockTabContentManager)
                .getTabThumbnailWithCallback(
                        eq(TAB_ID),
                        any(Size.class),
                        mGetThumbnailCallbackCaptor.capture(),
                        eq(true),
                        eq(true));

        mGetThumbnailCallbackCaptor.getValue().onResult(null);
        assertEquals(
                "Should be using default thumbnail.",
                mThumbnailView.getIconDrawableForTesting(),
                mThumbnailView.getDrawable());
    }

    @Test
    public void testUpdate_OriginalFavicon() {
        when(mMockTab.getWebContents()).thenReturn(mMockWebContents);
        when(mMockLayerTitleCache.getOriginalFavicon(any(Tab.class)))
                .thenReturn(mMockOriginalFaviconBitmap);

        mStripTabDragShadowView.setTab(mMockTab);

        assertEquals(
                "Should be using original favicon.",
                mMockOriginalFaviconBitmap,
                ((BitmapDrawable) mFaviconView.getDrawable()).getBitmap());
    }

    @Test
    public void testUpdate_HistoryFavicon() {
        when(mMockLayerTitleCache.getOriginalFavicon(any(Tab.class)))
                .thenReturn(mMockOriginalFaviconBitmap);

        mStripTabDragShadowView.setTab(mMockTab);
        verify(mMockLayerTitleCache)
                .fetchFaviconWithCallback(eq(mMockTab), mGetFaviconCallbackCaptor.capture());
        mGetFaviconCallbackCaptor
                .getValue()
                .onFaviconAvailable(mMockHistoryFaviconBitmap, /* iconUrl= */ null);

        assertEquals(
                "Should be using favicon from history.",
                mMockHistoryFaviconBitmap,
                ((BitmapDrawable) mFaviconView.getDrawable()).getBitmap());
    }

    @Test
    public void testSetIncognito() {
        boolean incognito = true;
        when(mMockTab.isIncognito()).thenReturn(incognito);

        mStripTabDragShadowView.setTab(mMockTab);

        @ColorRes
        int expectedBackgroundColor =
                TabUiThemeUtil.getTabStripContainerColor(
                        mActivity,
                        incognito,
                        /* foreground= */ true,
                        /* isReordering= */ false,
                        /* isPlaceholder= */ false,
                        /* isHovered= */ false);
        assertEquals(
                "Unexpected drag shadow color.",
                expectedBackgroundColor,
                mCardView.getBackgroundTintList().getDefaultColor());

        @ColorRes
        int expectedTitleColor =
                AppCompatResources.getColorStateList(
                                mActivity, R.color.compositor_tab_title_bar_text_incognito)
                        .getDefaultColor();
        assertEquals(
                "Unexpected title color", expectedTitleColor, mTitleView.getCurrentTextColor());
    }
}
