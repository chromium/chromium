// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.password_generation;

import android.app.Activity;
import android.widget.TextView;

import org.chromium.base.ThreadUtils;

import java.util.concurrent.atomic.AtomicReference;

public class TouchToFillPasswordGenerationTestHelper {
    public static String acceptPasswordInGenerationBottomSheet(Activity activity) {
        String password = getTextFromTextView(activity, R.id.password);
        ThreadUtils.runOnUiThreadBlocking(
                () -> activity.findViewById(R.id.use_password_button).performClick());
        return password;
    }

    public static void rejectPasswordInGenerationBottomSheet(Activity activity) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> activity.findViewById(R.id.reject_password_button).performClick());
    }

    private static String getTextFromTextView(Activity activity, int id) {
        AtomicReference<String> textRef = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    textRef.set(((TextView) activity.findViewById(id)).getText().toString());
                    return true;
                });
        return textRef.get();
    }
}
