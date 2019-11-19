// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.form;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.ui.modelutil.ListModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** A model for the assistant form. */
@JNINamespace("autofill_assistant")
public class AssistantFormModel {
    private final ListModel<AssistantFormInput> mInputsModel = new ListModel<>();

    public ListModel<AssistantFormInput> getInputsModel() {
        return mInputsModel;
    }

    @CalledByNative
    private void setInputs(List<AssistantFormInput> inputs) {
        mInputsModel.set(inputs);
    }

    @CalledByNative
    private void clearInputs() {
        mInputsModel.set(Arrays.asList());
    }

    @CalledByNative
    private static List<AssistantFormInput> createInputList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static void addInput(List<AssistantFormInput> inputs, AssistantFormInput input) {
        inputs.add(input);
    }
}
