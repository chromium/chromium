// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.content.Context;
import android.os.Bundle;

import com.google.android.gms.gcm.GcmListenerService;

/**
 * GcmListenerService base class which will call through to the given {@link Impl}. This class must
 * be present in the base module, while the Impl can be in the chrome module.
 */
public class SplitCompatGcmListenerService extends GcmListenerService {
    private String mServiceClassName;
    private Impl mImpl;

    public SplitCompatGcmListenerService(String serviceClassName) {
        mServiceClassName = serviceClassName;
    }

    @Override
    protected void attachBaseContext(Context context) {
        context = SplitCompatUtils.createChromeContext(context);
        mImpl = (Impl) SplitCompatUtils.newInstance(context, mServiceClassName);
        mImpl.setService(this);
        super.attachBaseContext(context);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        mImpl.onCreate();
    }

    @Override
    public void onMessageReceived(String from, Bundle data) {
        mImpl.onMessageReceived(from, data);
    }

    @Override
    public void onMessageSent(String msgId) {
        mImpl.onMessageSent(msgId);
    }

    @Override
    public void onSendError(String msgId, String error) {
        mImpl.onSendError(msgId, error);
    }

    @Override
    public void onDeletedMessages() {
        mImpl.onDeletedMessages();
    }

    /**
     * Holds the implementation of service logic. Will be called by {@link
     * SplitCompatGcmListenerService}.
     */
    public abstract static class Impl {
        private SplitCompatGcmListenerService mService;

        protected final void setService(SplitCompatGcmListenerService service) {
            mService = service;
        }

        protected final SplitCompatGcmListenerService getService() {
            return mService;
        }

        public void onCreate() {}

        public void onMessageReceived(String from, Bundle data) {}

        public void onMessageSent(String msgId) {}

        public void onSendError(String msgId, String error) {}

        public void onDeletedMessages() {}
    }
}
