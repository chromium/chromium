// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import android.content.Context;
import android.os.Bundle;

import androidx.browser.customtabs.PostMessageBackend;

/**
 * A {@link PostMessageBackend} which delegates incoming notifications to the {@link
 * ActivityDelegate} from the dynamic module.
 *
 * <p>If the dynamic module is not loaded, we ignore incoming notifications and always return
 * false.
 */
public class ActivityDelegatePostMessageBackend implements PostMessageBackend {
    private final ActivityDelegate mActivityDelegate;

    ActivityDelegatePostMessageBackend(ActivityDelegate activityDelegate) {
        mActivityDelegate = activityDelegate;
    }

    @Override
    public boolean onPostMessage(String message, Bundle extras) {
        if (mActivityDelegate != null) {
            mActivityDelegate.onPostMessage(message);
            return true;
        }
        return false;
    }

    @Override
    public boolean onNotifyMessageChannelReady(Bundle extras) {
        if (mActivityDelegate != null) {
            mActivityDelegate.onMessageChannelReady();
            return true;
        }
        return false;
    }

    @Override
    public void onDisconnectChannel(Context appContext) {
        // Nothing to do.
    }
}