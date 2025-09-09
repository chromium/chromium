// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.build.annotations.NullMarked;

/** TODO(crbug.com/442347616): Remove after downstream impl is removed. */
@NullMarked
public abstract class PasswordManagerBackendSupportHelper {
    public boolean isBackendPresent() {
        return false;
    }
}
