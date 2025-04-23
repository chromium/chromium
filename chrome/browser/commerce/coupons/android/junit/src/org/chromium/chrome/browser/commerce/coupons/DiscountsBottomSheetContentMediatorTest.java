// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce.coupons;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.COPY_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.COPY_BUTTON_TEXT;
import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.DESCRIPTION_DETAIL;
import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.DISCOUNT_CODE;
import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.EXPIRY_TIME;

import android.app.Activity;
import android.content.ClipboardManager;
import android.content.Context;
import android.view.View.OnClickListener;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.commerce.core.DiscountInfo;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.DiscountInfoCallback;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit Tests for {@link DiscountsBottomSheetContentMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
public class DiscountsBottomSheetContentMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mMockTab;
    @Mock private Profile mMockProfile;
    @Mock private ShoppingService mMockShoppingService;
    @Mock Callback<Boolean> mMockCallback;

    private Activity mActivity;
    private ClipboardManager mClipboardManager;
    private ModelList mModelList;
    private DiscountsBottomSheetContentMediator mMediator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mClipboardManager =
                (ClipboardManager) mActivity.getSystemService(Context.CLIPBOARD_SERVICE);
        mModelList = new ModelList();

        doReturn(mMockProfile).when(mMockTab).getProfile();

        ShoppingServiceFactory.setShoppingServiceForTesting(mMockShoppingService);
        doReturn(true).when(mMockShoppingService).isDiscountEligibleToShowOnNavigation();

        mMediator = new DiscountsBottomSheetContentMediator(mActivity, () -> mMockTab, mModelList);
    }

    @Test
    public void testRequestShowContent_discountNotEligible() {
        doReturn(false).when(mMockShoppingService).isDiscountEligibleToShowOnNavigation();
        mMediator.requestShowContent(mMockCallback);
        verify(mMockCallback).onResult(false);
    }

    @Test
    public void testRequestShowContent_emptyInfoList() {
        setShoppingServiceGetDiscountInfoForUrl(new ArrayList<DiscountInfo>());
        mMediator.requestShowContent(mMockCallback);
        verify(mMockCallback).onResult(false);
    }

    @Test
    @DisabledTest(message = "https://crbug.com/393301380")
    public void testRequestShowContent_contentReady() {
        List<DiscountInfo> infoList = createDiscountInfoList();
        setShoppingServiceGetDiscountInfoForUrl(infoList);

        mMediator.requestShowContent(mMockCallback);
        verify(mMockCallback).onResult(true);

        for (int i = 0; i < infoList.size(); i++) {
            assertEquals(
                    mModelList.get(i).model.get(DISCOUNT_CODE), infoList.get(i).discountCode.get());
            assertEquals(
                    mModelList.get(i).model.get(DESCRIPTION_DETAIL),
                    infoList.get(i).descriptionDetail);
            assertEquals("Valid until 09/20/2024", mModelList.get(i).model.get(EXPIRY_TIME));
            assertEquals("Copy", mModelList.get(i).model.get(COPY_BUTTON_TEXT));
        }
    }

    @Test
    public void testCloseContent() {
        setShoppingServiceGetDiscountInfoForUrl(createDiscountInfoList());
        mMediator.requestShowContent(mMockCallback);
        assertEquals(3, mModelList.size());
        mMediator.closeContent();
        assertEquals(0, mModelList.size());
    }

    @Test
    public void testCopyButtonOnclickListener() {
        List<DiscountInfo> infoList = createDiscountInfoList();
        setShoppingServiceGetDiscountInfoForUrl(infoList);
        mMediator.requestShowContent(mMockCallback);
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Commerce.Discounts.BottomSheet.ClusterTypeOnCopy", 1)
                        .build();
        for (int i = 0; i < mModelList.size(); i++) {
            PropertyModel model = mModelList.get(i).model;
            OnClickListener copyButtonOnClickListener = model.get(COPY_BUTTON_ON_CLICK_LISTENER);
            assertNotNull(copyButtonOnClickListener);
            copyButtonOnClickListener.onClick(null);
            assertEquals(mClipboardManager.getText(), model.get(DISCOUNT_CODE));
            assertEquals("Copied", model.get(COPY_BUTTON_TEXT));
            for (int j = 0; j < mModelList.size(); j++) {
                if (j != i) {
                    assertEquals("Copy", mModelList.get(j).model.get(COPY_BUTTON_TEXT));
                }
            }
        }
        watcher.assertExpected();
    }

    private List<DiscountInfo> createDiscountInfoList() {
        DiscountInfo discountInfo1 = createDiscountInfo("SAVE20", 1, "20% off all Muir silverware");
        DiscountInfo discountInfo2 = createDiscountInfo("SAVE15", 0, "15% off all Nike shoes");
        DiscountInfo discountInfo3 = createDiscountInfo("SAVE40", 2, "40% off all iPhone");
        return Arrays.asList(discountInfo1, discountInfo2, discountInfo3);
    }

    private DiscountInfo createDiscountInfo(
            String discountCode, int clusterType, String descriptionDetail) {
        return new DiscountInfo(
                clusterType,
                0,
                "en-US",
                descriptionDetail,
                null,
                "",
                discountCode,
                0L,
                true,
                true,
                1.7267948E+9f,
                0L);
    }

    private void setShoppingServiceGetDiscountInfoForUrl(List<DiscountInfo> infoList) {
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            ((DiscountInfoCallback) invocation.getArgument(1))
                                    .onResult(JUnitTestGURLs.EXAMPLE_URL, infoList);
                            return null;
                        })
                .when(mMockShoppingService)
                .getDiscountInfoForUrl(any(), any());
    }
}
