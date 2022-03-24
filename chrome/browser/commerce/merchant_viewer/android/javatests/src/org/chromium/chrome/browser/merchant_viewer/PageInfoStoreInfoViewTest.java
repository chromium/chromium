// Copyright 2021 The Chromium Authors. All rights reserved.
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
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.MediumTest;

import com.google.protobuf.ByteString;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;

import org.chromium.base.StrictModeContext;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.merchant_viewer.PageInfoStoreInfoController.StoreInfoActionHandler;
import org.chromium.chrome.browser.merchant_viewer.proto.MerchantTrustSignalsOuterClass.MerchantTrustSignalsV2;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge.OptimizationGuideCallback;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeJni;
import org.chromium.chrome.browser.page_info.ChromePageInfo;
import org.chromium.chrome.browser.page_info.ChromePageInfoHighlight;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.Any;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.PageInfoFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.url.GURL;

import java.io.IOException;

/**
 * Tests for PageInfoStoreInfo view.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Features.EnableFeatures(
        {PageInfoFeatures.PAGE_INFO_STORE_INFO_NAME, ChromeFeatureList.COMMERCE_MERCHANT_VIEWER})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
@Batch(Batch.PER_CLASS)
public class PageInfoStoreInfoViewTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule
    public JniMocker mMocker = new JniMocker();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_SHOPPING)
                    .build();

    @Mock
    private OptimizationGuideBridge.Natives mMockOptimizationGuideBridgeJni;

    @Mock
    private StoreInfoActionHandler mMockStoreInfoActionHandler;

    private final MerchantTrustSignalsV2 mFakeMerchantTrustSigals =
            MerchantTrustSignalsV2.newBuilder()
                    .setMerchantStarRating(4.5f)
                    .setMerchantCountRating(100)
                    .setMerchantDetailsPageUrl("http://dummy/url")
                    .build();

    private final Any mAnyMerchantTrustSignals =
            Any.newBuilder()
                    .setValue(ByteString.copyFrom(mFakeMerchantTrustSigals.toByteArray()))
                    .build();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMocker.mock(OptimizationGuideBridgeJni.TEST_HOOKS, mMockOptimizationGuideBridgeJni);
        doReturn(1L).when(mMockOptimizationGuideBridgeJni).init();
    }

    private void openPageInfoFromStoreIcon(boolean fromStoreIcon) {
        ChromeActivity activity = sActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            new ChromePageInfo(activity.getModalDialogManagerSupplier(), null,
                    PageInfoController.OpenedFromSource.TOOLBAR, () -> mMockStoreInfoActionHandler)
                    .show(tab, ChromePageInfoHighlight.forStoreInfo(fromStoreIcon));
        });
        onViewWaiting(allOf(withId(R.id.page_info_url_wrapper), isDisplayed()));
    }

    private void openPageInfo() {
        openPageInfoFromStoreIcon(false);
    }

    @Test
    @MediumTest
    public void testStoreInfoRowInvisibleWithoutData() {
        mockOptimizationGuideResponse(
                mMockOptimizationGuideBridgeJni, OptimizationGuideDecision.TRUE, null);
        openPageInfo();
        verifyStoreRowShowing(false);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStoreInfoRowVisibleWithData() throws IOException {
        mockOptimizationGuideResponse(mMockOptimizationGuideBridgeJni,
                OptimizationGuideDecision.TRUE, mAnyMerchantTrustSignals);
        openPageInfo();
        verifyStoreRowShowing(true);
        renderTestForStoreInfoRow("page_info_store_info_row");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStoreInfoRowVisibleWithData_Highlight() throws IOException {
        mockOptimizationGuideResponse(mMockOptimizationGuideBridgeJni,
                OptimizationGuideDecision.TRUE, mAnyMerchantTrustSignals);
        openPageInfoFromStoreIcon(true);
        verifyStoreRowShowing(true);
        renderTestForStoreInfoRow("page_info_store_info_row_highlight");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStoreInfoRowVisibleWithData_WithoutReviews() throws IOException {
        MerchantTrustSignalsV2 fakeMerchantTrustSigals =
                MerchantTrustSignalsV2.newBuilder()
                        .setMerchantStarRating(4.5f)
                        .setMerchantCountRating(0)
                        .setMerchantDetailsPageUrl("http://dummy/url")
                        .build();

        Any anyMerchantTrustSignals =
                Any.newBuilder()
                        .setValue(ByteString.copyFrom(fakeMerchantTrustSigals.toByteArray()))
                        .build();

        mockOptimizationGuideResponse(mMockOptimizationGuideBridgeJni,
                OptimizationGuideDecision.TRUE, anyMerchantTrustSignals);
        openPageInfo();
        verifyStoreRowShowing(true);
        renderTestForStoreInfoRow("page_info_store_info_row_without_reviews");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStoreInfoRowVisibleWithData_WithoutRating() throws IOException {
        MerchantTrustSignalsV2 fakeMerchantTrustSigals =
                MerchantTrustSignalsV2.newBuilder()
                        .setMerchantStarRating(0)
                        .setMerchantCountRating(0)
                        .setMerchantDetailsPageUrl("http://dummy/url")
                        .setHasReturnPolicy(true)
                        .build();

        Any anyMerchantTrustSignals =
                Any.newBuilder()
                        .setValue(ByteString.copyFrom(fakeMerchantTrustSigals.toByteArray()))
                        .build();

        mockOptimizationGuideResponse(mMockOptimizationGuideBridgeJni,
                OptimizationGuideDecision.TRUE, anyMerchantTrustSignals);
        openPageInfo();
        verifyStoreRowShowing(true);
        renderTestForStoreInfoRow("page_info_store_info_row_without_rating");
    }

    @Test
    @MediumTest
    public void testStoreInfoRowClick() {
        mockOptimizationGuideResponse(mMockOptimizationGuideBridgeJni,
                OptimizationGuideDecision.TRUE, mAnyMerchantTrustSignals);
        openPageInfo();
        verifyStoreRowShowing(true);
        onView(withId(PageInfoStoreInfoController.STORE_INFO_ROW_ID)).perform(click());
        onView(withId(R.id.page_info_url_wrapper)).check(doesNotExist());
        verify(mMockStoreInfoActionHandler, times(1))
                .onStoreInfoClicked(any(MerchantTrustSignalsV2.class));
    }

    private void mockOptimizationGuideResponse(OptimizationGuideBridge.Natives optimizationGuideJni,
            @OptimizationGuideDecision int decision, Any metadata) {
        doAnswer((Answer<Void>) invocation -> {
            OptimizationGuideCallback callback =
                    (OptimizationGuideCallback) invocation.getArguments()[3];
            callback.onOptimizationGuideDecision(decision, metadata);
            return null;
        })
                .when(optimizationGuideJni)
                .canApplyOptimization(
                        anyLong(), any(GURL.class), anyInt(), any(OptimizationGuideCallback.class));
    }

    private void verifyStoreRowShowing(boolean isVisible) {
        onView(withId(PageInfoStoreInfoController.STORE_INFO_ROW_ID))
                .check(matches(isVisible ? isDisplayed() : not(isDisplayed())));
    }

    private void renderTestForStoreInfoRow(String renderId) {
        onView(withId(PageInfoStoreInfoController.STORE_INFO_ROW_ID))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;
                    // Allow disk writes and slow calls to render from UI thread.
                    try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
                        mRenderTestRule.render(v, renderId);
                    } catch (IOException e) {
                        assert false : "Render test failed due to " + e;
                    }
                });
    }
}