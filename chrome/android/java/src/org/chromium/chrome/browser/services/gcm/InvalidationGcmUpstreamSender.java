// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services.gcm;

import android.accounts.Account;
import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Bundle;
import android.os.Parcel;
import android.support.annotation.MainThread;
import android.util.Log;

import com.google.android.gms.gcm.GoogleCloudMessaging;
import com.google.ipc.invalidation.ticl.android2.channel.GcmUpstreamSenderService;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.init.ProcessInitializationHandler;
import org.chromium.chrome.browser.signin.OAuth2TokenService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.sync.SyncConstants;

import java.io.IOException;
import java.util.UUID;

import javax.annotation.Nullable;

/**
 * Sends Upstream messages for Invalidations using GCM.
 */
public class InvalidationGcmUpstreamSender extends GcmUpstreamSenderService {
    private static final String TAG = "InvalidationGcmUpstream";

    // GCM Payload Limit in bytes.
    private static final int GCM_PAYLOAD_LIMIT = 4000;

    @Override
    public void deliverMessage(final String to, final Bundle data) {
        final Bundle dataToSend = createDeepCopy(data);

        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                doDeliverMessage(ContextUtils.getApplicationContext(), to, dataToSend);
            }
        });
    }

    // Incorrectly infers that this is called on a worker thread because of AsyncTask doInBackground
    // overriding.
    @SuppressWarnings("WrongThread")
    @MainThread
    private void doDeliverMessage(
            final Context applicationContext, final String to, final Bundle data) {
        ThreadUtils.assertOnUiThread();
        ProcessInitializationHandler.getInstance().initializePreNative();

        @Nullable
        Account account = ChromeSigninController.get().getSignedInUser();
        if (account == null) {
            // This should never happen, because this code should only be run if a user is
            // signed-in.
            Log.w(TAG, "No signed-in user; cannot send message to data center");
            return;
        }

        // Attempt to retrieve a token for the user.
        OAuth2TokenService.getOAuth2AccessToken(this, account,
                SyncConstants.CHROME_SYNC_OAUTH2_SCOPE,
                new AccountManagerFacade.GetAuthTokenCallback() {
                    @Override
                    public void tokenAvailable(final String token) {
                        new AsyncTask<Void>() {
                            @Override
                            protected Void doInBackground() {
                                sendUpstreamMessage(to, data, token, applicationContext);
                                return null;
                            }
                        }
                                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
                    }

                    @Override
                    public void tokenUnavailable(boolean isTransientError) {
                        GcmUma.recordGcmUpstreamHistogram(ContextUtils.getApplicationContext(),
                                GcmUma.UMA_UPSTREAM_TOKEN_REQUEST_FAILED);
                    }
                });
    }

    /*
     * This function runs on a thread from the AsyncTask.THREAD_POOL_EXECUTOR.
     */
    private void sendUpstreamMessage(String to, Bundle data, String token, Context context) {
        // Add the OAuth2 token to the bundle. The token should have the prefix Bearer added to it.
        data.putString("Authorization", "Bearer " + token);
        if (!isMessageWithinLimit(data)) {
            GcmUma.recordGcmUpstreamHistogram(context, GcmUma.UMA_UPSTREAM_SIZE_LIMIT_EXCEEDED);
            return;
        }
        String msgId = UUID.randomUUID().toString();
        try {
            GoogleCloudMessaging.getInstance(ContextUtils.getApplicationContext())
                    .send(to, msgId, 1, data);
        } catch (IOException | IllegalArgumentException exception) {
            Log.w(TAG, "Send message failed");
            GcmUma.recordGcmUpstreamHistogram(context, GcmUma.UMA_UPSTREAM_SEND_FAILED);
        }
    }

    private boolean isMessageWithinLimit(Bundle data) {
        int size = 0;
        for (String key : data.keySet()) {
            size += key.length() + data.getString(key).length();
        }
        if (size > GCM_PAYLOAD_LIMIT) {
            return false;
        }
        return true;
    }

    /*
     * Creates and returns a deep copy of the original Bundle.
     */
    // TODO(crbug.com/635567): Fix this properly.
    @SuppressLint("ParcelClassLoader")
    private Bundle createDeepCopy(Bundle original) {
        Parcel temp = Parcel.obtain();
        original.writeToParcel(temp, 0);
        temp.setDataPosition(0);
        Bundle copy = temp.readBundle();
        temp.recycle();
        return copy;
    }
}
