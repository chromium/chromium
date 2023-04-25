// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.bookmarks.BookmarkRow.Location;
import org.chromium.chrome.browser.commerce.PriceTrackingUtils;
import org.chromium.chrome.browser.commerce.PriceTrackingUtilsJni;
import org.chromium.chrome.browser.commerce.ShoppingFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.browser_ui.widget.dragreorder.DragStateDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.url.JUnitTestGURLs;

/**
 * Tests for the Shopping power bookmarks experience.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures(ChromeFeatureList.BOOKMARKS_REFRESH)
public class PowerBookmarkShoppingItemRowTest extends BlankUiTestActivityTestCase {
    private static final long CURRENCY_MUTLIPLIER = 1000000;
    private static final String TITLE = "PowerBookmarkShoppingItemRow";

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private PriceTrackingUtils.Natives mMockPriceTrackingUtilsJni;
    @Mock
    private ImageFetcher mImageFetcher;
    @Mock
    private CurrencyFormatter mCurrencyFormatter;
    @Mock
    private BookmarkModel mBookmarkModel;
    @Mock
    private SnackbarManager mSnackbarManager;
    @Mock
    private Profile mProfile;
    @Mock
    private BookmarkItem mBookmarkItem;
    @Mock
    private BookmarkDelegate mDelegate;
    @Mock
    private SelectionDelegate<BookmarkId> mSelectionDelegate;
    @Mock
    private DragStateDelegate mDragStateDelegate;
    @Mock
    private LargeIconBridge mLargeIconBridge;
    @Mock
    private RoundedIconGenerator mRoundedIconGenerator;

    private BookmarkId mBookmarkId;
    private Bitmap mBitmap;
    private PowerBookmarkShoppingItemRow mPowerBookmarkShoppingItemRow;
    private ViewGroup mContentView;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);
        ShoppingFeatures.setShoppingListEligibleForTesting(true);

        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        mJniMocker.mock(PriceTrackingUtilsJni.TEST_HOOKS, mMockPriceTrackingUtilsJni);

        mBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mBitmap.eraseColor(Color.GREEN);

        ArgumentCaptor<String> currencyCaptor = ArgumentCaptor.forClass(String.class);
        doAnswer((invocation) -> { return "$" + currencyCaptor.getValue(); })
                .when(mCurrencyFormatter)
                .format(currencyCaptor.capture());

        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(
                                ShoppingSpecifics.newBuilder().setProductClusterId(1234L).build())
                        .build();

        mBookmarkId = new BookmarkId(1, BookmarkType.NORMAL);
        doReturn(mBookmarkModel).when(mDelegate).getModel();
        doReturn(mSelectionDelegate).when(mDelegate).getSelectionDelegate();
        doReturn(mDragStateDelegate).when(mDelegate).getDragStateDelegate();
        doReturn(mLargeIconBridge).when(mDelegate).getLargeIconBridge();
        doReturn(TITLE).when(mBookmarkItem).getTitle();
        doReturn(JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL)).when(mBookmarkItem).getUrl();
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mBookmarkItem).getUrlForDisplay();
        doReturn(mBookmarkItem).when(mBookmarkModel).getBookmarkById(mBookmarkId);
        doReturn(meta).when(mBookmarkModel).getPowerBookmarkMeta(mBookmarkId);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContentView = new LinearLayout(getActivity());
            mContentView.setBackgroundColor(Color.WHITE);

            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);

            getActivity().setContentView(mContentView, params);
            mPowerBookmarkShoppingItemRow =
                    BookmarkManagerCoordinator.buildShoppingItemView(mContentView);
            mPowerBookmarkShoppingItemRow.setRoundedIconGeneratorForTesting(mRoundedIconGenerator);
            mPowerBookmarkShoppingItemRow.onDelegateInitialized(mDelegate);
            mPowerBookmarkShoppingItemRow.init(
                    mImageFetcher, mBookmarkModel, mSnackbarManager, mProfile);
            mPowerBookmarkShoppingItemRow.setCurrencyFormatterForTesting(mCurrencyFormatter);
        });
    }

    @Override
    public void tearDownTest() throws Exception {
        ShoppingFeatures.setShoppingListEligibleForTesting(null);
    }

    @Test
    @SmallTest
    public void initPriceTrackingUI_NullImage() {
        doAnswer((invocation) -> {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> ((Callback) invocation.getArgument(1)).onResult(null));
            return null;
        })
                .when(mImageFetcher)
                .fetchImage(any(), any());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPowerBookmarkShoppingItemRow.initPriceTrackingUI("http://foo.com/img", true, 100, 100);
        });

        Assert.assertFalse(mPowerBookmarkShoppingItemRow.getFaviconCancelledForTesting());
    }

    @Test
    @SmallTest
    public void testIconPropsAreInitialized() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPowerBookmarkShoppingItemRow.setBookmarkId(mBookmarkId, Location.TOP, false);
            // This will crash if the icon properties aren't initialized.
            mPowerBookmarkShoppingItemRow.onLargeIconAvailable(
                    mBitmap, Color.GREEN, false, IconType.FAVICON);
        });
    }
}
