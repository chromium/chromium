// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import android.net.Uri;
import android.os.Bundle;

import androidx.browser.trusted.TrustedWebActivityCallback;
import androidx.browser.trusted.TrustedWebActivityServiceConnection;

import org.chromium.base.Log;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.payments.mojom.DigitalGoods.AcknowledgeResponse;
import org.chromium.payments.mojom.DigitalGoods.GetDetailsResponse;
import org.chromium.payments.mojom.DigitalGoods.ListPurchasesResponse;

/**
 * This class uses the {@link DigitalGoodsConverter} to convert data types between mojo types and
 * Android types and then uses the {@link TrustedWebActivityClient} to call into the Trusted Web
 * Activity Client.
 */
public class DigitalGoodsAdapter {
    private static final String TAG = "DigitalGoods";

    public static final String COMMAND_ACKNOWLEDGE = "acknowledge";
    public static final String COMMAND_GET_DETAILS = "getDetails";
    public static final String COMMAND_LIST_PURCHASES = "listPurchases";
    public static final String KEY_SUCCESS = "success";

    private final TrustedWebActivityClient mClient;

    public DigitalGoodsAdapter(TrustedWebActivityClient client) {
        mClient = client;
    }

    public void getDetails(Uri scope, String[] itemIds, GetDetailsResponse response) {
        Bundle args = GetDetailsConverter.convertParams(itemIds);
        TrustedWebActivityCallback callback = GetDetailsConverter.convertCallback(response);
        Runnable onError = () -> GetDetailsConverter.returnClientAppError(response);
        Runnable onUnavailable = () -> GetDetailsConverter.returnClientAppUnavailable(response);

        execute(scope, COMMAND_GET_DETAILS, args, callback, onError, onUnavailable);
    }

    public void acknowledge(Uri scope, String purchaseToken, boolean makeAvailableAgain,
            AcknowledgeResponse response) {
        Bundle args = AcknowledgeConverter.convertParams(purchaseToken, makeAvailableAgain);
        TrustedWebActivityCallback callback = AcknowledgeConverter.convertCallback(response);
        Runnable onError = () -> AcknowledgeConverter.returnClientAppError(response);
        Runnable onUnavailable = () -> AcknowledgeConverter.returnClientAppUnavailable(response);

        execute(scope, COMMAND_ACKNOWLEDGE, args, callback, onError, onUnavailable);
    }

    public void listPurchases(Uri scope, ListPurchasesResponse response) {
        Bundle args = new Bundle();
        TrustedWebActivityCallback callback = ListPurchasesConverter.convertCallback(response);
        Runnable onError = () -> ListPurchasesConverter.returnClientAppError(response);
        Runnable onUnavailable = () -> ListPurchasesConverter.returnClientAppUnavailable(response);

        execute(scope, COMMAND_LIST_PURCHASES, args, callback, onError, onUnavailable);
    }

    private void execute(Uri scope, String command, Bundle args,
            TrustedWebActivityCallback callback, Runnable onClientAppError,
            Runnable onClientAppUnavailable) {
        mClient.connectAndExecute(scope, new TrustedWebActivityClient.ExecutionCallback() {
            @Override
            public void onConnected(Origin origin, TrustedWebActivityServiceConnection service) {
                // Wrap this call so that crashes in the TWA client don't cause crashes in Chrome.
                Bundle result = null;
                try {
                    result = service.sendExtraCommand(command, args, callback);
                } catch (Exception e) {
                    Log.w(TAG, "Exception communicating with client.");
                    onClientAppError.run();
                }

                boolean success = result != null &&
                        result.getBoolean(KEY_SUCCESS, false);
                if (!success) {
                    onClientAppError.run();
                }
            }

            @Override
            public void onNoTwaFound() {
                onClientAppUnavailable.run();
            }
        });
    }
}
