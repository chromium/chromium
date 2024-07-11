// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ThreadUtils;

/** Utils for testing SurfaceViews (SurfaceViews that display video). */
public class VideoSurfaceViewUtils {

    private static int containsNumChildrenOfType(
            final View view, final Class<? extends View> childType) throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> containsNumChildrenOfTypeOnUiThread(view, childType));
    }

    private static int containsNumChildrenOfTypeOnUiThread(
            final View view, final Class<? extends View> childType) throws Exception {
        return containsNumChildrenOfTypeOnUiThread(view, childType, 0);
    }

    private static int containsNumChildrenOfTypeOnUiThread(
            final View view, final Class<? extends View> childType, int sum) throws Exception {
        if (childType.isInstance(view)) return 1;

        if (view instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) view;
            for (int i = 0; i < viewGroup.getChildCount(); i++) {
                sum += containsNumChildrenOfTypeOnUiThread(viewGroup.getChildAt(i), childType);
            }
        }
        return sum;
    }
}
