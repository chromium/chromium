// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import android.app.Activity;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.payments.MethodStrings;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.mojo.system.MojoException;
import org.chromium.payments.mojom.CreateDigitalGoodsResponseCode;
import org.chromium.payments.mojom.DigitalGoods;
import org.chromium.payments.mojom.DigitalGoodsFactory;
import org.chromium.payments.mojom.DigitalGoodsFactory.CreateDigitalGoods_Response;

/** An implementation of the mojo {@link DigitalGoodsFactory} interface. */
public class DigitalGoodsFactoryImpl implements DigitalGoodsFactory {
    private static DigitalGoods sImplForTesting;

    private final RenderFrameHost mRenderFrameHost;
    private final DigitalGoodsImpl.Delegate mDigitalGoodsDelegate;
    private final DigitalGoodsAdapter mAdapter;

    public static void setDigitalGoodsForTesting(DigitalGoods impl) {
        sImplForTesting = impl;
        ResettersForTesting.register(() -> sImplForTesting = null);
    }

    public DigitalGoodsFactoryImpl(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
        mDigitalGoodsDelegate = mRenderFrameHost::getLastCommittedURL;
        mAdapter =
                new DigitalGoodsAdapter(
                        ChromeApplicationImpl.getComponent().resolveTrustedWebActivityClient());
    }

    private int getResponseCode(String paymentMethod) {
        if (!PaymentFeatureList.isEnabled(PaymentFeatureList.WEB_PAYMENTS_APP_STORE_BILLING)) {
            return CreateDigitalGoodsResponseCode.UNSUPPORTED_CONTEXT;
        }

        // Ensure that the DigitalGoodsImpl is only created if we're in a TWA and on its verified
        // origin.
        WebContents wc = WebContentsStatics.fromRenderFrameHost(mRenderFrameHost);
        Activity activity = ActivityUtils.getActivityFromWebContents(wc);
        if (!(activity instanceof CustomTabActivity)) {
            return CreateDigitalGoodsResponseCode.UNSUPPORTED_CONTEXT;
        }
        CustomTabActivity cta = (CustomTabActivity) activity;
        if (!cta.isInTwaMode()) {
            return CreateDigitalGoodsResponseCode.UNSUPPORTED_CONTEXT;
        }

        if (!MethodStrings.GOOGLE_PLAY_BILLING.equals(paymentMethod)) {
            return CreateDigitalGoodsResponseCode.UNSUPPORTED_PAYMENT_METHOD;
        }

        // TODO(peconn): Add a test for this.

        return CreateDigitalGoodsResponseCode.OK;
    }

    @Override
    public void createDigitalGoods(String paymentMethod, CreateDigitalGoods_Response callback) {
        if (sImplForTesting != null) {
            callback.call(CreateDigitalGoodsResponseCode.OK, sImplForTesting);
            return;
        }

        // If the user is making Digital Goods payments, this is a good hint that we should enable
        // site isolation for the site.
        WebContents wc = WebContentsStatics.fromRenderFrameHost(mRenderFrameHost);
        SiteIsolator.startIsolatingSite(
                Profile.fromWebContents(wc), mDigitalGoodsDelegate.getUrl());

        int code = getResponseCode(paymentMethod);
        CreateDigitalGoodsResponseCode.validate(code);
        if (code == CreateDigitalGoodsResponseCode.OK) {
            callback.call(code, new DigitalGoodsImpl(mAdapter, mDigitalGoodsDelegate));
        } else {
            callback.call(code, null);
        }
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {}
}
