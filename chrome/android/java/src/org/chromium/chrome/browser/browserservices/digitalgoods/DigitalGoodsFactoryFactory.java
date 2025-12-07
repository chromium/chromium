// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.payments.mojom.DigitalGoodsFactory;
import org.chromium.services.service_manager.InterfaceFactory;

/** A factory to produce instances of the mojo {@link DigitalGoodsFactory} interface. */
@NullMarked
public class DigitalGoodsFactoryFactory implements InterfaceFactory<@Nullable DigitalGoodsFactory> {
    private final RenderFrameHost mRenderFrameHost;

    public DigitalGoodsFactoryFactory(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
    }

    @Override
    public DigitalGoodsFactory createImpl() {
        return new DigitalGoodsFactoryImpl(mRenderFrameHost);
    }
}
