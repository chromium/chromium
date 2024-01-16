// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import android.net.Uri;
import android.os.Bundle;
import android.os.RemoteException;

import androidx.browser.trusted.TrustedWebActivityCallback;

import org.chromium.base.Log;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClientWrappers;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.payments.mojom.DigitalGoods.Consume_Response;
import org.chromium.payments.mojom.DigitalGoods.GetDetails_Response;
import org.chromium.payments.mojom.DigitalGoods.ListPurchaseHistory_Response;
import org.chromium.payments.mojom.DigitalGoods.ListPurchases_Response;

/**
 * This class uses the {@link DigitalGoodsConverter} to convert data types between mojo types and
 * Android types and then uses the {@link TrustedWebActivityClient} to call into the Trusted Web
 * Activity Client.
 */
public class DigitalGoodsAdapter {
    private static final String TAG = "DigitalGoods";

    public static final String COMMAND_CONSUME = "consume";
    public static final String COMMAND_GET_DETAILS = "getDetails";
    public static final String COMMAND_LIST_PURCHASES = "listPurchases";
    public static final String COMMAND_LIST_PURCHASE_HISTORY = "listPurchaseHistory";
    public static final String KEY_SUCCESS = "success";

    // Legacy commands kept around for backwards compatibility.
    public static final String COMMAND_ACKNOWLEDGE = "acknowledge";

    private final TrustedWebActivityClient mClient;

    public DigitalGoodsAdapter(TrustedWebActivityClient client) {
        mClient = client;
    }

    public void getDetails(Uri scope, String[] itemIds, GetDetails_Response response) {
        Bundle args = GetDetailsConverter.convertParams(itemIds);
        TrustedWebActivityCallback callback = GetDetailsConverter.convertCallback(response);
        Runnable onError = () -> GetDetailsConverter.returnClientAppError(response);
        Runnable onUnavailable = () -> GetDetailsConverter.returnClientAppUnavailable(response);

        execute(scope, COMMAND_GET_DETAILS, args, callback, onError, onUnavailable);
    }

    public void listPurchases(Uri scope, ListPurchases_Response response) {
        Bundle args = new Bundle();
        TrustedWebActivityCallback callback = ListPurchasesConverter.convertCallback(response);
        Runnable onError = () -> ListPurchasesConverter.returnClientAppError(response);
        Runnable onUnavailable = () -> ListPurchasesConverter.returnClientAppUnavailable(response);

        execute(scope, COMMAND_LIST_PURCHASES, args, callback, onError, onUnavailable);
    }

    public void listPurchaseHistory(Uri scope, ListPurchaseHistory_Response response) {
        Bundle args = new Bundle();
        TrustedWebActivityCallback callback =
                ListPurchaseHistoryConverter.convertCallback(response);
        Runnable onError = () -> ListPurchaseHistoryConverter.returnClientAppError(response);
        Runnable onUnavailable =
                () -> ListPurchaseHistoryConverter.returnClientAppUnavailable(response);

        execute(scope, COMMAND_LIST_PURCHASE_HISTORY, args, callback, onError, onUnavailable);
    }

    public void consume(Uri scope, String purchaseToken, Consume_Response response) {
        Bundle args = ConsumeConverter.convertParams(purchaseToken);
        TrustedWebActivityCallback callback = ConsumeConverter.convertCallback(response);
        Runnable onUnavailable = () -> ConsumeConverter.returnClientAppUnavailable(response);
        Runnable onError = () -> ConsumeConverter.returnClientAppError(response);

        // If Consume fails, try to call acknowledge(..., makeAvailableAgain = true) which will
        // achieve the same effect on older clients.
        Runnable tryAcknowledgeOnError =
                () -> {
                    Bundle ackArgs = AcknowledgeConverter.convertParams(purchaseToken);
                    TrustedWebActivityCallback ackCallback =
                            AcknowledgeConverter.convertCallback(response);

                    execute(
                            scope,
                            COMMAND_ACKNOWLEDGE,
                            ackArgs,
                            ackCallback,
                            onError,
                            onUnavailable);
                };

        execute(scope, COMMAND_CONSUME, args, callback, tryAcknowledgeOnError, onUnavailable);
    }

    private void execute(
            Uri scope,
            String command,
            Bundle args,
            TrustedWebActivityCallback callback,
            Runnable onClientAppError,
            Runnable onClientAppUnavailable) {
        mClient.connectAndExecute(
                scope,
                new TrustedWebActivityClient.ExecutionCallback() {
                    @Override
                    public void onConnected(
                            Origin origin, TrustedWebActivityClientWrappers.Connection service)
                            throws RemoteException {
                        // Wrap this call so that crashes in the TWA client don't cause crashes in
                        // Chrome.
                        Bundle result = null;
                        try {
                            result = service.sendExtraCommand(command, args, callback);
                        } catch (Exception e) {
                            Log.w(TAG, "Exception communicating with client.");
                            onClientAppError.run();
                        }

                        boolean success = result != null && result.getBoolean(KEY_SUCCESS, false);
                        if (!success) {
                            onClientAppError.run();
                        }
                    }

                    @Override
                    public void onNoTwaFound() {
                        Log.w(TAG, "Unable to execute " + command + ".");
                        onClientAppUnavailable.run();
                    }
                });
    }
}
