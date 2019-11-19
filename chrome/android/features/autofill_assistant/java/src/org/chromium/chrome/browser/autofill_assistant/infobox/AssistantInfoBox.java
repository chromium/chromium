// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.infobox;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Java side equivalent of autofill_assistant::InfoBoxProto.
 */
@JNINamespace("autofill_assistant")
public class AssistantInfoBox {
    private final String mImagePath;
    private final String mExplanation;

    public AssistantInfoBox(String imagePath, String explanation) {
        this.mImagePath = imagePath;
        this.mExplanation = explanation;
    }

    public String getImagePath() {
        return mImagePath;
    }

    public String getExplanation() {
        return mExplanation;
    }

    /**
     * Create infobox with the given values.
     */
    @CalledByNative
    private static AssistantInfoBox create(String imagePath, String explanation) {
        return new AssistantInfoBox(imagePath, explanation);
    }
}
