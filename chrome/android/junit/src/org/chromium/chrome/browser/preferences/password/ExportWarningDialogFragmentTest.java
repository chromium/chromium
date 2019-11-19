// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.password;

import android.support.v4.app.FragmentActivity;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests for the confirmation dialog used by the "Save passwords" settings page during exporting
 * passwords.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExportWarningDialogFragmentTest {
    /**
     * Ensure that dismissing the dialog is safe even if it was not initialized properly. This
     * models dismissing after the dialog is re-created by Android upon bringing a previously kiled
     * Chrome into foreground.
     */
    @Test
    public void testDismissWithoutInit() {
        FragmentActivity testActivity = Robolectric.setupActivity(FragmentActivity.class);

        ExportWarningDialogFragment exportWarningDialogFragment = new ExportWarningDialogFragment();
        // No initialization, just show and dismiss.
        exportWarningDialogFragment.show(testActivity.getSupportFragmentManager(), null);
        exportWarningDialogFragment.dismiss();
        // There should be no crash.
    }
}
