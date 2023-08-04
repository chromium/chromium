// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

/**
 * Coordinator for the autofill options settings screen. Connects the settings fragment with ...
 *   ... a model keeping track of the settings state, and
 *   ... a mediator to ensure the settings UI is consistent with prefs.
 */
public class AutofillOptionsCoordinator {
    /**
     * Creates a new coordinator and attaches it to the fragment. Waits until the fragment is ready
     * before completing initialization.
     *
     * @param fragment A n@{link AutofillOptionsFragment} hosting all settings.
     */
    public static void createFor(AutofillOptionsFragment fragment){
            // TODO(crbug/1469795): Implement.
    };
}
