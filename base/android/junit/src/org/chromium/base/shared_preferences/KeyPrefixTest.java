// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.shared_preferences;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link KeyPrefix}. */
@RunWith(BaseRobolectricTestRunner.class)
public class KeyPrefixTest {
    @Test
    @SmallTest
    public void testSuccess_validPattern() {
        KeyPrefix prefix = new KeyPrefix("Chrome.Feature.KP.*");

        assertEquals(prefix.pattern(), "Chrome.Feature.KP.*");

        assertEquals(prefix.createKey("DynamicKey"), "Chrome.Feature.KP.DynamicKey");
        assertEquals(prefix.createKey("Level.DynamicKey"), "Chrome.Feature.KP.Level.DynamicKey");
        assertEquals(prefix.createKey(42), "Chrome.Feature.KP.42");

        assertTrue(prefix.hasGenerated("Chrome.Feature.KP.DynamicKey"));
        assertTrue(prefix.hasGenerated("Chrome.Feature.KP.Level.DynamicKey"));
        assertTrue(prefix.hasGenerated("Chrome.Feature.KP.42"));
        assertFalse(prefix.hasGenerated("OtherKey"));
    }

    @Test
    @SmallTest
    public void testSuccess_validLegacyPattern() {
        KeyPrefix prefix = new KeyPrefix("legacy_pattern_*");

        assertEquals(prefix.pattern(), "legacy_pattern_*");
        assertEquals(prefix.createKey("DynamicKey"), "legacy_pattern_DynamicKey");

        assertTrue(prefix.hasGenerated("legacy_pattern_DynamicKey"));
        assertFalse(prefix.hasGenerated("OtherKey"));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testError_missingPeriod() {
        new KeyPrefix("Chrome.Feature.KP");
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testError_missingStar() {
        new KeyPrefix("Chrome.Feature.KP.");
    }
}
