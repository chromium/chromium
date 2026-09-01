// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import org.junit.After;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.CompatQuirks;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Robolectric unit tests for {@link CompatQuirks}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CompatQuirksTest {
    @After
    public void tearDown() {
        CompatQuirks.resetForTesting();
    }

    @Test
    public void testWithoutDelegateReturnsFalse() {
        Assert.assertFalse(CompatQuirks.isEnabled(CompatQuirks.Quirk.LEGACY_DARK_MODE));
        Assert.assertFalse(CompatQuirks.isEnabled(CompatQuirks.Quirk.ALLOW_SNIFFING_FILE_URLS));
    }

    @Test
    public void testOverrideForTestingTakesPrecedence() {
        CompatQuirks.overrideForTesting(CompatQuirks.Quirk.LEGACY_DARK_MODE, true);
        Assert.assertTrue(CompatQuirks.isEnabled(CompatQuirks.Quirk.LEGACY_DARK_MODE));

        CompatQuirks.overrideForTesting(CompatQuirks.Quirk.LEGACY_DARK_MODE, false);
        Assert.assertFalse(CompatQuirks.isEnabled(CompatQuirks.Quirk.LEGACY_DARK_MODE));
    }

    @Test
    public void testResetForTesting() {
        CompatQuirks.overrideForTesting(CompatQuirks.Quirk.FIXUP_OCTOTHORPES_IN_LOAD_DATA, true);
        Assert.assertTrue(
                CompatQuirks.isEnabled(CompatQuirks.Quirk.FIXUP_OCTOTHORPES_IN_LOAD_DATA));

        CompatQuirks.resetForTesting();

        Assert.assertFalse(
                CompatQuirks.isEnabled(CompatQuirks.Quirk.FIXUP_OCTOTHORPES_IN_LOAD_DATA));
    }
}
