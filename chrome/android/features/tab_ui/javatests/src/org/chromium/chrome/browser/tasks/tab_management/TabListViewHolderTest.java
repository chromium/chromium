// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_NO;
import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_YES;

import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;

import static org.chromium.base.GarbageCollectionTestUtils.canBeGarbageCollected;

import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.VectorDrawable;
import android.util.Size;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.core.widget.ImageViewCompat;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;

import com.google.protobuf.ByteString;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge.OptimizationGuideCallback;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactory;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactoryJni;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.LevelDBPersistedDataStorage;
import org.chromium.chrome.browser.tab.state.LevelDBPersistedDataStorageJni;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.ResourceTabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.StaticTabFaviconType;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabActionButtonData;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabActionButtonData.TabActionButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.commerce.PriceTracking.BuyableProduct;
import org.chromium.components.commerce.PriceTracking.PriceTrackingData;
import org.chromium.components.commerce.PriceTracking.ProductPrice;
import org.chromium.components.commerce.PriceTracking.ProductPriceUpdate;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.Any;
import org.chromium.components.optimization_guide.proto.HintsProto;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.components.payments.CurrencyFormatterJni;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Tests for the {@link androidx.recyclerview.widget.RecyclerView.ViewHolder} classes for {@link
 * TabListCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=" + ChromeFeatureList.COMMERCE_PRICE_TRACKING + "<Study",
    "force-fieldtrials=Study/Group"
})
@Batch(Batch.UNIT_TESTS)
public class TabListViewHolderTest extends BlankUiTestActivityTestCase {
    @Rule public JniMocker mMocker = new JniMocker();

    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final String EXPECTED_PRICE_STRING = "$287";
    private static final String EXPECTED_PREVIOUS_PRICE_STRING = "$314";

    private static final ProductPriceUpdate PRODUCT_PRICE_UPDATE =
            ProductPriceUpdate.newBuilder()
                    .setOldPrice(createProductPrice(10_000_000L, "USD"))
                    .setNewPrice(createProductPrice(5_000_000L, "USD"))
                    .build();
    private static final BuyableProduct BUYABLE_PRODUCT =
            BuyableProduct.newBuilder()
                    .setCurrentPrice(createProductPrice(5_000_000L, "USD"))
                    .build();
    private static final PriceTrackingData PRICE_TRACKING_DATA =
            PriceTrackingData.newBuilder()
                    .setBuyableProduct(BUYABLE_PRODUCT)
                    .setProductUpdate(PRODUCT_PRICE_UPDATE)
                    .build();
    private static final Any ANY_PRICE_TRACKING_DATA =
            Any.newBuilder()
                    .setValue(ByteString.copyFrom(PRICE_TRACKING_DATA.toByteArray()))
                    .build();

    private static ProductPrice createProductPrice(long amountMicros, String currencyCode) {
        return ProductPrice.newBuilder()
                .setCurrencyCode(currencyCode)
                .setAmountMicros(amountMicros)
                .build();
    }

    private static final String USD_CURRENCY_SYMBOL = "$";
    private static final String EXPECTED_PRICE = "$5.00";
    private static final String EXPECTED_PREVIOUS_PRICE = "$10";
    private static final String EXPECTED_CONTENT_DESCRIPTION =
            "The price of this item recently dropped from $10 to $5.00";
    private static final GURL TEST_GURL = new GURL("https://www.google.com");

    private ViewGroup mTabGridView;
    private PropertyModel mGridModel;
    private PropertyModelChangeProcessor mGridMcp;

    private ViewGroup mTabStripView;
    private PropertyModel mStripModel;
    private PropertyModelChangeProcessor mStripMcp;

    private ViewGroup mSelectableTabGridView;
    private PropertyModel mSelectableModel;
    private PropertyModelChangeProcessor mSelectableMcp;

    private ViewGroup mTabListView;
    private ViewGroup mSelectableTabListView;
    private PropertyModelChangeProcessor mListMcp;

    @Mock private Profile mProfile;

    @Mock private LevelDBPersistedDataStorage.Natives mLevelDBPersistedTabDataStorage;

    @Mock private UrlUtilities.Natives mUrlUtilitiesJniMock;

    @Mock private CurrencyFormatter.Natives mCurrencyFormatterJniMock;

    @Mock private OptimizationGuideBridgeFactory.Natives mOptimizationGuideBridgeFactoryJniMock;
    @Mock private OptimizationGuideBridge mOptimizationGuideBridge;

    private ThumbnailFetcher mMockThumbnailFetcher =
            new ThumbnailFetcher(
                    new ThumbnailProvider() {
                        @Override
                        public void getTabThumbnailWithCallback(
                                int tabId,
                                Size thumbnailSize,
                                boolean isSelected,
                                Callback<Drawable> callback) {
                            mThumbnailFetchedCount.incrementAndGet();
                            if (!mShouldReturnBitmap) {
                                callback.onResult(null);
                                return;
                            }
                            Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
                            callback.onResult(new BitmapDrawable(bitmap));
                        }
                    },
                    Tab.INVALID_TAB_ID);
    private AtomicInteger mThumbnailFetchedCount = new AtomicInteger();

    private TabListMediator.TabActionListener mMockCloseListener =
            new TabListMediator.TabActionListener() {
                @Override
                public void run(View view, int tabId) {
                    mCloseClicked.set(true);
                    mCloseTabId.set(tabId);
                }
            };
    private AtomicBoolean mCloseClicked = new AtomicBoolean();
    private AtomicInteger mCloseTabId = new AtomicInteger();

    private TabListMediator.TabActionListener mMockSelectedListener =
            new TabListMediator.TabActionListener() {
                @Override
                public void run(View view, int tabId) {
                    mSelectClicked.set(true);
                    mSelectTabId.set(tabId);
                }
            };
    private AtomicBoolean mSelectClicked = new AtomicBoolean();
    private AtomicInteger mSelectTabId = new AtomicInteger();

    private TabListMediator.TabActionListener mMockCreateGroupButtonListener =
            new TabListMediator.TabActionListener() {
                @Override
                public void run(View view, int tabId) {
                    mCreateGroupButtonClicked.set(true);
                    mCreateGroupTabId.set(tabId);
                }
            };
    private AtomicBoolean mCreateGroupButtonClicked = new AtomicBoolean();
    private AtomicInteger mCreateGroupTabId = new AtomicInteger();
    private boolean mShouldReturnBitmap;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        getActivity().setTheme(R.style.Theme_BrowserUI_DayNight);
        // Note: MockitoRule does not work here due to timing issues with
        // BlankUiTestActivityTestCase.
        MockitoAnnotations.initMocks(this);

        ViewGroup view = new LinearLayout(getActivity());
        FrameLayout.LayoutParams params =
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().setContentView(view, params);

                    mTabGridView =
                            (ViewGroup)
                                    getActivity()
                                            .getLayoutInflater()
                                            .inflate(R.layout.tab_grid_card_item, null);
                    mTabStripView =
                            (ViewGroup)
                                    getActivity()
                                            .getLayoutInflater()
                                            .inflate(R.layout.tab_strip_item, null);
                    mSelectableTabGridView =
                            (ViewGroup)
                                    getActivity()
                                            .getLayoutInflater()
                                            .inflate(R.layout.tab_grid_card_item, null);
                    mSelectableTabListView =
                            (ViewGroup)
                                    getActivity()
                                            .getLayoutInflater()
                                            .inflate(R.layout.tab_list_card_item, null);
                    mTabListView =
                            (ViewGroup)
                                    getActivity()
                                            .getLayoutInflater()
                                            .inflate(R.layout.tab_list_card_item, null);

                    view.addView(mTabGridView);
                    view.addView(mTabStripView);
                    view.addView(mSelectableTabGridView);
                    view.addView(mSelectableTabListView);
                    view.addView(mTabListView);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mGridModel =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                                    .with(TabProperties.TAB_ACTION_STATE, TabActionState.CLOSABLE)
                                    .with(TabProperties.IS_INCOGNITO, false)
                                    .with(TabProperties.TAB_ID, TAB1_ID)
                                    .with(TabProperties.IS_SELECTED, false)
                                    .with(TabProperties.TAB_CLICK_LISTENER, mMockSelectedListener)
                                    .with(
                                            TabProperties.TAB_ACTION_BUTTON_DATA,
                                            new TabActionButtonData(
                                                    TabActionButtonType.CLOSE, mMockCloseListener))
                                    .build();
                    mStripModel =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_STRIP)
                                    .with(TabProperties.TAB_CLICK_LISTENER, mMockSelectedListener)
                                    .with(
                                            TabProperties.TAB_ACTION_BUTTON_DATA,
                                            new TabActionButtonData(
                                                    TabActionButtonType.CLOSE, mMockCloseListener))
                                    .build();
                    mSelectableModel =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                                    .with(TabProperties.TAB_ACTION_STATE, TabActionState.SELECTABLE)
                                    .with(TabProperties.IS_SELECTED, false)
                                    .with(
                                            TabProperties.TAB_ACTION_BUTTON_DATA,
                                            new TabActionButtonData(
                                                    TabActionButtonType.SELECT,
                                                    mMockSelectedListener))
                                    .with(TabProperties.TAB_CLICK_LISTENER, mMockSelectedListener)
                                    .with(
                                            TabProperties.TAB_LONG_CLICK_LISTENER,
                                            mMockSelectedListener)
                                    .with(
                                            TabProperties.TAB_SELECTION_DELEGATE,
                                            new SelectionDelegate<>())
                                    .build();

                    mGridMcp =
                            PropertyModelChangeProcessor.create(
                                    mGridModel, mTabGridView, TabGridViewBinder::bindTab);
                    mStripMcp =
                            PropertyModelChangeProcessor.create(
                                    mStripModel, mTabStripView, TabStripViewBinder::bind);
                    mSelectableMcp =
                            PropertyModelChangeProcessor.create(
                                    mSelectableModel,
                                    mSelectableTabGridView,
                                    TabGridViewBinder::bindTab);
                    PropertyModelChangeProcessor.create(
                            mSelectableModel, mSelectableTabListView, TabListViewBinder::bindTab);
                    mListMcp =
                            PropertyModelChangeProcessor.create(
                                    mGridModel, mTabListView, TabListViewBinder::bindTab);
                });
        mMocker.mock(LevelDBPersistedDataStorageJni.TEST_HOOKS, mLevelDBPersistedTabDataStorage);
        doNothing()
                .when(mLevelDBPersistedTabDataStorage)
                .init(any(LevelDBPersistedDataStorage.class), any(BrowserContextHandle.class));
        doReturn(false).when(mProfile).isOffTheRecord();
        LevelDBPersistedDataStorage.setSkipNativeAssertionsForTesting(true);

        ProfileManager.setLastUsedProfileForTesting(mProfile);
        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(false);

        mMocker.mock(UrlUtilitiesJni.TEST_HOOKS, mUrlUtilitiesJniMock);
        mMocker.mock(CurrencyFormatterJni.TEST_HOOKS, mCurrencyFormatterJniMock);
        doReturn(1L)
                .when(mCurrencyFormatterJniMock)
                .initCurrencyFormatterAndroid(
                        any(CurrencyFormatter.class), anyString(), anyString());
        doNothing().when(mCurrencyFormatterJniMock).setMaxFractionalDigits(anyLong(), anyInt());
        mMocker.mock(
                OptimizationGuideBridgeFactoryJni.TEST_HOOKS,
                mOptimizationGuideBridgeFactoryJniMock);
        doReturn(mOptimizationGuideBridge)
                .when(mOptimizationGuideBridgeFactoryJniMock)
                .getForProfile(mProfile);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
    }

    private void testGridSelected(ViewGroup holder, PropertyModel model) {
        model.set(TabProperties.IS_SELECTED, true);
        Assert.assertTrue(TabUiTestHelper.isTabViewSelected(holder));
        model.set(TabProperties.IS_SELECTED, false);
        Assert.assertFalse(TabUiTestHelper.isTabViewSelected(holder));
    }

    private void tabListSelected(ViewGroup holder, PropertyModel model) {
        model.set(TabProperties.IS_SELECTED, true);
        Assert.assertNotNull(holder.getForeground());
        model.set(TabProperties.IS_SELECTED, false);
        Assert.assertNull(holder.getForeground());
    }

    @Test
    @MediumTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testSelected() {
        testGridSelected(mTabGridView, mGridModel);

        mStripModel.set(TabProperties.IS_SELECTED, true);
        Assert.assertNotNull(((FrameLayout) mTabStripView).getForeground());
        mStripModel.set(TabProperties.IS_SELECTED, false);
        Assert.assertNull(((FrameLayout) mTabStripView).getForeground());

        testGridSelected(mSelectableTabGridView, mSelectableModel);
        mSelectableModel.set(TabProperties.IS_SELECTED, true);
        ImageView actionButton = mSelectableTabGridView.findViewById(R.id.action_button);
        Assert.assertEquals(1, actionButton.getBackground().getLevel());
        Assert.assertNotNull(actionButton.getDrawable());
        Assert.assertEquals(255, actionButton.getDrawable().getAlpha());

        mSelectableModel.set(TabProperties.IS_SELECTED, false);
        Assert.assertEquals(0, actionButton.getBackground().getLevel());
        Assert.assertEquals(0, actionButton.getDrawable().getAlpha());

        tabListSelected(mSelectableTabListView, mSelectableModel);
        mSelectableModel.set(TabProperties.IS_SELECTED, true);
        ImageView endButton = mSelectableTabListView.findViewById(R.id.end_button);
        Assert.assertEquals(1, endButton.getBackground().getLevel());
        Assert.assertNotNull(endButton.getDrawable());
        Assert.assertEquals(255, actionButton.getDrawable().getAlpha());

        mSelectableModel.set(TabProperties.IS_SELECTED, false);
        Assert.assertEquals(0, endButton.getBackground().getLevel());
        Assert.assertEquals(0, endButton.getDrawable().getAlpha());
    }

    @Test
    @MediumTest
    public void testAnimationRestored() {
        View backgroundView = mTabGridView.findViewById(R.id.background_view);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mGridModel.set(TabProperties.IS_SELECTED, true);
                    mGridModel.set(
                            TabProperties.CARD_ANIMATION_STATUS,
                            TabGridView.AnimationStatus.CARD_RESTORE);
                });
        CriteriaHelper.pollUiThread(() -> !((TabGridView) mTabGridView).getIsAnimatingForTesting());

        Assert.assertEquals(View.GONE, backgroundView.getVisibility());
        Assert.assertTrue(TabUiTestHelper.isTabViewSelected(mTabGridView));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mGridModel.set(TabProperties.IS_SELECTED, false);
                    mGridModel.set(
                            TabProperties.CARD_ANIMATION_STATUS,
                            TabGridView.AnimationStatus.CARD_RESTORE);
                });
        CriteriaHelper.pollUiThread(() -> !((TabGridView) mTabGridView).getIsAnimatingForTesting());
        Assert.assertEquals(View.GONE, backgroundView.getVisibility());
        Assert.assertFalse(TabUiTestHelper.isTabViewSelected(mTabGridView));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testTitle() {
        final String title = "Surf the cool webz";
        mGridModel.set(TabProperties.TITLE, title);
        TextView textView = mTabGridView.findViewById(R.id.tab_title);
        Assert.assertEquals(textView.getText(), title);

        mSelectableModel.set(TabProperties.TITLE, title);
        textView = mSelectableTabGridView.findViewById(R.id.tab_title);
        Assert.assertEquals(textView.getText(), title);

        textView = mSelectableTabListView.findViewById(R.id.title);
        Assert.assertEquals(textView.getText(), title);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testThumbnail() {
        mGridModel.set(TabProperties.GRID_CARD_SIZE, new Size(100, 500));
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailFetcher);
        TabThumbnailView thumbnail = mTabGridView.findViewById(R.id.tab_thumbnail);
        assertNotNull("Thumbnail should have a background drawable.", thumbnail.getBackground());
        assertTrue("Thumbnail should be set to a place holder.", thumbnail.isPlaceholder());
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, null);
        assertThat(thumbnail.getDrawable(), instanceOf(VectorDrawable.class));
        assertTrue(
                "Thumbnail placeholder should be used when the thumbnail fetcher is null.",
                thumbnail.isPlaceholder());

        mShouldReturnBitmap = true;
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailFetcher);
        assertThat(
                "Thumbnail should be set.",
                thumbnail.getDrawable(),
                instanceOf(BitmapDrawable.class));
        assertNull("Thumbnail should not have a background drawable.", thumbnail.getBackground());
        assertFalse("Thumbnail should not be set to a place holder.", thumbnail.isPlaceholder());
        Assert.assertEquals(2, mThumbnailFetchedCount.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testThumbnailGridCardSize() {
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailFetcher);
        TabThumbnailView thumbnail = mTabGridView.findViewById(R.id.tab_thumbnail);
        assertNotNull("Thumbnail should have a background drawable.", thumbnail.getBackground());
        assertTrue("Thumbnail should be set to a place holder.", thumbnail.isPlaceholder());

        mShouldReturnBitmap = true;
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailFetcher);
        assertNotNull("Thumbnail should have a background drawable.", thumbnail.getBackground());
        assertTrue("Thumbnail should be set to a place holder.", thumbnail.isPlaceholder());
        mGridModel.set(TabProperties.GRID_CARD_SIZE, new Size(100, 500));
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailFetcher);
        assertThat(
                "Thumbnail should be set.",
                thumbnail.getDrawable(),
                instanceOf(BitmapDrawable.class));
        assertNull("Thumbnail should not have a background drawable.", thumbnail.getBackground());
        assertFalse("Thumbnail should not be set to a place holder.", thumbnail.isPlaceholder());
        Assert.assertEquals(2, mThumbnailFetchedCount.get());
    }

    @SuppressWarnings("UnusedAssignment") // Intentionally set to null for garbage collection.
    @Test
    @MediumTest
    @UiThreadTest
    public void testThumbnailGcAfterNullBitmap() {
        ImageView thumbnail = mTabGridView.findViewById(R.id.tab_thumbnail);
        mShouldReturnBitmap = true;
        mGridModel.set(TabProperties.GRID_CARD_SIZE, new Size(100, 500));
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailFetcher);
        assertThat(thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
        Bitmap bitmap = ((BitmapDrawable) thumbnail.getDrawable()).getBitmap();
        WeakReference<Bitmap> ref = new WeakReference<>(bitmap);
        bitmap = null;

        Assert.assertFalse(canBeGarbageCollected(ref));

        mShouldReturnBitmap = false;
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailFetcher);
        Assert.assertTrue(canBeGarbageCollected(ref));
        Assert.assertEquals(2, mThumbnailFetchedCount.get());
    }

    @SuppressWarnings("UnusedAssignment") // Intentionally set to null for garbage collection.
    @Test
    @MediumTest
    @UiThreadTest
    public void testThumbnailGcAfterNewBitmap() {
        ImageView thumbnail = mTabGridView.findViewById(R.id.tab_thumbnail);
        mShouldReturnBitmap = true;
        mGridModel.set(TabProperties.GRID_CARD_SIZE, new Size(100, 500));
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailFetcher);
        assertThat(thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
        Bitmap bitmap = ((BitmapDrawable) thumbnail.getDrawable()).getBitmap();
        WeakReference<Bitmap> ref = new WeakReference<>(bitmap);
        bitmap = null;

        Assert.assertFalse(canBeGarbageCollected(ref));

        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailFetcher);
        Assert.assertTrue(canBeGarbageCollected(ref));
        Assert.assertEquals(2, mThumbnailFetchedCount.get());
    }

    @SuppressWarnings("UnusedAssignment") // Intentionally set to null for garbage collection.
    @Test
    @MediumTest
    @UiThreadTest
    public void testResetThumbnailGc() {
        ImageView thumbnail = mTabGridView.findViewById(R.id.tab_thumbnail);
        mShouldReturnBitmap = true;
        mGridModel.set(TabProperties.GRID_CARD_SIZE, new Size(100, 500));
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailFetcher);
        assertThat(thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
        Bitmap bitmap = ((BitmapDrawable) thumbnail.getDrawable()).getBitmap();
        WeakReference<Bitmap> ref = new WeakReference<>(bitmap);
        bitmap = null;

        Assert.assertFalse(canBeGarbageCollected(ref));

        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, null);
        Assert.assertTrue(canBeGarbageCollected(ref));
    }

    @SuppressWarnings("UnusedAssignment") // Intentionally set to null for garbage collection.
    @Test
    @MediumTest
    @UiThreadTest
    public void testHiddenGc() {
        TabThumbnailView thumbnail = mTabGridView.findViewById(R.id.tab_thumbnail);
        mShouldReturnBitmap = true;
        mGridModel.set(TabProperties.GRID_CARD_SIZE, new Size(100, 500));
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailFetcher);
        assertThat(thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
        Bitmap bitmap = ((BitmapDrawable) thumbnail.getDrawable()).getBitmap();
        WeakReference<Bitmap> ref = new WeakReference<>(bitmap);
        bitmap = null;

        Assert.assertFalse(canBeGarbageCollected(ref));

        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, null);
        Assert.assertTrue(canBeGarbageCollected(ref));
        assertThat(thumbnail.getDrawable(), instanceOf(VectorDrawable.class));
        assertTrue(
                "Thumbnail placeholder should be used when the thumbnail fetcher is null.",
                thumbnail.isPlaceholder());
        Assert.assertEquals(1, mThumbnailFetchedCount.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testHiddenThenShow() {
        TabThumbnailView thumbnail = mTabGridView.findViewById(R.id.tab_thumbnail);
        mShouldReturnBitmap = true;
        mGridModel.set(TabProperties.GRID_CARD_SIZE, new Size(100, 500));
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailFetcher);
        assertThat(thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
        Assert.assertEquals(1, mThumbnailFetchedCount.get());

        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, null);
        assertThat(thumbnail.getDrawable(), instanceOf(VectorDrawable.class));
        assertTrue(
                "Thumbnail placeholder should be used when the thumbnail fetcher is null.",
                thumbnail.isPlaceholder());
        Assert.assertEquals(1, mThumbnailFetchedCount.get());

        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailFetcher);
        assertThat(thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
        Assert.assertEquals(2, mThumbnailFetchedCount.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testCloseButtonDescription() {
        ImageView listActionButton = mTabListView.findViewById(R.id.end_button);
        ImageView gridActionButton = mTabGridView.findViewById(R.id.action_button);

        Assert.assertNull(listActionButton.getContentDescription());
        Assert.assertNull(gridActionButton.getContentDescription());

        String closeTabDescription = "Close tab";
        mGridModel.set(TabProperties.ACTION_BUTTON_DESCRIPTION_STRING, closeTabDescription);

        Assert.assertEquals(closeTabDescription, listActionButton.getContentDescription());
        Assert.assertEquals(closeTabDescription, gridActionButton.getContentDescription());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testCloseButtonColor() {
        ImageView listActionButton = mTabListView.findViewById(R.id.end_button);
        ImageView gridActionButton = mTabGridView.findViewById(R.id.action_button);

        // This does not test all permutations as IS_INCOGNITO is a readable property key.
        boolean isIncognito = mGridModel.get(TabProperties.IS_INCOGNITO);

        boolean isSelected = false;
        mGridModel.set(TabProperties.IS_SELECTED, isSelected);
        ColorStateList unselectedColorStateList =
                TabUiThemeProvider.getActionButtonTintList(getActivity(), isIncognito, isSelected);

        Assert.assertEquals(
                unselectedColorStateList, ImageViewCompat.getImageTintList(gridActionButton));
        Assert.assertEquals(
                unselectedColorStateList, ImageViewCompat.getImageTintList(listActionButton));

        isSelected = true;
        mGridModel.set(TabProperties.IS_SELECTED, isSelected);
        ColorStateList selectedColorStateList =
                TabUiThemeProvider.getActionButtonTintList(getActivity(), isIncognito, isSelected);
        // The listActionButton does not highlight so use unselected always.
        Assert.assertEquals(
                selectedColorStateList, ImageViewCompat.getImageTintList(gridActionButton));
        Assert.assertEquals(
                unselectedColorStateList, ImageViewCompat.getImageTintList(listActionButton));
    }

    @Test
    @MediumTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testClickToClose() {
        ImageView gridActionButton = mTabGridView.findViewById(R.id.action_button);
        ImageView listActionButton = mTabListView.findViewById(R.id.end_button);
        ImageButton button = mTabStripView.findViewById(R.id.tab_strip_item_button);

        Assert.assertFalse(mCloseClicked.get());
        gridActionButton.performClick();
        Assert.assertTrue(mCloseClicked.get());
        int firstCloseId = mCloseTabId.get();
        Assert.assertEquals(TAB1_ID, firstCloseId);

        mCloseClicked.set(false);
        mCloseTabId.set(Tab.INVALID_TAB_ID);

        Assert.assertFalse(mCloseClicked.get());
        listActionButton.performClick();
        Assert.assertTrue(mCloseClicked.get());
        firstCloseId = mCloseTabId.get();
        Assert.assertEquals(TAB1_ID, firstCloseId);

        mCloseClicked.set(false);

        mGridModel.set(TabProperties.TAB_ID, TAB2_ID);
        gridActionButton.performClick();
        Assert.assertTrue(mCloseClicked.get());
        mCloseClicked.set(false);
        int secondClosed = mCloseTabId.get();

        mCloseClicked.set(false);
        mCloseTabId.set(Tab.INVALID_TAB_ID);

        Assert.assertFalse(mCloseClicked.get());
        listActionButton.performClick();
        Assert.assertTrue(mCloseClicked.get());
        secondClosed = mCloseTabId.get();
        // When TAB_ID in PropertyModel is updated, binder should close tab with updated tab ID.
        Assert.assertEquals(TAB2_ID, secondClosed);
        Assert.assertNotEquals(firstCloseId, secondClosed);

        mCloseClicked.set(false);

        mGridModel.set(TabProperties.TAB_ACTION_BUTTON_DATA, null);
        gridActionButton.performClick();
        Assert.assertFalse(mCloseClicked.get());
        listActionButton.performClick();
        Assert.assertFalse(mCloseClicked.get());

        mStripModel.set(TabProperties.IS_SELECTED, true);
        button.performClick();
        Assert.assertTrue(mCloseClicked.get());
        mCloseClicked.set(false);

        mStripModel.set(TabProperties.IS_SELECTED, false);
        button.performClick();
        Assert.assertFalse(mCloseClicked.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testPriceStringPriceDrop_TabCardLabel() {
        Tab tab = MockTab.createAndInitialize(1, mProfile);
        MockShoppingPersistedTabDataFetcher fetcher = new MockShoppingPersistedTabDataFetcher(tab);
        fetcher.setPriceStrings(EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
        testPriceString(
                tab, fetcher, View.VISIBLE, EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testPriceStringPriceDrop() {
        Tab tab = MockTab.createAndInitialize(1, mProfile);
        MockShoppingPersistedTabDataFetcher fetcher = new MockShoppingPersistedTabDataFetcher(tab);
        fetcher.setPriceStrings(EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
        testPriceString(
                tab, fetcher, View.VISIBLE, EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testPriceStringNullPriceDrop_TabCardLabel() {
        Tab tab = MockTab.createAndInitialize(1, mProfile);
        MockShoppingPersistedTabDataFetcher fetcher = new MockShoppingPersistedTabDataFetcher(tab);
        fetcher.setNullPriceDrop();
        testPriceString(
                tab, fetcher, View.GONE, EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testPriceStringNullPriceDrop() {
        Tab tab = MockTab.createAndInitialize(1, mProfile);
        MockShoppingPersistedTabDataFetcher fetcher = new MockShoppingPersistedTabDataFetcher(tab);
        fetcher.setNullPriceDrop();
        testPriceString(
                tab, fetcher, View.GONE, EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testPriceStringPriceDropThenNull_TabCardLabel() {
        Tab tab = MockTab.createAndInitialize(1, mProfile);
        MockShoppingPersistedTabDataFetcher fetcher = new MockShoppingPersistedTabDataFetcher(tab);
        fetcher.setPriceStrings(EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
        testPriceString(
                tab, fetcher, View.VISIBLE, EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
        fetcher.setNullPriceDrop();
        testPriceString(
                tab, fetcher, View.GONE, EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testPriceStringPriceDropThenNull() {
        Tab tab = MockTab.createAndInitialize(1, mProfile);
        MockShoppingPersistedTabDataFetcher fetcher = new MockShoppingPersistedTabDataFetcher(tab);
        fetcher.setPriceStrings(EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
        testPriceString(
                tab, fetcher, View.VISIBLE, EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
        fetcher.setNullPriceDrop();
        testPriceString(
                tab, fetcher, View.GONE, EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testPriceStringTurnFeatureOff_TabCardLabel() {
        Tab tab = MockTab.createAndInitialize(1, mProfile);
        MockShoppingPersistedTabDataFetcher fetcher = new MockShoppingPersistedTabDataFetcher(tab);
        fetcher.setPriceStrings(EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
        testPriceString(
                tab, fetcher, View.VISIBLE, EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
        mGridModel.set(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER, null);
        TabCardLabelView tabCardLabelView = mTabGridView.findViewById(R.id.tab_card_label);
        Assert.assertEquals(View.GONE, tabCardLabelView.getVisibility());
    }

    @Test
    @MediumTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testPriceStringTurnFeatureOff() {
        Tab tab = MockTab.createAndInitialize(1, mProfile);
        MockShoppingPersistedTabDataFetcher fetcher = new MockShoppingPersistedTabDataFetcher(tab);
        fetcher.setPriceStrings(EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
        testPriceString(
                tab, fetcher, View.VISIBLE, EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
        mGridModel.set(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER, null);
        PriceCardView priceCardView = mTabGridView.findViewById(R.id.price_info_box_outer);
        Assert.assertEquals(View.GONE, priceCardView.getVisibility());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testActionButtonImportantForAccessibility() {
        ImageView closableListActionButton = mTabListView.findViewById(R.id.end_button);
        ImageView closableGridActionButton = mTabGridView.findViewById(R.id.action_button);

        Assert.assertEquals(
                IMPORTANT_FOR_ACCESSIBILITY_YES,
                closableListActionButton.getImportantForAccessibility());
        Assert.assertEquals(
                IMPORTANT_FOR_ACCESSIBILITY_YES,
                closableGridActionButton.getImportantForAccessibility());

        ImageView selectableListActionButton = mSelectableTabListView.findViewById(R.id.end_button);
        ImageView selectableGridActionButton =
                mSelectableTabGridView.findViewById(R.id.action_button);

        Assert.assertEquals(
                IMPORTANT_FOR_ACCESSIBILITY_NO,
                selectableListActionButton.getImportantForAccessibility());
        Assert.assertEquals(
                IMPORTANT_FOR_ACCESSIBILITY_NO,
                selectableGridActionButton.getImportantForAccessibility());
    }

    private void testPriceString(
            Tab tab,
            MockShoppingPersistedTabDataFetcher fetcher,
            int expectedVisibility,
            String expectedCurrentPrice,
            String expectedPreviousPrice) {
        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(true);
        testGridSelected(mTabGridView, mGridModel);

        mGridModel.set(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER, fetcher);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING)) {
            TabCardLabelView tabCardLabelView = mTabGridView.findViewById(R.id.tab_card_label);

            if (tabCardLabelView == null) {
                ViewStub stub = mTabGridView.findViewById(R.id.tab_card_label_stub);
                Assert.assertNotNull(stub);
                Assert.assertEquals(View.GONE, expectedVisibility);
                return;
            }
            Assert.assertEquals(expectedVisibility, tabCardLabelView.getVisibility());
            if (expectedVisibility == View.VISIBLE) {
                TextView labelText = tabCardLabelView.findViewById(R.id.tab_label_text);

                String text = labelText.getText().toString();
                Assert.assertTrue(text.contains(expectedCurrentPrice));
                Assert.assertTrue(text.contains(expectedPreviousPrice));
            }
        } else {
            PriceCardView priceCardView = mTabGridView.findViewById(R.id.price_info_box_outer);

            Assert.assertEquals(expectedVisibility, priceCardView.getVisibility());
            if (expectedVisibility == View.VISIBLE) {
                TextView currentPrice = mTabGridView.findViewById(R.id.current_price);
                TextView previousPrice = mTabGridView.findViewById(R.id.previous_price);

                Assert.assertEquals(expectedCurrentPrice, currentPrice.getText());
                Assert.assertEquals(expectedPreviousPrice, previousPrice.getText());
            }
        }
    }

    static class MockShoppingPersistedTabData extends ShoppingPersistedTabData {
        private PriceDrop mPriceDrop;

        MockShoppingPersistedTabData(Tab tab) {
            super(tab);
        }

        public void setPriceStrings(String priceString, String previousPriceString) {
            mPriceDrop = new PriceDrop(priceString, previousPriceString);
        }

        @Override
        public PriceDrop getPriceDrop() {
            return mPriceDrop;
        }
    }

    /** Mock {@link TabListMediator.ShoppingPersistedTabDataFetcher} for testing purposes */
    static class MockShoppingPersistedTabDataFetcher
            extends TabListMediator.ShoppingPersistedTabDataFetcher {
        private ShoppingPersistedTabData mShoppingPersistedTabData;

        MockShoppingPersistedTabDataFetcher(Tab tab) {
            super(tab, null);
        }

        public void setPriceStrings(String priceString, String previousPriceString) {
            mShoppingPersistedTabData = new MockShoppingPersistedTabData(mTab);
            ((MockShoppingPersistedTabData) mShoppingPersistedTabData)
                    .setPriceStrings(priceString, previousPriceString);
        }

        public void setNullPriceDrop() {
            mShoppingPersistedTabData = new MockShoppingPersistedTabData(mTab);
        }

        @Override
        public void fetch(Callback<ShoppingPersistedTabData> callback) {
            callback.onResult(mShoppingPersistedTabData);
        }
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testFaviconFetcherAllViewsAndModels() {
        final TabFavicon tabFavicon =
                new ResourceTabFavicon(newDrawable(), StaticTabFaviconType.ROUNDED_GLOBE);
        TabFaviconFetcher fetcher =
                new TabFaviconFetcher() {
                    @Override
                    public void fetch(Callback<TabFavicon> callback) {
                        callback.onResult(tabFavicon);
                    }
                };

        testFaviconFetcher(
                mGridModel,
                mTabGridView,
                R.id.tab_favicon,
                fetcher,
                tabFavicon,
                TabGridViewBinder::onViewRecycled);

        testFaviconFetcher(
                mGridModel,
                mTabListView,
                R.id.start_icon,
                fetcher,
                tabFavicon,
                TabListViewBinder::onViewRecycled);

        testFaviconFetcher(
                mSelectableModel,
                mSelectableTabGridView,
                R.id.tab_favicon,
                fetcher,
                tabFavicon,
                TabGridViewBinder::onViewRecycled);

        testFaviconFetcher(
                mSelectableModel,
                mSelectableTabListView,
                R.id.start_icon,
                fetcher,
                tabFavicon,
                TabListViewBinder::onViewRecycled);

        testFaviconFetcher(
                mStripModel,
                mTabStripView,
                R.id.tab_strip_item_button,
                fetcher,
                tabFavicon,
                TabStripViewBinder::onViewRecycled);
        assertFalse(mStripModel.get(TabProperties.FAVICON_FETCHED));
    }

    @Test
    @MediumTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
    public void testColorIcon_Grid() {
        // Prevent errors with duplicate view attachment.
        mListMcp.destroy();

        FrameLayout gridContainer = mTabGridView.findViewById(R.id.tab_group_color_view_container);
        assertEquals(0, gridContainer.getChildCount());
        assertEquals(View.GONE, gridContainer.getVisibility());

        TabGroupColorViewProvider provider =
                new TabGroupColorViewProvider(
                        getActivity(),
                        new Token(1L, 2L),
                        /* isIncognito= */ false,
                        TabGroupColorId.BLUE,
                        /* tabGroupSyncService= */ null,
                        /* dataSharingService= */ null);

        mGridModel.set(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, provider);
        assertEquals(1, gridContainer.getChildCount());
        assertEquals(View.VISIBLE, gridContainer.getVisibility());

        mGridModel.set(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, null);
        assertEquals(0, gridContainer.getChildCount());
        assertEquals(View.GONE, gridContainer.getVisibility());

        mGridModel.set(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, provider);
        assertEquals(1, gridContainer.getChildCount());
        assertEquals(View.VISIBLE, gridContainer.getVisibility());

        TabGridViewBinder.onViewRecycled(mGridModel, mTabGridView);
        assertEquals(0, gridContainer.getChildCount());
        assertEquals(View.GONE, gridContainer.getVisibility());

        mGridModel.set(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, null);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
    public void testColorIcon_List() {
        // Prevent errors with duplicate view attachment.
        mSelectableMcp.destroy();
        mGridMcp.destroy();

        FrameLayout listContainer = mTabListView.findViewById(R.id.after_title_container);
        assertEquals(0, listContainer.getChildCount());
        assertEquals(View.GONE, listContainer.getVisibility());

        TabGroupColorViewProvider provider =
                new TabGroupColorViewProvider(
                        getActivity(),
                        new Token(1L, 2L),
                        /* isIncognito= */ false,
                        TabGroupColorId.BLUE,
                        /* tabGroupSyncService= */ null,
                        /* dataSharingService= */ null);

        mGridModel.set(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, provider);
        assertEquals(1, listContainer.getChildCount());
        assertEquals(View.VISIBLE, listContainer.getVisibility());

        mGridModel.set(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, null);
        assertEquals(0, listContainer.getChildCount());
        assertEquals(View.GONE, listContainer.getVisibility());

        mGridModel.set(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, provider);
        assertEquals(1, listContainer.getChildCount());
        assertEquals(View.VISIBLE, listContainer.getVisibility());

        TabListViewBinder.onViewRecycled(mGridModel, mTabListView);
        assertEquals(0, listContainer.getChildCount());
        assertEquals(View.GONE, listContainer.getVisibility());

        mGridModel.set(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, null);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testTabStripNotificationBubble_toggleHasNotification() {
        ImageView notificationView = mTabStripView.findViewById(R.id.tab_strip_notification_bubble);

        assertEquals(notificationView.getVisibility(), View.GONE);
        mStripModel.set(TabProperties.HAS_NOTIFICATION_BUBBLE, true);
        assertEquals(notificationView.getVisibility(), View.VISIBLE);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testTabStripNotificationBubble_contentDescription() {
        final String title = "vacation and relaxing";
        Resources resources = ContextUtils.getApplicationContext().getResources();
        ImageButton button = mTabStripView.findViewById(R.id.tab_strip_item_button);
        mStripModel.set(TabProperties.TITLE, title);

        // Test that as long as the strip item is selected, the notification has no bearing.
        mStripModel.set(TabProperties.IS_SELECTED, true);
        mStripModel.set(TabProperties.HAS_NOTIFICATION_BUBBLE, true);
        assertEquals(
                button.getContentDescription(),
                resources.getString(R.string.accessibility_tabstrip_btn_close_tab, title));
        // Test that this tab is not the selected tab but has a notification.
        mStripModel.set(TabProperties.IS_SELECTED, false);
        assertEquals(
                button.getContentDescription(),
                resources.getString(R.string.accessibility_tabstrip_tab_notification, title));
        // Test that this tab has no notification.
        mStripModel.set(TabProperties.HAS_NOTIFICATION_BUBBLE, false);
        assertEquals(
                button.getContentDescription(),
                resources.getString(R.string.accessibility_tabstrip_tab, title));
    }

    @FunctionalInterface
    interface Recycler {
        void onViewRecycled(PropertyModel model, View view);
    }

    private void testFaviconFetcher(
            PropertyModel model,
            View parentView,
            @IdRes int id,
            TabFaviconFetcher fetcher,
            TabFavicon expectedFavicon,
            Recycler recycler) {
        ImageView faviconView = parentView.findViewById(id);
        model.set(TabProperties.IS_SELECTED, true);

        model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(faviconView.getDrawable());

        model.set(TabProperties.FAVICON_FETCHER, fetcher);
        assertNotNull(faviconView.getDrawable());
        assertEquals(faviconView.getDrawable(), expectedFavicon.getSelectedDrawable());

        model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(faviconView.getDrawable());

        model.set(TabProperties.FAVICON_FETCHER, fetcher);
        assertNotNull(faviconView.getDrawable());
        assertEquals(faviconView.getDrawable(), expectedFavicon.getSelectedDrawable());

        recycler.onViewRecycled(model, parentView);
        assertNull(faviconView.getDrawable());
    }

    private Drawable newDrawable() {
        Bitmap image = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        Resources resources = ContextUtils.getApplicationContext().getResources();
        return new BitmapDrawable(resources, image);
    }

    private void mockCurrencyFormatter() {
        doAnswer(
                        new Answer<String>() {
                            @Override
                            public String answer(InvocationOnMock invocation) {
                                StringBuilder sb = new StringBuilder();
                                sb.append(USD_CURRENCY_SYMBOL);
                                sb.append(invocation.getArguments()[2]);
                                return sb.toString();
                            }
                        })
                .when(mCurrencyFormatterJniMock)
                .format(anyLong(), any(CurrencyFormatter.class), anyString());
    }

    private void mockUrlUtilities() {
        doAnswer(
                        new Answer<String>() {
                            @Override
                            public String answer(InvocationOnMock invocation) {
                                return (String) invocation.getArguments()[0];
                            }
                        })
                .when(mUrlUtilitiesJniMock)
                .escapeQueryParamValue(anyString(), anyBoolean());
    }

    private void mockOptimizationGuideResponse(
            @OptimizationGuideDecision int decision, Any metadata) {
        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                OptimizationGuideCallback callback =
                                        (OptimizationGuideCallback) invocation.getArguments()[2];
                                callback.onOptimizationGuideDecision(decision, metadata);
                                return null;
                            }
                        })
                .when(mOptimizationGuideBridge)
                .canApplyOptimization(
                        any(GURL.class),
                        any(HintsProto.OptimizationType.class),
                        any(OptimizationGuideCallback.class));
    }

    @Override
    public void tearDownTest() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mStripMcp.destroy();
                    mGridMcp.destroy();
                    mSelectableMcp.destroy();
                });
        super.tearDownTest();
    }
}
