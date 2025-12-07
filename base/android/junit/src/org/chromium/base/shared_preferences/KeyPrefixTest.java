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

        assertEquals("Chrome.Feature.KP.*", prefix.pattern());

        assertEquals("Chrome.Feature.KP.DynamicKey", prefix.createKey("DynamicKey"));
        assertEquals("Chrome.Feature.KP.Level.DynamicKey", prefix.createKey("Level.DynamicKey"));
        assertEquals("Chrome.Feature.KP.42", prefix.createKey(42));

        assertTrue(prefix.hasGenerated("Chrome.Feature.KP.DynamicKey"));
        assertTrue(prefix.hasGenerated("Chrome.Feature.KP.Level.DynamicKey"));
        assertTrue(prefix.hasGenerated("Chrome.Feature.KP.42"));
        assertFalse(prefix.hasGenerated("OtherKey"));
    }

    @Test
    @SmallTest
    public void testSuccess_validLegacyPattern() {
        KeyPrefix prefix = new KeyPrefix("legacy_pattern_*");

        assertEquals("legacy_pattern_*", prefix.pattern());
        assertEquals("legacy_pattern_DynamicKey", prefix.createKey("DynamicKey"));

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
