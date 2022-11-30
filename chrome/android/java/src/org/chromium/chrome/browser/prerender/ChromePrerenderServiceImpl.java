// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prerender;

import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;

import org.chromium.base.ContextUtils;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.externalauth.VerifiedHandler;
import org.chromium.components.version_info.VersionInfo;

/**
 * A bound service that does nothing. Kept here to prevent old clients relying on it being
 * available from crashing.
 */
public class ChromePrerenderServiceImpl extends ChromePrerenderService.Impl {
    /**
     * Handler of incoming messages from clients.
     */
    static class IncomingHandler extends VerifiedHandler {
        IncomingHandler(Context context) {
            super(context, ExternalAuthUtils.getInstance(),
                    VersionInfo.isLocalBuild() ? 0
                                               : ExternalAuthUtils.FLAG_SHOULD_BE_GOOGLE_SIGNED);
        }

        @Override
        public void handleMessage(Message msg) {}
    }

    private Messenger mMessenger;

    @Override
    public IBinder onBind(Intent intent) {
        mMessenger = new Messenger(new IncomingHandler(ContextUtils.getApplicationContext()));
        return mMessenger.getBinder();
    }
}
