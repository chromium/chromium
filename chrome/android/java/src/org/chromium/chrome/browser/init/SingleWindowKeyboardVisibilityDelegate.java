// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/**
 * A {@link ActivityKeyboardVisibilityDelegate} that prevents the multi-window functionality from
 * triggering the layout-based keyboard detection.
 */
public class SingleWindowKeyboardVisibilityDelegate extends ActivityKeyboardVisibilityDelegate {
    public SingleWindowKeyboardVisibilityDelegate(WeakReference<Activity> activity) {
        super(activity);
    }

    @Override
    public boolean isKeyboardShowing(Context context, View view) {
        Activity activity = WindowAndroid.activityFromContext(context);
        if (activity == null && view != null && view.getContext() instanceof Activity) {
            activity = (Activity) view.getContext();
        }

        if (activity != null && MultiWindowUtils.getInstance().isLegacyMultiWindow(activity)) {
            return false; // For multi-window mode we do not track keyboard visibility.
        }
        return super.isKeyboardShowing(context, view);
    }
}