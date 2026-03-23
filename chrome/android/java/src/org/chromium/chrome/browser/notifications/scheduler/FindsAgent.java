// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.scheduler;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.url.GURL;

/** Used by finds notifications to open URLs through IntentHandler. */
@NullMarked
public class FindsAgent {
    @CalledByNative
    private static void openNotificationUrl(@JniType("GURL") GURL url) {
        assert !GURL.isEmptyOrInvalid(url);
        Context context = ContextUtils.getApplicationContext();
        Intent newIntent =
                IntentHandler.createTrustedOpenNewTabIntent(context, /* incognito= */ false);
        newIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        newIntent.putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        newIntent.setData(Uri.parse(url.getSpec()));
        context.startActivity(newIntent);
    }

    private FindsAgent() {}
}
