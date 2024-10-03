// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Matrix;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.Size;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewStub;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;

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

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData.PriceDrop;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.ShoppingPersistedTabDataFetcher;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;

/** Junit Tests for {@link TabGridViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class TabGridViewBinderUnitTest {
    private static final int INIT_WIDTH = 100;
    private static final int INIT_HEIGHT = 200;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGridView mViewGroup;
    @Mock private ThumbnailFetcher mFetcher;
    @Mock private TabThumbnailView mThumbnailView;
    @Mock private FrameLayout mTabGroupColorViewContainer;
    @Mock private ImageView mFaviconView;
    @Mock private ViewStub mTabCardLabelStub;
    @Mock private TabCardLabelView mTabCardLabelView;
    @Mock private TypedArray mTypedArray;
    @Mock private TabFavicon mTabFavicon;
    @Mock private PriceCardView mPriceCardView;
    @Mock private Drawable mDrawable;
    @Mock private TabCardLabelData mTabCardLabelData;
    @Mock private ShoppingPersistedTabDataFetcher mShoppingPersistedTabDataFetcher;
    @Mock private ShoppingPersistedTabData mShoppingPersistedTabData;

    @Captor private ArgumentCaptor<Callback<Drawable>> mCallbackCaptor;

    private Context mContext;
    private PropertyModel mModel;
    private LayoutParams mLayoutParams;
    private BitmapDrawable mBitmapDrawable;
    private PriceDrop mPriceDrop = new PriceDrop("$7", "$89");

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;

        mModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ACTION_STATE, TabActionState.CLOSABLE)
                        .with(TabProperties.THUMBNAIL_FETCHER, mFetcher)
                        .with(TabProperties.IS_INCOGNITO, false)
                        .with(TabProperties.IS_SELECTED, true)
                        .with(TabProperties.GRID_CARD_SIZE, new Size(INIT_WIDTH, INIT_HEIGHT))
                        .build();
        when(mViewGroup.fastFindViewById(R.id.tab_thumbnail)).thenReturn(mThumbnailView);
        when(mViewGroup.fastFindViewById(R.id.tab_group_color_view_container))
                .thenReturn(mTabGroupColorViewContainer);
        when(mViewGroup.fastFindViewById(R.id.tab_favicon)).thenReturn(mFaviconView);
        when(mViewGroup.fastFindViewById(R.id.price_info_box_outer)).thenReturn(mPriceCardView);
        when(mViewGroup.fastFindViewById(R.id.tab_card_label_stub)).thenReturn(mTabCardLabelStub);
        doAnswer(
                        (ignored) -> {
                            when(mViewGroup.fastFindViewById(R.id.tab_card_label_stub))
                                    .thenReturn(null);
                            when(mViewGroup.fastFindViewById(R.id.tab_card_label))
                                    .thenReturn(mTabCardLabelView);
                            return mTabCardLabelView;
                        })
                .when(mTabCardLabelStub)
                .inflate();
        when(mFaviconView.getContext()).thenReturn(mContext);
        when(mViewGroup.getContext()).thenReturn(mContext);
        when(mViewGroup.getResources()).thenReturn(mContext.getResources());

        // mModel, view and bitmap all use the same initial values.
        mLayoutParams = new LayoutParams(INIT_WIDTH, INIT_HEIGHT);
        mBitmapDrawable =
                new BitmapDrawable(Bitmap.createBitmap(INIT_WIDTH, INIT_HEIGHT, Config.RGB_565));
        when(mViewGroup.getLayoutParams()).thenReturn(mLayoutParams);

        LayoutParams thumbnailParams = new LayoutParams(INIT_WIDTH, INIT_HEIGHT);
        when(mThumbnailView.getLayoutParams()).thenReturn(thumbnailParams);

        when(mShoppingPersistedTabData.getPriceDrop()).thenReturn(mPriceDrop);
        doCallback(
                        (Callback<ShoppingPersistedTabData> callback) -> {
                            callback.onResult(mShoppingPersistedTabData);
                        })
                .when(mShoppingPersistedTabDataFetcher)
                .fetch(any());
    }

    @Test
    @org.robolectric.annotation.Config(qualifiers = "sw348dp")
    public void bindClosableTabWithCardWidth_updateNullFetcher() {
        mModel.set(TabProperties.THUMBNAIL_FETCHER, null);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.THUMBNAIL_FETCHER);
        verify(mThumbnailView).updateThumbnailPlaceholder(false, true);
        verify(mThumbnailView).setImageDrawable(null);

        // Update width.
        // updatedBitmapWidth = updatedCardWidth - margins = 200 - 40 = 160.
        // updatedBitmapHeight = INIT_HEIGHT - margins = 200 - 40 - 160.
        final int updatedCardWidth = 200;
        mModel.set(TabProperties.GRID_CARD_SIZE, new Size(updatedCardWidth, INIT_HEIGHT));
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.GRID_CARD_SIZE);
        verify(mViewGroup).setMinimumWidth(updatedCardWidth);
        verify(mThumbnailView, times(2)).updateThumbnailPlaceholder(false, true);
        assertThat(mLayoutParams.width, equalTo(updatedCardWidth));

        verify(mThumbnailView, times(2)).setImageDrawable(null);
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

        verify(mViewGroup).setMinimumWidth(updatedCardWidth);
        verify(mThumbnailView).updateThumbnailPlaceholder(false, true);
        assertThat(mLayoutParams.width, equalTo(updatedCardWidth));

        verify(mFetcher).fetch(any(), eq(true), mCallbackCaptor.capture());
        mCallbackCaptor.getValue().onResult(mBitmapDrawable);

        verify(mThumbnailView).setScaleType(ScaleType.MATRIX);
        verify(mThumbnailView).setImageDrawable(mBitmapDrawable);
        ArgumentCaptor<Matrix> matrixCaptor = ArgumentCaptor.forClass(Matrix.class);
        verify(mThumbnailView).setImageMatrix(matrixCaptor.capture());
        verifyNoMoreInteractions(mThumbnailView);

        // Verify metrics scale + translate.
        // Scale = updatedBitmapWidth / INIT_WIDTH = 176 / 100 = 1.76f.
        float expectedScale = 1.76f;
        // xTranslate = (updatedBitmapWidth - scaledWidth) /2 = (176 - (100*1.76))/2 = 0.
        float expectedXTrans = 0.f;
        assertImageMatrix(matrixCaptor, expectedScale, expectedXTrans);
    }

    @Test
    @org.robolectric.annotation.Config(qualifiers = "sw348dp")
    public void bindClosableTabNoSizeChange_NoOp() {
        // Update width.
        // updatedBitmapWidth = updatedCardWidth - margins = 200 - 40 = 160.
        // updatedBitmapHeight = INIT_HEIGHT - margins = 200 - 40 - 160.
        final int updatedCardWidth = 200;
        when(mViewGroup.getMinimumWidth()).thenReturn(updatedCardWidth);
        when(mViewGroup.getMinimumHeight()).thenReturn(INIT_HEIGHT);
        mLayoutParams.width = updatedCardWidth;
        mLayoutParams.height = INIT_HEIGHT;
        when(mThumbnailView.isPlaceholder()).thenReturn(false);

        mModel.set(TabProperties.GRID_CARD_SIZE, new Size(updatedCardWidth, INIT_HEIGHT));
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.GRID_CARD_SIZE);

        verify(mViewGroup, never()).setMinimumWidth(anyInt());
        verify(mViewGroup, never()).setMinimumHeight(anyInt());
        verify(mViewGroup, never()).setLayoutParams(any());
        verify(mFetcher, never()).fetch(any(), anyBoolean(), any());
    }

    @Test
    @org.robolectric.annotation.Config(qualifiers = "sw348dp")
    public void bindClosableTabNoSizeChange_ThumbnailOnly() {
        // Update width.
        // updatedBitmapWidth = updatedCardWidth - margins = 200 - 40 = 160.
        // updatedBitmapHeight = INIT_HEIGHT - margins = 200 - 40 - 160.
        final int updatedCardWidth = 200;
        when(mViewGroup.getMinimumWidth()).thenReturn(updatedCardWidth);
        when(mViewGroup.getMinimumHeight()).thenReturn(INIT_HEIGHT);
        mLayoutParams.width = updatedCardWidth;
        mLayoutParams.height = INIT_HEIGHT;
        when(mThumbnailView.isPlaceholder()).thenReturn(true);

        mModel.set(TabProperties.GRID_CARD_SIZE, new Size(updatedCardWidth, INIT_HEIGHT));
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.GRID_CARD_SIZE);

        verify(mViewGroup, never()).setMinimumWidth(anyInt());
        verify(mViewGroup, never()).setMinimumHeight(anyInt());
        verify(mViewGroup, never()).setLayoutParams(any());
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

        verify(mViewGroup).setMinimumWidth(updatedCardWidth);
        verify(mThumbnailView).updateThumbnailPlaceholder(false, false);
        assertThat(mLayoutParams.width, equalTo(updatedCardWidth));

        verify(mFetcher).fetch(any(), eq(false), mCallbackCaptor.capture());
        mCallbackCaptor.getValue().onResult(mBitmapDrawable);

        verify(mThumbnailView).setScaleType(ScaleType.MATRIX);
        verify(mThumbnailView).setImageDrawable(mBitmapDrawable);
        ArgumentCaptor<Matrix> matrixCaptor = ArgumentCaptor.forClass(Matrix.class);
        verify(mThumbnailView).setImageMatrix(matrixCaptor.capture());
        verifyNoMoreInteractions(mThumbnailView);

        // Verify metrics scale + translate.
        // Scale = updatedBitmapWidth / INIT_WIDTH = 176 / 100 = 1.76f.
        float expectedScale = 1.76f;
        // xTranslate = (updatedBitmapWidth - scaledWidth) /2 = (176 - (100*1.76))/2 = 0.
        float expectedXTrans = 0.f;
        assertImageMatrix(matrixCaptor, expectedScale, expectedXTrans);
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
        verify(mViewGroup).setMinimumWidth(updatedCardWidth);
        verify(mThumbnailView).updateThumbnailPlaceholder(false, true);
        assertThat(mLayoutParams.width, equalTo(updatedCardWidth));
        verify(mFetcher).fetch(any(), eq(true), mCallbackCaptor.capture());

        // Pass bitmap to callback and verify thumbnail updated with image resize.
        mCallbackCaptor.getValue().onResult(mBitmapDrawable);

        verify(mThumbnailView).setScaleType(ScaleType.MATRIX);
        verify(mThumbnailView).setImageDrawable(mBitmapDrawable);
        ArgumentCaptor<Matrix> matrixCaptor = ArgumentCaptor.forClass(Matrix.class);
        verify(mThumbnailView).setImageMatrix(matrixCaptor.capture());
        verifyNoMoreInteractions(mThumbnailView);

        // Verify metrics scale + translate.
        // Scale = updatedBitmapWidth / INIT_WIDTH = 176 / 100 = 1.76f.
        float expectedScale = 1.76f;
        // xTranslate = (updatedBitmapWidth - scaledWidth) /2 = (176 - (100*1.76))/2 = 0.
        float expectedXTrans = 0.f;
        assertImageMatrix(matrixCaptor, expectedScale, expectedXTrans);
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
        verify(mViewGroup).setMinimumHeight(updatedCardHeight);
        verify(mThumbnailView).updateThumbnailPlaceholder(false, true);
        assertThat(mLayoutParams.height, equalTo(updatedCardHeight));
        verify(mFetcher).fetch(any(), eq(true), mCallbackCaptor.capture());

        // Pass bitmap to callback and verify thumbnail updated with image resize.
        mCallbackCaptor.getValue().onResult(mBitmapDrawable);

        verify(mThumbnailView).setScaleType(ScaleType.MATRIX);
        verify(mThumbnailView).setImageDrawable(mBitmapDrawable);
        ArgumentCaptor<Matrix> matrixCaptor = ArgumentCaptor.forClass(Matrix.class);
        verify(mThumbnailView).setImageMatrix(matrixCaptor.capture());
        verifyNoMoreInteractions(mThumbnailView);

        // Verify metrics scale + translate.
        // Scale = updatedBitmapHeight / INIT_HEIGHT = 340 / 200 = 1.7f.
        float expectedScale = 1.7f;
        // xTranslate = (updatedBitmapWidth - scaledWidth) /2 = (76 - (100*1.7))/2 = -47.
        float expectedXTrans = -47f;
        assertImageMatrix(matrixCaptor, expectedScale, expectedXTrans);
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
        mModel.set(TabProperties.FAVICON_FETCHER, fetcher);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.FAVICON_FETCHER);

        verify(mFaviconView).setImageDrawable(mDrawable);
        verify(mFaviconView).setVisibility(View.VISIBLE);

        mModel.set(TabProperties.FAVICON_FETCHER, null);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.FAVICON_FETCHER);
        verify(mFaviconView).setImageDrawable(null);
        verify(mFaviconView).setVisibility(View.GONE);
    }

    @Test
    public void testBindNullFaviconFetcher() {
        mModel.set(TabProperties.FAVICON_FETCHER, null);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.FAVICON_FETCHER);

        verify(mFaviconView).setImageDrawable(null);
    }

    @Test
    public void testBindTabCardLabelData() {
        mModel.set(TabProperties.TAB_CARD_LABEL_DATA, null);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.TAB_CARD_LABEL_DATA);

        verify(mTabCardLabelStub, never()).inflate();

        mModel.set(TabProperties.TAB_CARD_LABEL_DATA, mTabCardLabelData);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.TAB_CARD_LABEL_DATA);

        verify(mTabCardLabelStub).inflate();
        verify(mTabCardLabelView).setData(mTabCardLabelData);

        mModel.set(TabProperties.TAB_CARD_LABEL_DATA, mTabCardLabelData);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.TAB_CARD_LABEL_DATA);

        verify(mTabCardLabelStub).inflate();
        verify(mTabCardLabelView, times(2)).setData(mTabCardLabelData);
    }

    @Test
    public void testBindTabCardLabelData_IgnoredForSelection() {
        mModel.set(TabProperties.TAB_ACTION_STATE, TabActionState.SELECTABLE);
        mModel.set(TabProperties.TAB_CARD_LABEL_DATA, null);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.TAB_CARD_LABEL_DATA);

        verify(mTabCardLabelStub, never()).inflate();

        mModel.set(TabProperties.TAB_CARD_LABEL_DATA, mTabCardLabelData);
        TabGridViewBinder.bindTab(mModel, mViewGroup, TabProperties.TAB_CARD_LABEL_DATA);

        verify(mTabCardLabelStub, never()).inflate();
        verify(mTabCardLabelView, never()).setData(any());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testPriceDrop_PriceCardView() {
        mModel.set(
                TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER,
                mShoppingPersistedTabDataFetcher);
        TabGridViewBinder.bindTab(
                mModel, mViewGroup, TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER);

        verify(mPriceCardView).setPriceStrings(mPriceDrop.price, mPriceDrop.previousPrice);
        verify(mPriceCardView).setVisibility(View.VISIBLE);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testPriceDrop_TabCardLabelView() {
        mModel.set(
                TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER,
                mShoppingPersistedTabDataFetcher);
        TabGridViewBinder.bindTab(
                mModel, mViewGroup, TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER);

        verify(mTabCardLabelStub).inflate();
        verify(mTabCardLabelView)
                .setData(argThat((data) -> TabCardLabelType.PRICE_DROP == data.labelType));
    }

    @Test
    public void testOnViewRecycled() {
        // Shouldn't crash.
        TabGridViewBinder.onViewRecycled(mModel, null);
        TabGridViewBinder.onViewRecycled(mModel, mThumbnailView);

        verify(mThumbnailView, never()).setImageDrawable(null);
        verify(mFetcher, never()).cancel();

        // Should work!
        TabGridViewBinder.onViewRecycled(mModel, mViewGroup);

        verify(mThumbnailView).setImageDrawable(null);
        verify(mFetcher).cancel();

        verify(mTabGroupColorViewContainer).removeAllViews();
        verify(mTabGroupColorViewContainer).setVisibility(View.GONE);
    }

    private void assertImageMatrix(
            ArgumentCaptor<Matrix> matrixCaptor, float expectedScale, float expectedTrans) {
        float[] matValues = new float[9];
        matrixCaptor.getValue().getValues(matValues);
        float scaleX = matValues[0];
        float scaleY = matValues[4];
        float transX = matValues[2];
        float transY = matValues[5];
        assertThat("Incorrect image matrix scaleX", scaleX, equalTo(expectedScale));
        assertThat("Incorrect image matrix scaleY", scaleY, equalTo(expectedScale));
        assertThat("Incorrect image matrix transY", transY, equalTo(0f));
        assertThat("Incorrect image matrix transX", transX, equalTo(expectedTrans));
    }
}
