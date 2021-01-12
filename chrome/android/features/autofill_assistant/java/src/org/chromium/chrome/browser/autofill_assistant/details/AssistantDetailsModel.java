// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.details;

import android.support.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * State for the details of the Autofill Assistant.
 */
@JNINamespace("autofill_assistant")
public class AssistantDetailsModel extends PropertyModel {
    // TODO(crbug.com/806868): We might want to split this property into multiple, simpler
    // properties.
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static final WritableObjectPropertyKey<List<AssistantDetails>> DETAILS =
            new WritableObjectPropertyKey<>();

    public AssistantDetailsModel() {
        super(DETAILS);
        set(DETAILS, new ArrayList<>());
    }

    @CalledByNative
    private static List<AssistantDetails> createDetailsList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static void addDetails(List<AssistantDetails> list, AssistantDetails details) {
        list.add(details);
    }

    @CalledByNative
    @VisibleForTesting
    public void setDetailsList(List<AssistantDetails> list) {
        set(DETAILS, list);
    }
}
