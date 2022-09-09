// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.tab.Tab;

import javax.inject.Inject;

/**
 * Provides {@link ReparentingTask} object to CustomTabActivityController.
 * Makes the target class test-friendly by allowing for mock injection.
 */
public class ReparentingTaskProvider {
    @Inject
    public ReparentingTaskProvider() {}

    public ReparentingTask get(Tab tab) {
        return ReparentingTask.from(tab);
    }
}
