// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.merchant_viewer.PageInfoStoreInfoController.StoreInfoActionHandler;
import org.chromium.chrome.browser.page_info.ChromePageInfo;
import org.chromium.chrome.browser.page_info.ChromePageInfoHighlight;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.MerchantInfo;
import org.chromium.components.commerce.core.ShoppingService.MerchantInfoCallback;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.url.GURL;

import java.io.IOException;

/** Tests for PageInfoStoreInfo view. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.COMMERCE_MERCHANT_VIEWER})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction({DeviceFormFactor.PHONE})
@Batch(Batch.PER_CLASS)
public class PageInfoStoreInfoViewTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule public JniMocker mMocker = new JniMocker();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_SHOPPING)
                    .build();

    @Mock private StoreInfoActionHandler mMockStoreInfoActionHandler;
    @Mock private ShoppingService mMockShoppingService;

    private final MerchantInfo mFakeMerchantTrustSignals =
            new MerchantInfo(4.5f, 100, new GURL("http://fake/url"), false, 0f, false, false);

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShoppingServiceFactory.setShoppingServiceForTesting(mMockShoppingService);
        doReturn(true).when(mMockShoppingService).isMerchantViewerEnabled();
    }

    // dialogCheck ensures that a dialog is in focus when checking the view. If not
    // used it can cause flakiness issues for apis >= 30.
    private void openPageInfoFromStoreIcon(boolean fromStoreIcon, boolean dialogCheck) {
        ChromeActivity activity = sActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    new ChromePageInfo(
                                    activity.getModalDialogManagerSupplier(),
                                    null,
                                    PageInfoController.OpenedFromSource.TOOLBAR,
                                    () -> mMockStoreInfoActionHandler,
                                    null,
                                    null)
                            .show(tab, ChromePageInfoHighlight.forStoreInfo(fromStoreIcon));
                });
        onViewWaiting(allOf(withId(R.id.page_info_url_wrapper), isDisplayed()), dialogCheck);
    }

    @Test
    @MediumTest
    public void testStoreInfoRowInvisibleWithoutData() {
        mockShoppingServiceResponse(null);
        openPageInfoFromStoreIcon(false, true); // fromStoreIcon, dialogCheck
        verifyStoreRowShowing(false);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStoreInfoRowVisibleWithData() throws IOException {
        mockShoppingServiceResponse(mFakeMerchantTrustSignals);
        openPageInfoFromStoreIcon(false, true); // fromStoreIcon, dialogCheck
        verifyStoreRowShowing(true);
        renderTestForStoreInfoRow("page_info_store_info_row");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStoreInfoRowVisibleWithData_Highlight() throws IOException {
        mockShoppingServiceResponse(mFakeMerchantTrustSignals);
        openPageInfoFromStoreIcon(true, false); // fromStoreIcon, dialogCheck
        verifyStoreRowShowing(true);
        renderTestForStoreInfoRow("page_info_store_info_row_highlight");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStoreInfoRowVisibleWithData_WithoutReviews() throws IOException {
        MerchantInfo fakeMerchantTrustSignals =
                new MerchantInfo(4.5f, 0, new GURL("http://fake/url"), false, 0f, false, false);
        mockShoppingServiceResponse(fakeMerchantTrustSignals);

        openPageInfoFromStoreIcon(false, true); // fromStoreIcon, dialogCheck
        verifyStoreRowShowing(true);
        renderTestForStoreInfoRow("page_info_store_info_row_without_reviews");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStoreInfoRowVisibleWithData_WithoutRating() throws IOException {
        MerchantInfo fakeMerchantTrustSignals =
                new MerchantInfo(0f, 0, new GURL("http://fake/url"), true, 0f, false, false);
        mockShoppingServiceResponse(fakeMerchantTrustSignals);

        openPageInfoFromStoreIcon(false, true); // fromStoreIcon, dialogCheck
        verifyStoreRowShowing(true);
        renderTestForStoreInfoRow("page_info_store_info_row_without_rating");
    }

    @Test
    @MediumTest
    public void testStoreInfoRowClick() {
        mockShoppingServiceResponse(mFakeMerchantTrustSignals);
        openPageInfoFromStoreIcon(false, true); // fromStoreIcon, dialogCheck
        verifyStoreRowShowing(true);
        onView(withId(PageInfoStoreInfoController.STORE_INFO_ROW_ID)).perform(click());
        onView(withId(R.id.page_info_url_wrapper)).check(doesNotExist());
        verify(mMockStoreInfoActionHandler, times(1)).onStoreInfoClicked(any(MerchantInfo.class));
    }

    private void mockShoppingServiceResponse(MerchantInfo merchantInfo) {
        doAnswer(
                        (Answer<Void>)
                                invocation -> {
                                    GURL url = (GURL) invocation.getArguments()[0];
                                    MerchantInfoCallback callback =
                                            (MerchantInfoCallback) invocation.getArguments()[1];
                                    callback.onResult(url, merchantInfo);
                                    return null;
                                })
                .when(mMockShoppingService)
                .getMerchantInfoForUrl(any(GURL.class), any(MerchantInfoCallback.class));
    }

    private void verifyStoreRowShowing(boolean isVisible) {
        onView(withId(PageInfoStoreInfoController.STORE_INFO_ROW_ID))
                .check(matches(isVisible ? isDisplayed() : not(isDisplayed())));
    }

    private void renderTestForStoreInfoRow(String renderId) {
        onView(withId(PageInfoStoreInfoController.STORE_INFO_ROW_ID))
                .check(
                        (v, noMatchException) -> {
                            if (noMatchException != null) throw noMatchException;
                            try {
                                mRenderTestRule.render(v, renderId);
                            } catch (IOException e) {
                                assert false : "Render test failed due to " + e;
                            }
                        });
    }
}
