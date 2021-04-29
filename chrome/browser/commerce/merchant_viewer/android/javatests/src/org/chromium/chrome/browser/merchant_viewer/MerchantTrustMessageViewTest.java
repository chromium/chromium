// Copyright 2021 The Chromium Authors. All rights reserved.
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

import org.chromium.base.Callback;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.merchant_viewer.proto.MerchantTrustSignalsOuterClass.MerchantTrustSignals;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.messages.MessageBannerView;
import org.chromium.components.messages.MessageBannerViewBinder;
import org.chromium.components.messages.R;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;
import java.util.List;

/**
 * Tests for MerchantTrustMessageView.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public class MerchantTrustMessageViewTest extends DummyUiActivityTestCase {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    public MerchantTrustMessageViewTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForDummyUiActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Mock
    private Callback<Integer> mMockOnDismissed;

    @Mock
    private Callback<MerchantTrustSignals> mMockOnPrimaryAction;

    private Activity mActivity;
    private MessageBannerView mMessageBannerView;
    private MerchantTrustSignals mMerchantTrustSignals;
    private LayoutParams mParams;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        mActivity = getActivity();
        mMessageBannerView = (MessageBannerView) LayoutInflater.from(mActivity).inflate(
                R.layout.message_banner_view, null, false);
        mMerchantTrustSignals = MerchantTrustSignals.newBuilder()
                                        .setMerchantStarRating(3.51234f)
                                        .setMerchantCountRating(1640)
                                        .setMerchantDetailsPageUrl("http://dummy/url")
                                        .build();
        mParams = new LayoutParams(LayoutParams.MATCH_PARENT,
                mActivity.getResources().getDimensionPixelSize(R.dimen.message_banner_height));
    }

    @Override
    public void tearDownTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { NightModeTestUtils.tearDownNightModeForDummyUiActivity(); });
        super.tearDownTest();
    }

    private void createModelAndSetView() {
        PropertyModel propertyModel = MerchantTrustMessageViewModel.create(
                mActivity, mMerchantTrustSignals, mMockOnDismissed, mMockOnPrimaryAction);
        PropertyModelChangeProcessor.create(
                propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.setContentView(mMessageBannerView, mParams); });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderMessage_UseRatingBar() throws IOException {
        MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_USE_RATING_BAR.setForTesting(true);
        createModelAndSetView();
        mRenderTestRule.render(mMessageBannerView, "merchant_trust_message_use_rating_bar");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderMessage_NotUseRatingBar() throws IOException {
        MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_USE_RATING_BAR.setForTesting(false);
        createModelAndSetView();
        mRenderTestRule.render(mMessageBannerView, "merchant_trust_message_not_use_rating_bar");
    }
}