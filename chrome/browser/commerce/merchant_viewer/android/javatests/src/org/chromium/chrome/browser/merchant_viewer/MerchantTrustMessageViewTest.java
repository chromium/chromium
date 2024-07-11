// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.ViewGroup.LayoutParams;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.FeatureList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMessageViewModel.MessageActionsHandler;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.commerce.core.ShoppingService.MerchantInfo;
import org.chromium.components.messages.MessageBannerView;
import org.chromium.components.messages.MessageBannerViewBinder;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.List;

/** Tests for MerchantTrustMessageView. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public class MerchantTrustMessageViewTest extends BlankUiTestActivityTestCase {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(
                            ChromeRenderTestRule.Component.UI_BROWSER_SHOPPING_MERCHANT_TRUST)
                    .build();

    public MerchantTrustMessageViewTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Mock private MessageActionsHandler mMockActionHandler;

    private Activity mActivity;
    private MessageBannerView mMessageBannerView;
    private LayoutParams mParams;
    private MerchantInfo mMerchantInfo =
            new MerchantInfo(3.51234f, 1640, new GURL("http://dummy/url"), false, 0f, false, false);

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        mActivity = getActivity();
        mMessageBannerView =
                (MessageBannerView)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.message_banner_view, null, false);
        mParams =
                new LayoutParams(
                        LayoutParams.MATCH_PARENT,
                        mActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.message_banner_height));
    }

    @Override
    public void tearDownTest() throws Exception {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
        super.tearDownTest();
    }

    private void createModelAndSetView(MerchantInfo merchantInfo) {
        PropertyModel propertyModel =
                MerchantTrustMessageViewModel.create(
                        mActivity, merchantInfo, "fake_url", mMockActionHandler);
        PropertyModelChangeProcessor.create(
                propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity.setContentView(mMessageBannerView, mParams);
                });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderMessage_UseRatingBar() throws IOException {
        setUseRatingBarParam("true");

        createModelAndSetView(mMerchantInfo);
        mRenderTestRule.render(mMessageBannerView, "merchant_trust_message_use_rating_bar");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderMessage_NotUseRatingBar() throws IOException {
        setUseRatingBarParam("false");

        createModelAndSetView(mMerchantInfo);
        mRenderTestRule.render(mMessageBannerView, "merchant_trust_message_not_use_rating_bar");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderMessage_NoRatingReviews() throws IOException {
        setUseRatingBarParam("true");

        MerchantInfo merchantInfo =
                new MerchantInfo(
                        3.51234f, 0, new GURL("http://dummy/url"), false, 0f, false, false);
        createModelAndSetView(merchantInfo);
        mRenderTestRule.render(mMessageBannerView, "merchant_trust_message_no_rating_reviews");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderMessage_IntegerRatingValue() throws IOException {
        setUseRatingBarParam("true");

        MerchantInfo merchantInfo =
                new MerchantInfo(4f, 1640, new GURL("http://dummy/url"), false, 0f, false, false);
        createModelAndSetView(merchantInfo);
        mRenderTestRule.render(mMessageBannerView, "merchant_trust_message_integer_rating_value");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderMessage_Alternative1() throws IOException {
        setMessageUIParams("true", "false", "1", "1");

        createModelAndSetView(mMerchantInfo);
        mRenderTestRule.render(mMessageBannerView, "merchant_trust_message_alternative1");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderMessage_Alternative2() throws IOException {
        setMessageUIParams("true", "true", "0", "0");

        createModelAndSetView(mMerchantInfo);
        mRenderTestRule.render(mMessageBannerView, "merchant_trust_message_alternative2");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderMessage_Alternative3() throws IOException {
        setMessageUIParams("true", "false", "1", "2");

        createModelAndSetView(mMerchantInfo);
        mRenderTestRule.render(mMessageBannerView, "merchant_trust_message_alternative3");
    }

    private void setUseRatingBarParam(String useRatingBar) {
        setMessageUIParams(useRatingBar, "false", "0", "1");
    }

    private void setMessageUIParams(
            String useRatingBar, String useGoogleIcon, String titleUI, String descriptionUI) {
        // TODO: Remove use of setDisableNativeForTesting(), probably needed due to isInitialized()
        // in MerchantViewerConfig.
        FeatureList.setDisableNativeForTesting(true);
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_USE_RATING_BAR_PARAM,
                useRatingBar);
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_USE_GOOGLE_ICON_PARAM,
                useGoogleIcon);
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_TITLE_UI_PARAM,
                titleUI);
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_DESCRIPTION_UI_PARAM,
                descriptionUI);
        FeatureList.setTestValues(testValues);
    }
}
