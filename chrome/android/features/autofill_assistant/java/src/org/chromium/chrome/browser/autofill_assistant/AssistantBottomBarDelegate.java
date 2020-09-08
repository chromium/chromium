// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

/** Common interface for the bottom bar delegate. */
public interface AssistantBottomBarDelegate {
    /** The back button has been pressed. */
    boolean onBackButtonPressed();
}
