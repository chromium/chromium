// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;

/** Test for {@link PriceUtils}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class PriceUtilsTest {
    private static final int MICROS_TO_UNITS = 1000000;

    @Test
    @SmallTest
    public void testPriceFormatting_lessThanTenUnits() {
        Assert.assertEquals("$0.50", PriceUtils.formatPrice("USD", MICROS_TO_UNITS / 2));
        Assert.assertEquals("$1.00", PriceUtils.formatPrice("USD", MICROS_TO_UNITS));
    }

    @Test
    @SmallTest
    public void testPriceFormatting_ZeroUnit() {
        Assert.assertEquals("$0.00", PriceUtils.formatPrice("USD", 0));
    }

    @Test
    @SmallTest
    public void testPriceFormatting_GreaterOrEqualThanTenUnit() {
        Assert.assertEquals("$10", PriceUtils.formatPrice("USD", 10 * MICROS_TO_UNITS));
        Assert.assertEquals("$10", PriceUtils.formatPrice("USD", (long) (10.4 * MICROS_TO_UNITS)));
        Assert.assertEquals(
                "Should round up.",
                "$11",
                PriceUtils.formatPrice("USD", (long) (10.5 * MICROS_TO_UNITS)));
    }
}
