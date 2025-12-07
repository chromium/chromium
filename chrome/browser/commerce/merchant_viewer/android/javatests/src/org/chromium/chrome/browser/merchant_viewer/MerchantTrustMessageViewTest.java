// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
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
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.List;

/** Tests for MerchantTrustMessageView. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class MerchantTrustMessageViewTest {
    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

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

    private MessageBannerView mMessageBannerView;
    private View mMessageBannerContent;
    private LayoutParams mParams;
    private final MerchantInfo mMerchantInfo =
            new MerchantInfo(3.51234f, 1640, new GURL("http://dummy/url"), false, 0f, false, false);

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() {
        mMessageBannerView =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                (MessageBannerView)
                                        LayoutInflater.from(sActivity)
                                                .inflate(
                                                        R.layout.message_banner_view, null, false));
        mParams =
                new LayoutParams(
                        LayoutParams.MATCH_PARENT,
                        sActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.message_banner_main_content_height));
    }

    @After
    public void tearDown() throws Exception {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    private void createModelAndSetView(MerchantInfo merchantInfo) {
        PropertyModel propertyModel =
                MerchantTrustMessageViewModel.create(
                        sActivity, merchantInfo, "fake_url", mMockActionHandler);
        PropertyModelChangeProcessor.create(
                propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
        mMessageBannerContent = getMessageBannerMainContent();
        ThreadUtils.runOnUiThreadBlocking(
                () -> sActivity.setContentView(mMessageBannerContent, mParams));
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderMessage_UseRatingBar() throws IOException {
        setUseRatingBarParam("true");

        createModelAndSetView(mMerchantInfo);
        mRenderTestRule.render(mMessageBannerContent, "merchant_trust_message_use_rating_bar");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderMessage_NotUseRatingBar() throws IOException {
        setUseRatingBarParam("false");

        createModelAndSetView(mMerchantInfo);
        mRenderTestRule.render(mMessageBannerContent, "merchant_trust_message_not_use_rating_bar");
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
        mRenderTestRule.render(mMessageBannerContent, "merchant_trust_message_no_rating_reviews");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderMessage_IntegerRatingValue() throws IOException {
        setUseRatingBarParam("true");

        MerchantInfo merchantInfo =
                new MerchantInfo(4f, 1640, new GURL("http://dummy/url"), false, 0f, false, false);
        createModelAndSetView(merchantInfo);
        mRenderTestRule.render(
                mMessageBannerContent, "merchant_trust_message_integer_rating_value");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderMessage_Alternative1() throws IOException {
        setMessageUiParams("true", "false", "1", "1");

        createModelAndSetView(mMerchantInfo);
        mRenderTestRule.render(mMessageBannerContent, "merchant_trust_message_alternative1");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderMessage_Alternative2() throws IOException {
        setMessageUiParams("true", "true", "0", "0");

        createModelAndSetView(mMerchantInfo);
        mRenderTestRule.render(mMessageBannerContent, "merchant_trust_message_alternative2");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderMessage_Alternative3() throws IOException {
        setMessageUiParams("true", "false", "1", "2");

        createModelAndSetView(mMerchantInfo);
        mRenderTestRule.render(mMessageBannerContent, "merchant_trust_message_alternative3");
    }

    private void setUseRatingBarParam(String useRatingBar) {
        setMessageUiParams(useRatingBar, "false", "0", "1");
    }

    private void setMessageUiParams(
            String useRatingBar, String useGoogleIcon, String titleUi, String descriptionUi) {
        // TODO: Remove use of setDisableNativeForTesting(), probably needed due to isInitialized()
        // in MerchantViewerConfig.
        FeatureList.setDisableNativeForTesting(true);
        FeatureOverrides.newBuilder()
                .param(
                        ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                        MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_USE_RATING_BAR_PARAM,
                        useRatingBar)
                .param(
                        ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                        MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_USE_GOOGLE_ICON_PARAM,
                        useGoogleIcon)
                .param(
                        ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                        MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_TITLE_UI_PARAM,
                        titleUi)
                .param(
                        ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                        MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_DESCRIPTION_UI_PARAM,
                        descriptionUi)
                .apply();
    }

    private View getMessageBannerMainContent() {
        View mainContent = mMessageBannerView.getMainContentForTesting();
        ((ViewGroup) mainContent.getParent()).removeView(mainContent);
        return mainContent;
    }
}
