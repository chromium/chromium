// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.generic_ui;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * State for the generic UI of the Autofill Assistant.
 */
@JNINamespace("autofill_assistant")
public class AssistantGenericUiModel extends PropertyModel {
    /** The view inflated by the generic UI framework. */
    public static final WritableObjectPropertyKey<View> VIEW = new WritableObjectPropertyKey<>();

    public AssistantGenericUiModel() {
        super(VIEW);
    }

    @CalledByNative
    private void setView(@Nullable View view) {
        set(VIEW, view);
    }
}
