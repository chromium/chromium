// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.util.Size;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider.MultiThumbnailMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.MultiThumbnailCardProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link StripDragShadowView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class StripDragShadowViewUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Captor private ArgumentCaptor<FaviconImageCallback> mGetFaviconCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Bitmap>> mGetThumbnailCallbackCaptor;

    @Mock private BrowserControlsStateProvider mMockBrowserControlsStateProvider;
    @Mock private MultiThumbnailCardProvider mMockMultiThumbnailCardProvider;
    @Mock private TabModelSelector mMockTabModelSelector;
    @Mock private StripDragShadowView.ShadowUpdateHost mMockShadowUpdateHost;

    @Mock private TabContentManager mMockTabContentManager;
    @Mock private LayerTitleCache mMockLayerTitleCache;
    @Mock private TabGroupModelFilterProvider mMockTabGroupModelFilterProvider;
    @Mock private TabGroupModelFilter mMockTabGroupModelFilter;
    @Mock private Tab mMockTab;
    @Mock private TabFavicon mMockTabFavicon;
    @Mock private Bitmap mMockThumbnailBitmap;
    @Mock private Bitmap mMockOriginalFaviconBitmap;
    @Mock private Bitmap mMockHistoryFaviconBitmap;

    private static final int TAB_ID = 10;

    private Activity mActivity;
    private StripDragShadowView mStripDragShadowView;

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

        when(mMockTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(mMockTabGroupModelFilterProvider);
        when(mMockTabGroupModelFilterProvider.getTabGroupModelFilter(anyBoolean()))
                .thenReturn(mMockTabGroupModelFilter);

        when(mMockTab.getId()).thenReturn(TAB_ID);
        when(mMockTab.getTabGroupId()).thenReturn(Token.createRandom());

        mStripDragShadowView =
                (StripDragShadowView)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.strip_drag_shadow_view, null);
        mStripDragShadowView.initialize(
                mMockBrowserControlsStateProvider,
                mMockMultiThumbnailCardProvider,
                mMockTabContentManager,
                new ObservableSupplierImpl<>(mMockLayerTitleCache),
                mMockTabModelSelector,
                mMockShadowUpdateHost);

        mCardView = mStripDragShadowView.findViewById(R.id.card_view);
        mTitleView = mStripDragShadowView.findViewById(R.id.tab_title);
        mFaviconView = mStripDragShadowView.findViewById(R.id.tab_favicon);
        mThumbnailView = mStripDragShadowView.findViewById(R.id.tab_thumbnail);

        mActivity.setContentView(mStripDragShadowView);
    }

    @Test
    public void testSetTab() {
        mStripDragShadowView.prepareForTabDrag(mMockTab, 0);

        assertEquals(
                "Should have set Tab on drag start.",
                mMockTab,
                mStripDragShadowView.getTabForTesting());
        verify(mMockTab).addObserver(any());
    }

    @Test
    public void testClear() {
        mStripDragShadowView.prepareForTabDrag(mMockTab, 0);

        mStripDragShadowView.clear();

        assertNull(
                "Should no longer be tied to Tab on drag end.",
                mStripDragShadowView.getTabForTesting());
        verify(mMockTab).removeObserver(any());
    }

    @Test
    @Config(qualifiers = "w640dp-h360dp")
    public void testUpdate_LayoutSize_Landscape() {
        mStripDragShadowView.prepareForTabDrag(mMockTab, 0);

        int expectedWidth = StripDragShadowView.WIDTH_DP;
        int expectedHeight =
                TabUtils.deriveGridCardHeight(
                        expectedWidth, mActivity, mMockBrowserControlsStateProvider);
        LayoutParams layoutParams = mStripDragShadowView.getLayoutParams();
        assertEquals("Unexpected view width.", expectedWidth, layoutParams.width);
        assertEquals("Unexpected view height.", expectedHeight, layoutParams.height);
    }

    @Test
    @Config(qualifiers = "w360dp-h640dp")
    public void testUpdate_LayoutSize_Portrait() {
        mStripDragShadowView.prepareForTabDrag(mMockTab, 0);

        int expectedHeight = StripDragShadowView.HEIGHT_DP;
        int expectedWidth =
                TabUtils.deriveGridCardWidth(
                        expectedHeight, mActivity, mMockBrowserControlsStateProvider);
        LayoutParams layoutParams = mStripDragShadowView.getLayoutParams();
        assertEquals("Unexpected view width.", expectedWidth, layoutParams.width);
        assertEquals("Unexpected view height.", expectedHeight, layoutParams.height);
    }

    @Test
    public void testUpdate_LayoutSize_Expand() {
        mStripDragShadowView.prepareForTabDrag(mMockTab, 0);
        assertNull("Should not be animating.", mStripDragShadowView.getRunningAnimatorForTesting());

        mStripDragShadowView.expand();

        Resources resources = mActivity.getResources();
        int expectedWidth = 0;
        int expectedHeight =
                resources.getDimensionPixelSize(R.dimen.tab_grid_card_header_height)
                        + (2 * resources.getDimensionPixelSize(R.dimen.tab_grid_card_margin));
        LayoutParams layoutParams = mStripDragShadowView.getLayoutParams();
        assertEquals("Unexpected view width.", expectedWidth, layoutParams.width);
        assertEquals("Unexpected view height.", expectedHeight, layoutParams.height);
        assertNotNull("Should be animating.", mStripDragShadowView.getRunningAnimatorForTesting());
    }

    @Test
    public void testUpdate_ThumbnailRequestSuccess() {
        mStripDragShadowView.prepareForTabDrag(mMockTab, 0);

        verify(mMockTabContentManager)
                .getTabThumbnailWithCallback(
                        eq(TAB_ID), any(Size.class), mGetThumbnailCallbackCaptor.capture());

        mGetThumbnailCallbackCaptor.getValue().onResult(mMockThumbnailBitmap);
        assertEquals(
                "Should be using returned thumbnail.",
                ((BitmapDrawable) mThumbnailView.getDrawable()).getBitmap(),
                mMockThumbnailBitmap);
    }

    @Test
    public void testUpdate_ThumbnailRequestFailure() {
        mStripDragShadowView.prepareForTabDrag(mMockTab, 0);
        verify(mMockTabContentManager)
                .getTabThumbnailWithCallback(
                        eq(TAB_ID), any(Size.class), mGetThumbnailCallbackCaptor.capture());

        mGetThumbnailCallbackCaptor.getValue().onResult(null);
        assertEquals(
                "Should be using default thumbnail.",
                mThumbnailView.getIconDrawableForTesting(),
                mThumbnailView.getDrawable());
    }

    @Test
    public void testUpdate_GroupThumbnailRequest() {
        mStripDragShadowView.prepareForGroupDrag(mMockTab, 0);

        // Verify we only request the group thumbnail, and not a single tab thumbnail.
        verify(mMockTabContentManager, never()).getTabThumbnailWithCallback(anyInt(), any(), any());
        verify(mMockMultiThumbnailCardProvider)
                .getTabThumbnailWithCallback(
                        any(MultiThumbnailMetadata.class),
                        any(Size.class),
                        eq(false),
                        any(Callback.class));
    }

    @Test
    public void testUpdate_OriginalFavicon() {
        TabFavicon.setInstanceForTesting(mMockTabFavicon);
        when(mMockTabFavicon.getFavicon()).thenReturn(mMockOriginalFaviconBitmap);

        mStripDragShadowView.prepareForTabDrag(mMockTab, 0);
        assertEquals(
                "Should be using original favicon.",
                mMockOriginalFaviconBitmap,
                ((BitmapDrawable) mFaviconView.getDrawable()).getBitmap());
        verify(mMockLayerTitleCache, never()).getDefaultFavicon(any());
        verify(mMockLayerTitleCache, never()).fetchFaviconWithCallback(any(), any());
    }

    @Test
    public void testUpdate_HistoryFavicon() {
        Bitmap defaultFavicon = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        when(mMockLayerTitleCache.getDefaultFavicon(mMockTab)).thenReturn(defaultFavicon);

        // Assume TabFavicon.getBitmap(mMockTab) returns null.
        mStripDragShadowView.prepareForTabDrag(mMockTab, 0);

        // Verify default is used before fetch.
        assertEquals(
                "Should be using default favicon.",
                defaultFavicon,
                ((BitmapDrawable) mFaviconView.getDrawable()).getBitmap());

        // Verify fetch is kicked off.
        verify(mMockLayerTitleCache)
                .fetchFaviconWithCallback(eq(mMockTab), mGetFaviconCallbackCaptor.capture());

        // Fulfill fetch.
        mGetFaviconCallbackCaptor
                .getValue()
                .onFaviconAvailable(mMockHistoryFaviconBitmap, /* iconUrl= */ null);

        assertEquals(
                "Should be using favicon from history.",
                mMockHistoryFaviconBitmap,
                ((BitmapDrawable) mFaviconView.getDrawable()).getBitmap());
    }

    @Test
    public void testUpdate_TabTitle() {
        String expectedTitle = "tab";
        when(mMockLayerTitleCache.getUpdatedTitle(any(), anyString())).thenReturn(expectedTitle);

        mStripDragShadowView.prepareForTabDrag(mMockTab, 0);

        assertEquals("Unexpected tab title.", expectedTitle, mTitleView.getText());
    }

    @Test
    public void testUpdate_GroupTitle() {
        String expectedTitle = "group";
        when(mMockLayerTitleCache.getUpdatedGroupTitle(any(), anyString(), anyBoolean()))
                .thenReturn(expectedTitle);

        mStripDragShadowView.prepareForGroupDrag(mMockTab, 0);

        assertEquals("Unexpected group title.", expectedTitle, mTitleView.getText());
    }

    @Test
    public void testUpdate_TabTinting_Standard() {
        testUpdate_TabTinting(/* incognito= */ false);
    }

    @Test
    public void testUpdate_TabTinting_Incognito() {
        testUpdate_TabTinting(/* incognito= */ true);
    }

    private void testUpdate_TabTinting(boolean incognito) {
        when(mMockTab.isIncognitoBranded()).thenReturn(incognito);
        mStripDragShadowView.prepareForTabDrag(mMockTab, 0);

        // Verify card color
        @ColorRes
        int expectedBackgroundColor = TabUiThemeUtil.getDraggedTabBackgroundColor(mActivity);
        assertEquals(
                "Unexpected card color.",
                expectedBackgroundColor,
                mCardView.getBackgroundTintList().getDefaultColor());

        // Verify text color
        @ColorRes
        int expectedTextColor =
                AppCompatResources.getColorStateList(
                                mActivity,
                                incognito
                                        ? R.color.compositor_tab_title_bar_text_incognito
                                        : R.color.compositor_tab_title_bar_text)
                        .getDefaultColor();
        assertEquals("Unexpected text color.", expectedTextColor, mTitleView.getCurrentTextColor());
    }

    @Test
    public void testUpdate_GroupTinting_Standard() {
        testUpdate_GroupTinting(/* incognito= */ false);
    }

    @Test
    public void testUpdate_GroupTinting_Incognito() {
        testUpdate_GroupTinting(/* incognito= */ true);
    }

    private void testUpdate_GroupTinting(boolean incognito) {
        @TabGroupColorId int colorId = TabGroupColorId.GREY;
        when(mMockTab.isIncognitoBranded()).thenReturn(incognito);
        when(mMockTabGroupModelFilter.getTabGroupColorWithFallback(any(Token.class)))
                .thenReturn(colorId);
        mStripDragShadowView.prepareForGroupDrag(mMockTab, 0);

        // Verify card color
        @ColorInt
        int expectedGroupColor =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                        mActivity, colorId, incognito);
        assertEquals(
                "Unexpected card color.",
                expectedGroupColor,
                mCardView.getBackgroundTintList().getDefaultColor());

        // Verify thumbnail color
        verify(mMockMultiThumbnailCardProvider)
                .setMiniThumbnailPlaceholderColor(
                        TabUiThemeUtil.getMiniThumbnailPlaceholderColorForGroup(
                                mActivity, incognito, expectedGroupColor));

        // Verify text color
        @ColorInt
        int expectedTextColor =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemTextColor(
                        mActivity, colorId, incognito);
        assertEquals("Unexpected text color.", expectedTextColor, mTitleView.getCurrentTextColor());
    }
}
