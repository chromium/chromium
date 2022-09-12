// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatBackupAgent;

/** See {@link ChromeBackupAgentImpl}. */
public class ChromeBackupAgent extends SplitCompatBackupAgent {
    @IdentifierNameString
    private static String sImplClassName = "org.chromium.chrome.browser.ChromeBackupAgentImpl";

    public ChromeBackupAgent() {
        super(sImplClassName);
    }
}
