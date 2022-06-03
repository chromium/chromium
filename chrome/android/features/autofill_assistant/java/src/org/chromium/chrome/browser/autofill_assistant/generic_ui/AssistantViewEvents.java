// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.generic_ui;

import android.view.View;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/** JNI bridge between {@code generic_ui_events_android} and Java. */
@JNINamespace("autofill_assistant")
public class AssistantViewEvents {
    @CalledByNative
    private static void setOnClickListener(
            View view, String identifier, AssistantGenericUiDelegate delegate) {
        view.setOnClickListener(unused -> delegate.onViewClicked(identifier));
    }
}
