// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Matrix;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.util.Size;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewStub;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData.PriceDrop;
import org.chromium.chrome.browser.tab_ui.TabCardThemeUtil;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tab_ui.ThumbnailFetcher;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData.TabActionButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.ShoppingPersistedTabDataFetcher;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabCardHighlightState;
import org.chromium.chrome.browser.tasks.tab_management.labels.TabCardLabelData;
import org.chromium.chrome.browser.tasks.tab_management.labels.TabCardLabelType;
import org.chromium.chrome.browser.tasks.tab_management.labels.TabCardLabelView;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.browser_ui.util.motion.OnPeripheralClickListener;
import org.chromium.components.tabs.TabAlert;
import org.chromium.ui.modelutil.PropertyModel;

/** Junit Tests for {@link TabGridViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class TabGridViewBinderUnitTest {
    private static final int INIT_WIDTH = 100;

    private static final int INIT_HEIGHT = 200;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ThumbnailFetcher mFetcher;
    @Mock private TabFavicon mTabFavicon;
    @Mock private Drawable mDrawable;
    @Mock private ShoppingPersistedTabDataFetcher mShoppingPersistedTabDataFetcher;
    @Mock private ShoppingPersistedTabData mShoppingPersistedTabData;

    @Captor private ArgumentCaptor<Callback<Drawable>> mCallbackCaptor;

    private final TabCardLabelData mTabCardLabelData =
            new TabCardLabelData(
                    TabCardLabelType.ACTIVITY_UPDATE,
                    (context) -> "Label",
                    /* asyncImageFactory= */ null,
                    /* contentDescriptionResolver= */ null);
    private final PriceDrop mPriceDrop = new PriceDrop("$7", "$89");

    private TabGridView mViewGroup;
    private TabThumbnailView mThumbnailView;
    private FrameLayout mTabGroupColorViewContainer;
    private ImageView mFaviconView;
    private ImageView mAlertIndicatorView;
    private ViewStub mTabCardLabelStub;
    private ImageView mActionButton;
    private PriceCardView mPriceCardView;
    private TextView mTabTitleView;
    private View mSpinner;
    private Context mContext;
    private PropertyModel mModel;
    private LayoutParams mLayoutParams;
    private BitmapDrawable mBitmapDrawable;

    @Before
    public void setUp() {
        doReturn(mDrawable).when(mDrawable).mutate();
        mContext =
                new ContextThemeWrapper(
                        RuntimeEnvironment.getApplication(), R.style.Theme_BrowserUI_DayNight);

        mModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ACTION_STATE, TabActionState.CLOSABLE)
                        .with(TabProperties.THUMBNAIL_FETCHER, mFetcher)
                        .with(TabProperties.IS_INCOGNITO, false)
                        .with(TabProperties.IS_SELECTED, true)
                        .with(TabProperties.TAB_GROUP_CARD_COLOR, null)
                        .with(TabProperties.GRID_CARD_SIZE, new Size(INIT_WIDTH, INIT_HEIGHT))
                        .build();
        mViewGroup =
                (TabGridView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.tab_grid_card_item, /* root= */ null);
        mThumbnailView = mViewGroup.findViewById(R.id.tab_thumbnail);
        mTabGroupColorViewContainer = mViewGroup.findViewById(R.id.tab_group_color_view_container);
        mFaviconView = mViewGroup.findViewById(R.id.tab_favicon);
        mAlertIndicatorView = mViewGroup.findViewById(R.id.alert_indicator_icon);
        mPriceCardView = mViewGroup.findViewById(R.id.price_info_box_outer);
        mTabCardLabelStub = mViewGroup.findViewById(R.id.tab_card_label_stub);
        mActionButton = mViewGroup.findViewById(R.id.action_button);
        mSpinner = mViewGroup.findViewById(R.id.fetch_thumbnail_spinner);

        // mModel, view and bitmap all use the same initial values.
        mLayoutParams = new LayoutParams(INIT_WIDTH, INIT_HEIGHT);
        mBitmapDrawable =
                new BitmapDrawable(Bitmap.createBitmap(INIT_WIDTH, INIT_HEIGHT, Config.RGB_565));
        mViewGroup.setLayoutParams(mLayoutParams);
        layoutViewGroup();

        when(mShoppingPersistedTabData.getPriceDrop()).thenReturn(mPriceDrop);
        doCallback(
                        (Callback<ShoppingPersistedTabData> callback) -> {
                            callback.onResult(mShoppingPersistedTabData);
                        })
                .when(mShoppingPersistedTabDataFetcher)
                .fetch(any());

        mTabTitleView = mViewGroup.findViewById(R.id.tab_title);
    }

    @Test
    @org.robolectric.annotation.Config(qualifiers = "sw348dp")
    public void bindClosableTabWithCardWidth_updateNullFetcher() {
        mThumbnailView.setImageDrawable(mBitmapDrawable);
        mModel.set(TabProperties.THUMBNAIL_FETCHER, null);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.THUMBNAIL_FETCHER);
        assertThumbnailPlaceholderColor(/* isSelected= */ true);
        assertTrue(mThumbnailView.isPlaceholder());

        // Update width.
        // updatedBitmapWidth = updatedCardWidth - margins = 200 - 40 = 160.
        // updatedBitmapHeight = INIT_HEIGHT - margins = 200 - 40 - 160.
        final int updatedCardWidth = 200;
        mThumbnailView.setImageDrawable(mBitmapDrawable);
        mModel.set(TabProperties.GRID_CARD_SIZE, new Size(updatedCardWidth, INIT_HEIGHT));
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.GRID_CARD_SIZE);
        assertEquals(updatedCardWidth, mViewGroup.getMinimumWidth());
        assertThumbnailPlaceholderColor(/* isSelected= */ true);
        assertThat(mLayoutParams.width, equalTo(updatedCardWidth));

        assertTrue(mThumbnailView.isPlaceholder());
    }

    @Test
    @org.robolectric.annotation.Config(qualifiers = "sw348dp")
    public void bindClosableTabWithCardWidth_updateCardAndThumbnail() {
        // Update width.
        // updatedBitmapWidth = updatedCardWidth - margins = 200 - 40 = 160.
        // updatedBitmapHeight = INIT_HEIGHT - margins = 200 - 40 - 160.
        final int updatedCardWidth = 200;
        mModel.set(TabProperties.GRID_CARD_SIZE, new Size(updatedCardWidth, INIT_HEIGHT));
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.GRID_CARD_SIZE);

        assertEquals(updatedCardWidth, mViewGroup.getMinimumWidth());
        assertThumbnailPlaceholderColor(/* isSelected= */ true);
        assertThat(mLayoutParams.width, equalTo(updatedCardWidth));

        verify(mFetcher).fetch(any(), eq(true), mCallbackCaptor.capture());
        mThumbnailView.setScaleType(ScaleType.FIT_CENTER);
        mCallbackCaptor.getValue().onResult(mBitmapDrawable);

        assertEquals(ScaleType.MATRIX, mThumbnailView.getScaleType());
        assertEquals(mBitmapDrawable, mThumbnailView.getDrawable());
        // The placeholder background is removed once the thumbnail is loaded.
        assertNull(mThumbnailView.getBackground());

        // Verify metrics scale + translate.
        // Scale = updatedBitmapWidth / INIT_WIDTH = 176 / 100 = 1.76f.
        float expectedScale = 1.76f;
        // xTranslate = (updatedBitmapWidth - scaledWidth) /2 = (176 - (100*1.76))/2 = 0.
        float expectedXTrans = 0.f;
        assertImageMatrix(mThumbnailView.getImageMatrix(), expectedScale, expectedXTrans);
    }

    @Test
    @org.robolectric.annotation.Config(qualifiers = "sw348dp")
    public void bindClosableTabNoSizeChange_NoOp() {
        // Update width.
        // updatedBitmapWidth = updatedCardWidth - margins = 200 - 40 = 160.
        // updatedBitmapHeight = INIT_HEIGHT - margins = 200 - 40 - 160.
        final int updatedCardWidth = 200;
        mViewGroup.setMinimumWidth(updatedCardWidth);
        mViewGroup.setMinimumHeight(INIT_HEIGHT);
        mLayoutParams.width = updatedCardWidth;
        mLayoutParams.height = INIT_HEIGHT;
        mThumbnailView.setImageDrawable(mBitmapDrawable);
        // Applying the action state updates the action button, so do it before laying out.
        mViewGroup.setTabActionState(TabActionState.CLOSABLE);
        layoutViewGroup();

        mModel.set(TabProperties.GRID_CARD_SIZE, new Size(updatedCardWidth, INIT_HEIGHT));
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.GRID_CARD_SIZE);

        // The size is unchanged, so no layout should be requested.
        assertFalse(mViewGroup.isLayoutRequested());
        assertEquals(updatedCardWidth, mViewGroup.getMinimumWidth());
        assertEquals(INIT_HEIGHT, mViewGroup.getMinimumHeight());
        verify(mFetcher, never()).fetch(any(), anyBoolean(), any());
    }

    @Test
    @org.robolectric.annotation.Config(qualifiers = "sw348dp")
    public void bindClosableTabNoSizeChange_ThumbnailOnly() {
        // Update width.
        // updatedBitmapWidth = updatedCardWidth - margins = 200 - 40 = 160.
        // updatedBitmapHeight = INIT_HEIGHT - margins = 200 - 40 - 160.
        final int updatedCardWidth = 200;
        mViewGroup.setMinimumWidth(updatedCardWidth);
        mViewGroup.setMinimumHeight(INIT_HEIGHT);
        mLayoutParams.width = updatedCardWidth;
        mLayoutParams.height = INIT_HEIGHT;
        assertTrue(mThumbnailView.isPlaceholder());
        // Applying the action state updates the action button, so do it before laying out.
        mViewGroup.setTabActionState(TabActionState.CLOSABLE);
        layoutViewGroup();

        mModel.set(TabProperties.GRID_CARD_SIZE, new Size(updatedCardWidth, INIT_HEIGHT));
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.GRID_CARD_SIZE);

        // The size is unchanged, so no layout should be requested.
        assertFalse(mViewGroup.isLayoutRequested());
        assertEquals(updatedCardWidth, mViewGroup.getMinimumWidth());
        assertEquals(INIT_HEIGHT, mViewGroup.getMinimumHeight());
        verify(mFetcher).fetch(any(), anyBoolean(), any());
    }

    @Test
    @org.robolectric.annotation.Config(qualifiers = "sw348dp")
    public void bindClosableTabWithCardWidth_updateCardAndThumbnail_notSelected() {
        // Update width.
        // updatedBitmapWidth = updatedCardWidth - margins = 200 - 40 = 160.
        // updatedBitmapHeight = INIT_HEIGHT - margins = 200 - 40 - 160.
        final int updatedCardWidth = 200;
        mModel.set(TabProperties.GRID_CARD_SIZE, new Size(updatedCardWidth, INIT_HEIGHT));
        mModel.set(TabProperties.IS_SELECTED, false);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.GRID_CARD_SIZE);

        assertEquals(updatedCardWidth, mViewGroup.getMinimumWidth());
        assertThumbnailPlaceholderColor(/* isSelected= */ false);
        assertThat(mLayoutParams.width, equalTo(updatedCardWidth));

        verify(mFetcher).fetch(any(), eq(false), mCallbackCaptor.capture());
        mThumbnailView.setScaleType(ScaleType.FIT_CENTER);
        mCallbackCaptor.getValue().onResult(mBitmapDrawable);

        assertEquals(ScaleType.MATRIX, mThumbnailView.getScaleType());
        assertEquals(mBitmapDrawable, mThumbnailView.getDrawable());
        // The placeholder background is removed once the thumbnail is loaded.
        assertNull(mThumbnailView.getBackground());

        // Verify metrics scale + translate.
        // Scale = updatedBitmapWidth / INIT_WIDTH = 176 / 100 = 1.76f.
        float expectedScale = 1.76f;
        // xTranslate = (updatedBitmapWidth - scaledWidth) /2 = (176 - (100*1.76))/2 = 0.
        float expectedXTrans = 0.f;
        assertImageMatrix(mThumbnailView.getImageMatrix(), expectedScale, expectedXTrans);
    }

    @Test
    @org.robolectric.annotation.Config(qualifiers = "sw600dp")
    public void bindClosableTabWithCardWidthOnTablet_updateCardAndThumbnail() {
        // Update card width.
        // updatedBitmapWidth = updatedCardWidth - margins = 200 - 24 = 176.
        // updatedBitmapHeight = INIT_HEIGHT - margins = 200 - 24 - 176.
        final int updatedCardWidth = 200;
        mModel.set(TabProperties.GRID_CARD_SIZE, new Size(updatedCardWidth, INIT_HEIGHT));

        // Call.
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.GRID_CARD_SIZE);

        // Verify.
        assertEquals(updatedCardWidth, mViewGroup.getMinimumWidth());
        assertThumbnailPlaceholderColor(/* isSelected= */ true);
        assertThat(mLayoutParams.width, equalTo(updatedCardWidth));
        verify(mFetcher).fetch(any(), eq(true), mCallbackCaptor.capture());

        // Pass bitmap to callback and verify thumbnail updated with image resize.
        mThumbnailView.setScaleType(ScaleType.FIT_CENTER);
        mCallbackCaptor.getValue().onResult(mBitmapDrawable);

        assertEquals(ScaleType.MATRIX, mThumbnailView.getScaleType());
        assertEquals(mBitmapDrawable, mThumbnailView.getDrawable());
        // The placeholder background is removed once the thumbnail is loaded.
        assertNull(mThumbnailView.getBackground());

        // Verify metrics scale + translate.
        // Scale = updatedBitmapWidth / INIT_WIDTH = 176 / 100 = 1.76f.
        float expectedScale = 1.76f;
        // xTranslate = (updatedBitmapWidth - scaledWidth) /2 = (176 - (100*1.76))/2 = 0.
        float expectedXTrans = 0.f;
        assertImageMatrix(mThumbnailView.getImageMatrix(), expectedScale, expectedXTrans);
    }

    @Test
    @org.robolectric.annotation.Config(qualifiers = "sw600dp")
    public void bindClosableTabWithCardHeightOnTablet_updateCardAndThumbnail() {
        // Update card height = 400.
        // updatedBitmapWidth = INIT_WIDTH - margins = 100 - 24 = 76.
        // updatedBitmapHeight = updatedCardHeight - margins = 400 - 60 = 340.
        final int updatedCardHeight = 400;
        mModel.set(TabProperties.GRID_CARD_SIZE, new Size(INIT_WIDTH, updatedCardHeight));
        // Call.
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.GRID_CARD_SIZE);

        // Verify.
        assertEquals(updatedCardHeight, mViewGroup.getMinimumHeight());
        assertThumbnailPlaceholderColor(/* isSelected= */ true);
        assertThat(mLayoutParams.height, equalTo(updatedCardHeight));
        verify(mFetcher).fetch(any(), eq(true), mCallbackCaptor.capture());

        // Pass bitmap to callback and verify thumbnail updated with image resize.
        mThumbnailView.setScaleType(ScaleType.FIT_CENTER);
        mCallbackCaptor.getValue().onResult(mBitmapDrawable);

        assertEquals(ScaleType.MATRIX, mThumbnailView.getScaleType());
        assertEquals(mBitmapDrawable, mThumbnailView.getDrawable());
        // The placeholder background is removed once the thumbnail is loaded.
        assertNull(mThumbnailView.getBackground());

        // Verify metrics scale + translate.
        // Scale = updatedBitmapHeight / INIT_HEIGHT = 340 / 200 = 1.7f.
        float expectedScale = 1.7f;
        // xTranslate = (updatedBitmapWidth - scaledWidth) /2 = (76 - (100*1.7))/2 = -47.
        float expectedXTrans = -47f;
        assertImageMatrix(mThumbnailView.getImageMatrix(), expectedScale, expectedXTrans);
    }

    @Test
    public void testBindFaviconFetcher() {
        doReturn(mDrawable).when(mTabFavicon).getSelectedDrawable();

        TabFaviconFetcher fetcher =
                new TabFaviconFetcher() {
                    @Override
                    public void fetch(Callback<TabFavicon> callback) {
                        callback.onResult(mTabFavicon);
                    }
                };
        mFaviconView.setVisibility(View.GONE);
        mModel.set(TabProperties.FAVICON_FETCHER, fetcher);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.FAVICON_FETCHER);

        assertEquals(mDrawable, mFaviconView.getDrawable());
        assertEquals(View.VISIBLE, mFaviconView.getVisibility());

        mModel.set(TabProperties.FAVICON_FETCHER, null);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.FAVICON_FETCHER);
        assertNull(mFaviconView.getDrawable());
        assertEquals(View.GONE, mFaviconView.getVisibility());
    }

    @Test
    public void testBindNullFaviconFetcher() {
        mFaviconView.setImageDrawable(mDrawable);
        mModel.set(TabProperties.FAVICON_FETCHER, null);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.FAVICON_FETCHER);

        assertNull(mFaviconView.getDrawable());
    }

    @Test
    public void testBindTabCardLabelData() {
        mModel.set(TabProperties.TAB_CARD_LABEL_DATA, null);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.TAB_CARD_LABEL_DATA);

        // The stub is only removed from the hierarchy once it is inflated.
        assertNotNull(mTabCardLabelStub.getParent());

        mModel.set(TabProperties.TAB_CARD_LABEL_DATA, mTabCardLabelData);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.TAB_CARD_LABEL_DATA);

        assertNull(mTabCardLabelStub.getParent());
        TabCardLabelView tabCardLabelView = mViewGroup.findViewById(R.id.tab_card_label);
        assertEquals(View.VISIBLE, tabCardLabelView.getVisibility());
        assertEquals("Label", getTabCardLabelText(tabCardLabelView));

        TabCardLabelData updatedLabelData =
                new TabCardLabelData(
                        TabCardLabelType.ACTIVITY_UPDATE,
                        (context) -> "Updated label",
                        /* asyncImageFactory= */ null,
                        /* contentDescriptionResolver= */ null);
        mModel.set(TabProperties.TAB_CARD_LABEL_DATA, updatedLabelData);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.TAB_CARD_LABEL_DATA);

        assertSame(tabCardLabelView, mViewGroup.findViewById(R.id.tab_card_label));
        assertEquals(View.VISIBLE, tabCardLabelView.getVisibility());
        assertEquals("Updated label", getTabCardLabelText(tabCardLabelView));
    }

    @Test
    public void testBindTabCardLabelData_IgnoredForSelection() {
        mModel.set(TabProperties.TAB_ACTION_STATE, TabActionState.SELECTABLE);
        mModel.set(TabProperties.TAB_CARD_LABEL_DATA, null);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.TAB_CARD_LABEL_DATA);

        // The stub is only removed from the hierarchy once it is inflated.
        assertNotNull(mTabCardLabelStub.getParent());

        mModel.set(TabProperties.TAB_CARD_LABEL_DATA, mTabCardLabelData);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.TAB_CARD_LABEL_DATA);

        assertNotNull(mTabCardLabelStub.getParent());
        assertNull(mViewGroup.findViewById(R.id.tab_card_label));
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.DATA_SHARING,
        ChromeFeatureList.DATA_SHARING_JOIN_ONLY
    })
    public void testPriceDrop_PriceCardView() {
        mModel.set(
                TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER,
                mShoppingPersistedTabDataFetcher);
        TabGridViewBinder.bindTab(
                mModel, mViewGroup, TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER);

        TextView priceView = mPriceCardView.findViewById(R.id.current_price);
        TextView previousPriceView = mPriceCardView.findViewById(R.id.previous_price);
        assertEquals("$7", priceView.getText().toString());
        assertEquals("$89", previousPriceView.getText().toString());
        assertEquals(View.VISIBLE, mPriceCardView.getVisibility());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testPriceDrop_TabCardLabelView() {
        mModel.set(
                TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER,
                mShoppingPersistedTabDataFetcher);
        TabGridViewBinder.bindTab(
                mModel, mViewGroup, TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER);

        assertNull(mTabCardLabelStub.getParent());
        TabCardLabelView tabCardLabelView = mViewGroup.findViewById(R.id.tab_card_label);
        assertEquals(View.VISIBLE, tabCardLabelView.getVisibility());
        assertEquals("$7 $89", getTabCardLabelText(tabCardLabelView));
    }

    @Test
    public void testOnViewRecycled() {
        mThumbnailView.setImageDrawable(mBitmapDrawable);
        mTabGroupColorViewContainer.addView(new View(mContext));
        mTabGroupColorViewContainer.setVisibility(View.VISIBLE);
        mSpinner.setVisibility(View.VISIBLE);
        // Use the selected action button background, so that resetting it is observable.
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.IS_SELECTED);

        // Shouldn't crash.
        TabGridViewBinder.onViewRecycled(mModel, null);
        TabGridViewBinder.onViewRecycled(mModel, mThumbnailView);

        assertFalse(mThumbnailView.isPlaceholder());
        verify(mFetcher, never()).cancel();

        // Should work!
        TabGridViewBinder.onViewRecycled(mModel, mViewGroup);

        assertTrue(mThumbnailView.isPlaceholder());
        verify(mFetcher).cancel();

        assertEquals(0, mTabGroupColorViewContainer.getChildCount());
        assertEquals(View.GONE, mTabGroupColorViewContainer.getVisibility());
        assertEquals(View.GONE, mSpinner.getVisibility());
        assertActionButtonBackground(R.drawable.small_icon_background);
    }

    @Test
    public void bindClosableTab_actionButtonTint() {
        mModel.set(TabProperties.IS_SELECTED, true);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.TAB_ACTION_BUTTON_DATA);

        assertEquals(
                TabCardThemeUtil.getActionButtonTintList(
                                mContext,
                                /* isIncognito= */ false,
                                /* isSelected= */ true,
                                /* colorId= */ null)
                        .getDefaultColor(),
                mActionButton.getImageTintList().getDefaultColor());
        assertActionButtonBackground(R.drawable.small_icon_background_on_primary);
    }

    @Test
    public void bindTabWithActionButtonData_setOnClickListenerAndOnPeripheralClickListener() {
        TabActionButtonData tabActionButtonData =
                new TabActionButtonData(
                        TabActionButtonType.CLOSE,
                        new TabActionListener() {
                            @Override
                            public void run(
                                    View view,
                                    int tabId,
                                    @Nullable MotionEventInfo triggeringMotion) {}

                            @Override
                            public void run(
                                    View view,
                                    String syncId,
                                    @Nullable MotionEventInfo triggeringMotion) {}
                        });
        mModel.set(TabProperties.TAB_ACTION_BUTTON_DATA, tabActionButtonData);

        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.TAB_ACTION_BUTTON_DATA);

        assertTrue(mActionButton.hasOnClickListeners());
        assertTrue(
                Shadows.shadowOf(mActionButton).getOnTouchListener()
                        instanceof OnPeripheralClickListener);
    }

    @Test
    public void testBindHighlightState() {
        mModel.set(TabProperties.HIGHLIGHT_STATE, TabCardHighlightState.NOT_HIGHLIGHTED);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.HIGHLIGHT_STATE);
        assertCardHighlighted(false);

        mModel.set(TabProperties.HIGHLIGHT_STATE, TabCardHighlightState.TO_BE_HIGHLIGHTED);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.HIGHLIGHT_STATE);
        assertCardHighlighted(true);

        mModel.set(TabProperties.HIGHLIGHT_STATE, TabCardHighlightState.HIGHLIGHTED);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.HIGHLIGHT_STATE);
        assertCardHighlighted(true);

        mModel.set(TabProperties.HIGHLIGHT_STATE, TabCardHighlightState.NOT_HIGHLIGHTED);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.HIGHLIGHT_STATE);
        assertCardHighlighted(false);
    }

    @Test
    public void testBindHighlightState_updateTransientState() {
        mModel.set(TabProperties.HIGHLIGHT_STATE, TabCardHighlightState.TO_BE_HIGHLIGHTED);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.HIGHLIGHT_STATE);
        assertCardHighlighted(true);
        assertThat(
                mModel.get(TabProperties.HIGHLIGHT_STATE),
                equalTo(TabCardHighlightState.HIGHLIGHTED));
    }

    @Test
    public void testAlertState() {
        mModel.set(TabProperties.ALERT_STATE, TabAlert.MEDIA_RECORDING);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.ALERT_STATE);
        assertAlertIndicatorIcon(R.drawable.radio_button_checked_24dp);

        mModel.set(TabProperties.ALERT_STATE, TabAlert.AUDIO_PLAYING);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.ALERT_STATE);
        assertAlertIndicatorIcon(R.drawable.volume_up_24dp);

        mModel.set(TabProperties.ALERT_STATE, TabAlert.AUDIO_MUTING);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.ALERT_STATE);
        assertAlertIndicatorIcon(R.drawable.volume_off_24dp);

        mModel.set(TabProperties.ALERT_STATE, TabAlert.NONE);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.ALERT_STATE);
        assertEquals(View.GONE, mAlertIndicatorView.getVisibility());
    }

    @Test
    public void testBindContextClickListener() {
        TabActionListener listener = mock();
        mModel.set(TabProperties.TAB_CONTEXT_CLICK_LISTENER, listener);

        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.TAB_CONTEXT_CLICK_LISTENER);

        assertTrue(mViewGroup.isContextClickable());
        assertTrue(mViewGroup.performContextClick());
        verify(listener).run(mViewGroup, /* tabId= */ 0, /* triggeringMotion= */ null);

        mModel.set(TabProperties.TAB_CONTEXT_CLICK_LISTENER, null);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.TAB_CONTEXT_CLICK_LISTENER);
        assertFalse(mViewGroup.isContextClickable());
        assertFalse(mViewGroup.performContextClick());
    }

    private void assertImageMatrix(Matrix matrix, float expectedScale, float expectedTrans) {
        float[] matValues = new float[9];
        matrix.getValues(matValues);
        float scaleX = matValues[0];
        float scaleY = matValues[4];
        float transX = matValues[2];
        float transY = matValues[5];
        assertThat("Incorrect image matrix scaleX", scaleX, equalTo(expectedScale));
        assertThat("Incorrect image matrix scaleY", scaleY, equalTo(expectedScale));
        assertThat("Incorrect image matrix transY", transY, equalTo(0f));
        assertThat("Incorrect image matrix transX", transX, equalTo(expectedTrans));
    }

    /** Measures and lays out the detached card with the size of its LayoutParams. */
    private void layoutViewGroup() {
        mViewGroup.measure(
                MeasureSpec.makeMeasureSpec(mLayoutParams.width, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(mLayoutParams.height, MeasureSpec.EXACTLY));
        mViewGroup.layout(0, 0, mLayoutParams.width, mLayoutParams.height);
    }

    private void assertThumbnailPlaceholderColor(boolean isSelected) {
        GradientDrawable placeholderBackground = (GradientDrawable) mThumbnailView.getBackground();
        assertEquals(
                TabCardThemeUtil.getMiniThumbnailPlaceholderColor(
                        mContext, /* isIncognito= */ false, isSelected, /* colorId= */ null),
                placeholderBackground.getColor().getDefaultColor());
    }

    private void assertActionButtonBackground(@DrawableRes int expectedBackgroundRes) {
        assertEquals(
                expectedBackgroundRes,
                Shadows.shadowOf(mActionButton.getBackground()).getCreatedFromResId());
    }

    private void assertAlertIndicatorIcon(@DrawableRes int expectedIconRes) {
        assertEquals(View.VISIBLE, mAlertIndicatorView.getVisibility());
        assertEquals(
                expectedIconRes,
                Shadows.shadowOf(mAlertIndicatorView.getDrawable()).getCreatedFromResId());
    }

    private void assertCardHighlighted(boolean isHighlighted) {
        // Let the highlight fade animations finish.
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        View cardWrapper = mViewGroup.findViewById(R.id.card_wrapper);
        if (isHighlighted) {
            assertEquals(View.VISIBLE, cardWrapper.getVisibility());
            assertEquals(
                    R.drawable.tab_grid_card_highlight,
                    Shadows.shadowOf(cardWrapper.getBackground()).getCreatedFromResId());
        } else {
            assertEquals(View.GONE, cardWrapper.getVisibility());
            assertNull(cardWrapper.getBackground());
        }
    }

    private static String getTabCardLabelText(TabCardLabelView tabCardLabelView) {
        TextView labelText = tabCardLabelView.findViewById(R.id.tab_label_text);
        return labelText.getText().toString();
    }

    @Test
    public void testTabContentDescription() {
        final String title = "Tab title";
        mModel.set(TabProperties.TITLE, title);

        // Unpinned state.
        mModel.set(TabProperties.IS_PINNED, false);

        // TabAlert NONE.
        mModel.set(TabProperties.ALERT_STATE, TabAlert.NONE);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.ALERT_STATE);
        assertEquals(
                mContext.getString(R.string.accessibility_tabstrip_tab, title),
                mTabTitleView.getContentDescription());

        // TabAlert AUDIO_PLAYING.
        mModel.set(TabProperties.ALERT_STATE, TabAlert.AUDIO_PLAYING);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.ALERT_STATE);
        assertEquals(
                mContext.getString(R.string.accessibility_tabstrip_tab_audible, title),
                mTabTitleView.getContentDescription());

        // TabAlert AUDIO_MUTING.
        mModel.set(TabProperties.ALERT_STATE, TabAlert.AUDIO_MUTING);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.ALERT_STATE);
        assertEquals(
                mContext.getString(R.string.accessibility_tabstrip_tab_muted, title),
                mTabTitleView.getContentDescription());

        // TabAlert MEDIA_RECORDING.
        mModel.set(TabProperties.ALERT_STATE, TabAlert.MEDIA_RECORDING);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.ALERT_STATE);
        assertEquals(
                mContext.getString(R.string.accessibility_tabstrip_tab_recording, title),
                mTabTitleView.getContentDescription());

        // TabAlert TAB_CAPTURING.
        mModel.set(TabProperties.ALERT_STATE, TabAlert.TAB_CAPTURING);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.ALERT_STATE);
        assertEquals(
                mContext.getString(R.string.accessibility_tabstrip_tab_sharing, title),
                mTabTitleView.getContentDescription());

        // TabAlert PIP_PLAYING.
        mModel.set(TabProperties.ALERT_STATE, TabAlert.PIP_PLAYING);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.ALERT_STATE);
        assertEquals(
                mContext.getString(R.string.accessibility_tabstrip_tab_picture_in_picture, title),
                mTabTitleView.getContentDescription());

        // Pinned state.
        mModel.set(TabProperties.IS_PINNED, true);

        // TabAlert NONE.
        mModel.set(TabProperties.ALERT_STATE, TabAlert.NONE);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.ALERT_STATE);
        assertEquals(
                mContext.getString(R.string.accessibility_tabstrip_tab_pinned, title),
                mTabTitleView.getContentDescription());

        // TabAlert AUDIO_PLAYING.
        mModel.set(TabProperties.ALERT_STATE, TabAlert.AUDIO_PLAYING);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.ALERT_STATE);
        assertEquals(
                mContext.getString(R.string.accessibility_tabstrip_tab_pinned_audible, title),
                mTabTitleView.getContentDescription());

        // TabAlert AUDIO_MUTING.
        mModel.set(TabProperties.ALERT_STATE, TabAlert.AUDIO_MUTING);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.ALERT_STATE);
        assertEquals(
                mContext.getString(R.string.accessibility_tabstrip_tab_pinned_muted, title),
                mTabTitleView.getContentDescription());

        // TabAlert MEDIA_RECORDING.
        mModel.set(TabProperties.ALERT_STATE, TabAlert.MEDIA_RECORDING);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.ALERT_STATE);
        assertEquals(
                mContext.getString(R.string.accessibility_tabstrip_tab_pinned_recording, title),
                mTabTitleView.getContentDescription());

        // TabAlert TAB_CAPTURING.
        mModel.set(TabProperties.ALERT_STATE, TabAlert.TAB_CAPTURING);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.ALERT_STATE);
        assertEquals(
                mContext.getString(R.string.accessibility_tabstrip_tab_pinned_sharing, title),
                mTabTitleView.getContentDescription());

        // TabAlert PIP_PLAYING.
        mModel.set(TabProperties.ALERT_STATE, TabAlert.PIP_PLAYING);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.ALERT_STATE);
        assertEquals(
                mContext.getString(
                        R.string.accessibility_tabstrip_tab_pinned_picture_in_picture, title),
                mTabTitleView.getContentDescription());
    }
}
