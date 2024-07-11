// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import org.junit.rules.ExternalResource;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;

/**
 * JUnit 4 rule that disables the First-Run Flow for tests. This is needed to correctly populate the
 * Context Menu.
 *
 * <p>The CommandLineFlags setting is redundant, but helps ensure that clients know that they don't
 * need to add it themselves. This is also set in ChromeActivityTest, but having this here adds
 * resilience to changes in that class.
 */
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class DisableFirstRun extends ExternalResource {
    @Override
    protected void before() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
    }

    @Override
    protected void after() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));
    }
}
