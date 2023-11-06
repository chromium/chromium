// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;

import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.PLAY_BILLING_OK;

import android.os.Bundle;
import android.os.RemoteException;

import androidx.browser.trusted.TrustedWebActivityCallback;

import org.mockito.Mockito;

import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClientWrappers;
import org.chromium.components.embedder_support.util.Origin;

/**
 * Helper class for {@link DigitalGoodsUnitTest} - it pretends to be a TrustedWebActivityClient
 * talking to the Digital Goods handler in the TWA shell, providing hardcoded responses.
 */
public class MockTrustedWebActivityClient {
    private TrustedWebActivityClientWrappers.Connection mConnection;
    private TrustedWebActivityClient mClient;
    private Runnable mCallback;
    private int mVersion = 2;

    private static final Bundle ITEM1 =
            GetDetailsConverter.createItemDetailsBundle(
                    "id1", "title 1", "description 1", "GBP", "20");
    private static final Bundle ITEM2 =
            GetDetailsConverter.createItemDetailsBundle(
                    "id2",
                    "title 2",
                    "description 2",
                    "GBP",
                    "30",
                    "subs",
                    "https://www.example.com/image.png",
                    "2 weeks",
                    "month",
                    "GBP",
                    "20",
                    "week",
                    2);

    private static final Bundle PURCHASE1 =
            ListPurchasesConverter.createPurchaseReferenceBundle("id3", "abc");
    private static final Bundle PURCHASE2 =
            ListPurchasesConverter.createPurchaseReferenceBundle("id4", "def");

    public MockTrustedWebActivityClient() {
        mConnection = Mockito.mock(TrustedWebActivityClientWrappers.Connection.class);
        mClient = Mockito.mock(TrustedWebActivityClient.class);

        doAnswer(
                        invocation -> {
                            TrustedWebActivityClient.ExecutionCallback callback =
                                    invocation.getArgument(1);

                            callback.onConnected(Mockito.mock(Origin.class), mConnection);

                            return null;
                        })
                .when(mClient)
                .connectAndExecute(any(), any());

        try {
            doAnswer(
                            invocation -> {
                                String command = invocation.getArgument(0);
                                Bundle args = invocation.getArgument(1);
                                TrustedWebActivityCallback callback = invocation.getArgument(2);

                                return handleWrapper(command, args, callback);
                            })
                    .when(mConnection)
                    .sendExtraCommand(any(), any(), any());
        } catch (RemoteException e) {
            // This ugliness is because we're mocking a method that could throw an exception.
            throw new RuntimeException(e);
        }
    }

    /** Returns a mocked {@link TrustedWebActivityClient} to be used in tests. */
    public TrustedWebActivityClient getClient() {
        return mClient;
    }

    /** Runs the pending callback. */
    public void runCallback() {
        mCallback.run();
        mCallback = null;
    }

    /** Changes the clients behaviour to simulate older TWA shells. */
    public void setVersion(int version) {
        mVersion = version;
    }

    private boolean handle(String command, Bundle args, TrustedWebActivityCallback callback) {
        // At this point, pretend we're in the TWA shell.
        switch (command) {
            case "getDetails":
                mCallback =
                        () -> {
                            callback.onExtraCallback(
                                    "getDetails.response",
                                    GetDetailsConverter.createResponseBundle(
                                            PLAY_BILLING_OK, ITEM1, ITEM2));
                        };
                return true;
            case "listPurchases":
                mCallback =
                        () -> {
                            callback.onExtraCallback(
                                    "listPurchases.response",
                                    ListPurchasesConverter.createResponseBundle(
                                            PLAY_BILLING_OK, PURCHASE1, PURCHASE2));
                        };
                return true;
            case "listPurchaseHistory":
                // Version 1 clients did not have purchase history.
                if (mVersion == 1) return false;
                mCallback =
                        () -> {
                            // Swap the order of PURCHASE1 and PURCHASE2 as an easy way to make sure
                            // the correct branch of this switch statement is executed.
                            callback.onExtraCallback(
                                    "listPurchaseHistory.response",
                                    ListPurchaseHistoryConverter.createResponseBundle(
                                            PLAY_BILLING_OK, PURCHASE2, PURCHASE1));
                        };
                return true;
            case "consume":
                // Version 1 clients did not have consume.
                if (mVersion == 1) return false;
                mCallback =
                        () -> {
                            callback.onExtraCallback(
                                    "consume.response",
                                    ConsumeConverter.createResponseBundle(PLAY_BILLING_OK));
                        };
                return true;
            case "acknowledge":
                // Only version 1 clients have acknowledge.
                if (mVersion != 1) return false;
                mCallback =
                        () -> {
                            callback.onExtraCallback(
                                    "acknowledge.response",
                                    AcknowledgeConverter.createResponseBundle(PLAY_BILLING_OK));
                        };
                return true;
        }

        return false;
    }

    private Bundle handleWrapper(String command, Bundle args, TrustedWebActivityCallback callback) {
        Bundle bundle = new Bundle();
        bundle.putBoolean("success", handle(command, args, callback));
        return bundle;
    }
}
