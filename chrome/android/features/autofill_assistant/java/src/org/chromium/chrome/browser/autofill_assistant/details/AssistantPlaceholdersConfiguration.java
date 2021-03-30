// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.details;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Configuration for the details placeholders.
 */
@JNINamespace("autofill_assistant")
public class AssistantPlaceholdersConfiguration {
    private final boolean mShowImagePlaceholder;
    private final boolean mShowTitlePlaceholder;
    private final boolean mShowDescriptionLine1Placeholder;
    private final boolean mShowDescriptionLine2Placeholder;
    private final boolean mShowDescriptionLine3Placeholder;

    @CalledByNative
    public AssistantPlaceholdersConfiguration(boolean showImagePlaceholder,
            boolean showTitlePlaceholder, boolean showDescriptionLine1Placeholder,
            boolean showDescriptionLine2Placeholder, boolean showDescriptionLine3Placeholder) {
        mShowImagePlaceholder = showImagePlaceholder;
        mShowTitlePlaceholder = showTitlePlaceholder;
        mShowDescriptionLine1Placeholder = showDescriptionLine1Placeholder;
        mShowDescriptionLine2Placeholder = showDescriptionLine2Placeholder;
        mShowDescriptionLine3Placeholder = showDescriptionLine3Placeholder;
    }

    public boolean getShowImagePlaceholder() {
        return mShowImagePlaceholder;
    }

    public boolean getShowTitlePlaceholder() {
        return mShowTitlePlaceholder;
    }

    public boolean getShowDescriptionLine1Placeholder() {
        return mShowDescriptionLine1Placeholder;
    }

    public boolean getShowDescriptionLine2Placeholder() {
        return mShowDescriptionLine2Placeholder;
    }

    public boolean getShowDescriptionLine3Placeholder() {
        return mShowDescriptionLine3Placeholder;
    }
}
