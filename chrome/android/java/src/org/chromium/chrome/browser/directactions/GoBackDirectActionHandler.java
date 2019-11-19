// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

/**
 * Maps the direct action {@code go_back} to the same behavior as the one defined when pressing the
 * back button.
 */
public class GoBackDirectActionHandler extends SimpleDirectActionHandler {
    private final Runnable mAction;

    public GoBackDirectActionHandler(Runnable action) {
        super(ChromeDirectActionIds.GO_BACK);

        mAction = action;
    }

    @Override
    protected boolean isAvailable() {
        // "go back" is always available
        return true;
    }

    @Override
    protected void run() {
        mAction.run();
    }
}
