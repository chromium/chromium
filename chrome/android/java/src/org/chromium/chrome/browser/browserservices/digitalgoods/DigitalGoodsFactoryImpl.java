// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.mojo.system.MojoException;
import org.chromium.payments.mojom.CreateDigitalGoodsResponseCode;
import org.chromium.payments.mojom.DigitalGoods;
import org.chromium.payments.mojom.DigitalGoodsFactory;

/**
 * An implementation of the mojo {@link DigitalGoodsFactory} interface.
 */
public class DigitalGoodsFactoryImpl implements DigitalGoodsFactory {
    private static DigitalGoods sImplForTesting;

    private final RenderFrameHost mRenderFrameHost;
    private final DigitalGoodsImpl.Delegate mDigitalGoodsDelegate;
    private final DigitalGoodsAdapter mAdapter;

    @VisibleForTesting
    public static void setDigitalGoodsForTesting(DigitalGoods impl) {
        sImplForTesting = impl;
    }

    public DigitalGoodsFactoryImpl(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
        mDigitalGoodsDelegate = mRenderFrameHost::getLastCommittedURL;
        mAdapter = new DigitalGoodsAdapter(
                ChromeApplicationImpl.getComponent().resolveTrustedWebActivityClient());
    }

    private int getResponseCode(String paymentMethod) {
        return CreateDigitalGoodsResponseCode.UNSUPPORTED_CONTEXT;
    }

    @Override
    public void createDigitalGoods(String paymentMethod, CreateDigitalGoods_Response callback) {
        if (sImplForTesting != null) {
            callback.call(CreateDigitalGoodsResponseCode.OK, sImplForTesting);
            return;
        }

        // If the user is making Digital Goods payments, this is a good hint that we should enable
        // site isolation for the site.
        SiteIsolator.startIsolatingSite(mDigitalGoodsDelegate.getUrl());

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
