// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.ViewGroup;

/** Interface for factories of additional user form sections. */
public interface AssistantAdditionalSectionFactory {
    /**
     * Instantiates the additional section for {@code context} and adds it at position {@code
     * index} to {@code parent}.
     */
    AssistantAdditionalSection createSection(Context context, ViewGroup parent, int index,
            @Nullable AssistantAdditionalSection.Delegate delegate);
}
