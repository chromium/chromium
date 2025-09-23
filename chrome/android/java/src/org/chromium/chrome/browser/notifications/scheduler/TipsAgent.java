// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.scheduler;

import android.content.Context;
import android.content.Intent;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.IntentHandler;

/** Used by tips notifications to schedule and display tips through the Android UI. */
@NullMarked
public class TipsAgent {
    @CalledByNative
    private static void showTipsPromo(@TipsNotificationsFeatureType int featureType) {
        Context context = ContextUtils.getApplicationContext();
        Intent newIntent = IntentHandler.createTrustedOpenNewTabIntent(context, false);
        newIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        newIntent.putExtra(IntentHandler.EXTRA_TIPS_NOTIFICATION_FEATURE_TYPE, featureType);
        context.startActivity(newIntent);
    }
}
