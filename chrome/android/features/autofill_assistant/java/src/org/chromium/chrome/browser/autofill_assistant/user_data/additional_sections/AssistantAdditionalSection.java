// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections;

import android.view.View;

/** Interface for an additional section of the user data form. */
public interface AssistantAdditionalSection {
    /** Delegate interface for generic key/value widgets. */
    interface Delegate {
        void onValueChanged(String key, String value);
    }

    /** Returns the root view of the section. */
    View getView();

    /** Sets the padding for the top-most and the bottom-most view, respectively. */
    void setPaddings(int topPadding, int bottomPadding);

    /** Sets the delegate to notify for changes. */
    void setDelegate(Delegate delegate);
}
