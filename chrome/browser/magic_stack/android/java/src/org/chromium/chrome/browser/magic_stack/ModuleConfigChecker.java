// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

/** An interface to check module's configuration. */
public interface ModuleConfigChecker {
    /**
     * Returns whether the module can be built due to special restrictions, like location. This
     * method should be called after profile is initialized.
     */
    boolean isEligible();
}
