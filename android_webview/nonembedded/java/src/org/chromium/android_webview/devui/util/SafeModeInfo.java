// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui.util;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.RemoteException;

import org.chromium.android_webview.common.SafeModeController;
import org.chromium.android_webview.common.services.ISafeModeService;
import org.chromium.android_webview.common.services.ServiceHelper;
import org.chromium.android_webview.services.SafeModeService;
import org.chromium.base.Log;

import java.util.Set;
import java.util.function.LongConsumer;

/** Expose information about SafeMode status needed for the UI. */
public class SafeModeInfo {
    private static final String TAG = "WebViewDevTools";
    private final Context mContext;
    private final String mWebViewPackageName;

    public SafeModeInfo(Context context, String webViewPackageName) {
        mContext = context;
        mWebViewPackageName = webViewPackageName;
    }

    public boolean isEnabledForUI() {
        return SafeModeController.getInstance().isSafeModeEnabled(mWebViewPackageName);
    }

    public Set<String> getActionsForUI() {
        return SafeModeController.getInstance().queryActions(mWebViewPackageName);
    }

    public void getActivationTimeForUI(LongConsumer callback) {
        ServiceConnection connection =
                new ServiceConnection() {
                    @Override
                    public void onServiceConnected(ComponentName className, IBinder service) {
                        // This is called when the connection with the service is established.
                        ISafeModeService mService = ISafeModeService.Stub.asInterface(service);
                        try {
                            long activationTime = mService.getSafeModeActivationTimestamp();
                            callback.accept(activationTime);
                        } catch (RemoteException e) {
                            Log.e(
                                    TAG,
                                    "Failed to get SafeMode Activation Time from SafeModeService",
                                    e);
                        } finally {
                            mContext.unbindService(this);
                        }
                    }

                    @Override
                    public void onServiceDisconnected(ComponentName className) {}
                };

        Intent intent = new Intent();
        intent.setClassName(mWebViewPackageName, SafeModeService.class.getName());
        if (!ServiceHelper.bindService(mContext, intent, connection, Context.BIND_AUTO_CREATE)) {
            Log.w(TAG, "Could not bind to SafeModeService to get SafeMode Activation Time");
        }
    }
}
