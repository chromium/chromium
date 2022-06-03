// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.chrome.browser.base.SplitCompatBackupAgent;
import org.chromium.chrome.browser.base.SplitCompatUtils;

/** See {@link ChromeBackupAgentImpl}. */
public class ChromeBackupAgent extends SplitCompatBackupAgent {
    public ChromeBackupAgent() {
        super(SplitCompatUtils.getIdentifierName(
                "org.chromium.chrome.browser.ChromeBackupAgentImpl"));
    }
}
