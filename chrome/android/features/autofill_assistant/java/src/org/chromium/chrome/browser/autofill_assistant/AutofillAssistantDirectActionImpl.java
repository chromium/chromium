// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.Arrays;
import java.util.List;

/**
 * A container that corresponds to the DirectActionProto used to store action realated data like
 * arguments.
 */
@JNINamespace("autofill_assistant")
class AutofillAssistantDirectActionImpl implements AutofillAssistantDirectAction {
    /* List of direct actions with the given names. */
    private final List<String> mNames;

    /* List of required argument names for this action. */
    private final List<String> mRequiredArguments;

    /* List of optional argument names for this action. */
    private final List<String> mOptionalArguments;

    @CalledByNative
    public AutofillAssistantDirectActionImpl(String[] names, String[] required, String[] optional) {
        mNames = Arrays.asList(names);
        mRequiredArguments = Arrays.asList(required);
        mOptionalArguments = Arrays.asList(optional);
    }

    @Override
    public List<String> getNames() {
        return mNames;
    }

    @Override
    public List<String> getRequiredArguments() {
        return mRequiredArguments;
    }

    @Override
    public List<String> getOptionalArguments() {
        return mOptionalArguments;
    }
}
