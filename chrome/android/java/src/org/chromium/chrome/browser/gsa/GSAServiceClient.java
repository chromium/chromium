// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gsa;

import android.annotation.SuppressLint;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.os.StrictMode;
import android.util.Log;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.AppHooks;

/**
 * A simple client that connects and talks to the GSAService using Messages.
 */
public class GSAServiceClient {
    private static final String TAG = "GSAServiceClient";

    /**
     * Constants for gsa communication. These should not change without corresponding changes on the
     * service side in GSA.
     */
    private static final String GSA_SERVICE = "com.google.android.ssb.action.SSB_SERVICE";
    public static final int REQUEST_REGISTER_CLIENT = 2;
    public static final int RESPONSE_UPDATE_SSB = 3;

    public static final String KEY_GSA_STATE = "ssb_service:ssb_state";
    public static final String KEY_GSA_CONTEXT = "ssb_service:ssb_context";
    public static final String KEY_GSA_PACKAGE_NAME = "ssb_service:ssb_package_name";
    public static final String KEY_GSA_SUPPORTS_BROADCAST =
            "ssb_service:chrome_holds_account_update_permission";

    static final String ACCOUNT_CHANGE_HISTOGRAM = "Search.GsaAccountChangeNotificationSource";
    // For the histogram above. Append-only.
    static final int ACCOUNT_CHANGE_SOURCE_SERVICE = 0;
    static final int ACCOUNT_CHANGE_SOURCE_BROADCAST = 1;
    static final int ACCOUNT_CHANGE_SOURCE_COUNT = 2;

    /** Messenger to handle incoming messages from the service */
    private final Messenger mMessenger;
    private final IncomingHandler mHandler;
    private final GSAServiceConnection mConnection;
    private final GSAHelper mGsaHelper;
    private Context mContext;
    private Callback<Bundle> mOnMessageReceived;

    /** Messenger for communicating with service. */
    private Messenger mService;

    /**
     * Handler of incoming messages from service.
     */
    @SuppressLint("HandlerLeak")
    private class IncomingHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
            if (msg.what != RESPONSE_UPDATE_SSB) {
                super.handleMessage(msg);
                return;
            }

            if (mService == null) return;
            final Bundle bundle = (Bundle) msg.obj;
            String account = mGsaHelper.getGSAAccountFromState(bundle.getByteArray(KEY_GSA_STATE));
            RecordHistogram.recordEnumeratedHistogram(ACCOUNT_CHANGE_HISTOGRAM,
                    ACCOUNT_CHANGE_SOURCE_SERVICE, ACCOUNT_CHANGE_SOURCE_COUNT);
            GSAState.getInstance(mContext).setGsaAccount(account);
            if (mOnMessageReceived != null) mOnMessageReceived.onResult(bundle);
        }
    }

    /**
     * Constructs an instance of this class.
     *
     * @param context Appliation context.
     * @param onMessageReceived optional callback when a message is received.
     */
    GSAServiceClient(Context context, Callback<Bundle> onMessageReceived) {
        mContext = context.getApplicationContext();
        mOnMessageReceived = onMessageReceived;
        mHandler = new IncomingHandler();
        mMessenger = new Messenger(mHandler);
        mConnection = new GSAServiceConnection();
        mGsaHelper = AppHooks.get().createGsaHelper();
    }

    /**
     * Establishes a connection to the service. Call this method once the callback passed here is
     * ready to handle calls.
     * If you pass in an GSA context, it will be sent up the service as soon as the connection is
     * established.
     * @return Whether or not the connection to the service was established successfully.
     */
    boolean connect() {
        if (mService != null) Log.e(TAG, "Already connected.");
        Intent intent = new Intent(GSA_SERVICE).setPackage(GSAState.SEARCH_INTENT_PACKAGE);

        // Third-party modifications to the framework lead to StrictMode violations in
        // Context#bindService(). See crbug.com/670195.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            return mContext.bindService(
                    intent, mConnection, Context.BIND_AUTO_CREATE | Context.BIND_NOT_FOREGROUND);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /**
     * Disconnects from the service and resets the client's state.
     */
    void disconnect() {
        if (mService == null) return;
        mContext.unbindService(mConnection);
        mService = null;

        // Remove pending handler actions to prevent memory leaks.
        mHandler.removeCallbacksAndMessages(null);
    }

    /**
     * Indicates whether or not the client is currently connected to the service.
     * @return true if connected, false otherwise.
     */
    boolean isConnected() {
        return mService != null;
    }

    private class GSAServiceConnection implements ServiceConnection {
        private static final String SERVICE_CONNECTION_TAG = "GSAServiceConnection";

        @Override
        public void onServiceDisconnected(ComponentName name) {
            mService = null;
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            // Ignore this call if we disconnected in the meantime.
            if (mContext == null) return;

            mService = new Messenger(service);
            try {
                Message registerClientMessage = Message.obtain(
                        null, REQUEST_REGISTER_CLIENT);
                registerClientMessage.replyTo = mMessenger;
                Bundle b = mGsaHelper.getBundleForRegisteringGSAClient(mContext);
                if (b == null) b = new Bundle();
                b.putString(KEY_GSA_PACKAGE_NAME, mContext.getPackageName());
                b.putBoolean(KEY_GSA_SUPPORTS_BROADCAST,
                        GSAAccountChangeListener.holdsAccountUpdatePermission());
                registerClientMessage.setData(b);
                mService.send(registerClientMessage);
                // Send prepare overlay message if there is a pending GSA context.
            } catch (RemoteException e) {
                Log.w(SERVICE_CONNECTION_TAG, "GSAServiceConnection - remote call failed", e);
            }
        }
    }
}
