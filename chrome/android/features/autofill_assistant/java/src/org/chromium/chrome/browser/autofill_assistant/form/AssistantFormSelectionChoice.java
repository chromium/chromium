// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.form;

class AssistantFormSelectionChoice {
    private final String mLabel;
    private final boolean mIsInitiallySelected;

    public AssistantFormSelectionChoice(String label, boolean isInitiallySelected) {
        mLabel = label;
        mIsInitiallySelected = isInitiallySelected;
    }

    public String getLabel() {
        return mLabel;
    }

    public boolean isInitiallySelected() {
        return mIsInitiallySelected;
    }
}
