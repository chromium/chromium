// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;

/**
 * Represents a button.
 */
@JNINamespace("autofill_assistant")
public class AssistantDialogButton {
    private String mLabel;
    @Nullable
    private String mUrl;

    @CalledByNative
    public AssistantDialogButton(String label, @Nullable String url) {
        mLabel = label;
        mUrl = url;
    }

    public void onClick(Context context) {
        if (mUrl != null) {
            CustomTabActivity.showInfoPage(context, mUrl);
        }
    }

    public String getLabel() {
        return mLabel;
    }
}
