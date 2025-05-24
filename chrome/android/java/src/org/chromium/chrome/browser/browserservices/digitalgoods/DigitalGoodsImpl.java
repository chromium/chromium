// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import android.net.Uri;

import androidx.annotation.Nullable;

import org.chromium.mojo.system.MojoException;
import org.chromium.payments.mojom.DigitalGoods;
import org.chromium.payments.mojom.DigitalGoods.GetDetails_Response;
import org.chromium.payments.mojom.DigitalGoods.ListPurchases_Response;
import org.chromium.url.GURL;

/**
 * An implementation of the {@link DigitalGoods} mojo interface that communicates with Trusted Web
 * Activity clients to call Billing APIs.
 */
public class DigitalGoodsImpl implements DigitalGoods {
    private final Delegate mDelegate;

    /** A Delegate that provides the current URL. */
    public interface Delegate {
        /** @return The current URL or null when the frame is being destroyed. */
        @Nullable
        GURL getUrl();
    }

    /** Constructs the object with a given adapter and delegate. */
    public DigitalGoodsImpl(Delegate delegate) {
        mDelegate = delegate;
    }

    @Override
    public void getDetails(String[] itemIds, GetDetails_Response callback) {
        GURL url = mDelegate.getUrl();
        if (url != null)
            DigitalGoodsAdapter.getDetails(Uri.parse(url.getSpec()), itemIds, callback);
    }

    @Override
    public void listPurchases(ListPurchases_Response callback) {
        GURL url = mDelegate.getUrl();
        if (url != null) DigitalGoodsAdapter.listPurchases(Uri.parse(url.getSpec()), callback);
    }

    @Override
    public void listPurchaseHistory(ListPurchaseHistory_Response callback) {
        GURL url = mDelegate.getUrl();
        if (url != null)
            DigitalGoodsAdapter.listPurchaseHistory(Uri.parse(url.getSpec()), callback);
    }

    @Override
    public void consume(String purchaseToken, Consume_Response callback) {
        GURL url = mDelegate.getUrl();
        if (url != null)
            DigitalGoodsAdapter.consume(Uri.parse(url.getSpec()), purchaseToken, callback);
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {}
}
