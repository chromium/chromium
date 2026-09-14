// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertFalse;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Robolectric unit tests for {@link MultiColumnSettings}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MultiColumnSettingsUnitTest {
    /**
     * Regression test for crbug.com/561275965: A theme change destroys the settings activity and
     * detaches its fragments, but a pending layout pass on the old view hierarchy can still query
     * the column mode. A fragment that is not attached to a host has no context, which used to make
     * isTwoColumn() throw IllegalStateException from getResources().
     */
    @Test
    public void testIsTwoColumn_whenFragmentNotAttached_doesNotCrash() {
        MultiColumnSettings fragment = new MultiColumnSettings();

        assertFalse(fragment.isTwoColumn());
    }
}
