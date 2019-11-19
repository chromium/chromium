// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.details;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * State for the details of the Autofill Assistant.
 */
@JNINamespace("autofill_assistant")
public class AssistantDetailsModel extends PropertyModel {
    // TODO(crbug.com/806868): We might want to split this property into multiple, simpler
    // properties.
    @VisibleForTesting
    public static final WritableObjectPropertyKey<AssistantDetails> DETAILS =
            new WritableObjectPropertyKey<>();

    public AssistantDetailsModel() {
        super(DETAILS);
    }

    @CalledByNative
    private void setDetails(AssistantDetails details) {
        set(DETAILS, details);
    }

    @CalledByNative
    // TODO(crbug.com/806868): Make private once this is only called by native.
    public void clearDetails() {
        set(DETAILS, null);
    }
}
