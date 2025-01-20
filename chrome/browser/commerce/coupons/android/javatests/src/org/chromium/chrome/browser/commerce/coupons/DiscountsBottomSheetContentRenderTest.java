// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce.coupons;

import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.ALL_KEYS;
import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.COPY_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.COPY_BUTTON_TEXT;
import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.DESCRIPTION_DETAIL;
import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.DISCOUNT_CODE;
import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.EXPIRY_TIME;
import static org.chromium.ui.test.util.RenderTestRule.Component.UI_BROWSER_SHOPPING;

import android.app.Activity;
import android.os.Build.VERSION_CODES;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.notifications.NotificationFeatureMap;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/** Render Tests for the discounts bottom sheet content. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({NotificationFeatureMap.CACHE_NOTIIFICATIONS_ENABLED})
@Batch(Batch.UNIT_TESTS)
public class DiscountsBottomSheetContentRenderTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(UI_BROWSER_SHOPPING)
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mMockTab;

    private ModelList mModelList;
    private View mContentView;
    private RecyclerView mRecyclerView;
    private DiscountsBottomSheetContentCoordinator mCoordinator;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new DiscountsBottomSheetContentCoordinator(sActivity, () -> mMockTab);
                    mContentView = mCoordinator.getContentViewForTesting();
                    mRecyclerView = mCoordinator.getRecyclerViewForTesting();
                    mModelList = mCoordinator.getModelListForTesting();
                    sActivity.setContentView(mContentView);
                });
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisableIf.Build(sdk_is_less_than = VERSION_CODES.P, message = "crbug.com/382335140")
    public void testSingleDiscountContent() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModelList.add(
                            new ListItem(
                                    0,
                                    createPropertyModel(
                                            "SAVE20",
                                            "20% off all Muir silverware",
                                            "Valid until 07/21/2024")));
                });
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mRecyclerView);
        mRenderTestRule.render(mContentView, "single_discount_content");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisableIf.Build(sdk_is_less_than = VERSION_CODES.P, message = "crbug.com/382335140")
    public void testMultipleDiscountsContents() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModelList.add(
                            new ListItem(
                                    0,
                                    createPropertyModel(
                                            "SAVE20",
                                            "20% off all Muir silverware",
                                            "Valid until 07/21/2024")));
                    mModelList.add(
                            new ListItem(
                                    0,
                                    createPropertyModel(
                                            "SAVE10",
                                            "15% off all Nike shoes",
                                            "Valid until 08/19/2024")));
                    mModelList.add(
                            new ListItem(
                                    0,
                                    createPropertyModel(
                                            "SAVE40",
                                            "40% off all iPhone",
                                            "Valid until 10/21/2024")));
                });
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mRecyclerView);
        mRenderTestRule.render(mContentView, "multiple_discounts_contents");
    }

    private PropertyModel createPropertyModel(
            String discountCode, String descriptionDetail, String expiryTime) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(DISCOUNT_CODE, discountCode)
                .with(DESCRIPTION_DETAIL, descriptionDetail)
                .with(EXPIRY_TIME, expiryTime)
                .with(COPY_BUTTON_TEXT, "Copy")
                .with(COPY_BUTTON_ON_CLICK_LISTENER, (v) -> {})
                .build();
    }
}
