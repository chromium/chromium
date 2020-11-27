// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.autofill_assistant.AssistantInfoPopup;
import org.chromium.components.autofill.EditableOption;

/**
 * Represents a single login choice.
 *
 * <p>Note: currently, login choices are always considered 'complete'.</p>
 */
public class AssistantLoginChoice extends EditableOption {
    private final int mPriority;
    private final String mSublabelAccessibilityHint;
    private final @Nullable AssistantInfoPopup mInfoPopup;
    private final @Nullable String mEditButtonContentDescription;

    /**
     * @param identifier The unique identifier of this login choice.
     * @param label The label to display to the user.
     * @param sublabel Optional sublabel to display below the label.
     * @param sublabelAccessibilityHint The a11y hint for {@code sublabel}.
     * @param priority The priority of this login choice (lower value == higher priority). Can be -1
     * to indicate default/auto.
     * @param infoPopup Optional popup that provides further information for this login choice.
     * @param editButtonContentDescription Optional content description for the edit button.
     */
    public AssistantLoginChoice(String identifier, String label, String sublabel,
            String sublabelAccessibilityHint, int priority, @Nullable AssistantInfoPopup infoPopup,
            @Nullable String editButtonContentDescription) {
        super(identifier, label, sublabel, null);
        mPriority = priority;
        mSublabelAccessibilityHint = sublabelAccessibilityHint;
        mInfoPopup = infoPopup;
        mEditButtonContentDescription = editButtonContentDescription;
    }

    public int getPriority() {
        return mPriority;
    }

    public @Nullable AssistantInfoPopup getInfoPopup() {
        return mInfoPopup;
    }

    public String getSublabelAccessibilityHint() {
        return mSublabelAccessibilityHint;
    }

    public @Nullable String getEditButtonContentDescription() {
        return mEditButtonContentDescription;
    }
}
