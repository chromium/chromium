// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

/**
 * Represents a simple info popup.
 */
public class AssistantInfoPopup {
    private final String mTitle;
    private final String mText;

    public AssistantInfoPopup(String title, String text) {
        mTitle = title;
        mText = text;
    }

    public String getTitle() {
        return mTitle;
    }

    public String getText() {
        return mText;
    }
}
