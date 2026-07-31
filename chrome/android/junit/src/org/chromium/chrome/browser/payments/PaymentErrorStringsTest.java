// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.text.TextUtils;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.payments.ErrorStrings;

import java.lang.reflect.Field;

/** Tests for generated {@link ErrorStrings.java}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class PaymentErrorStringsTest {
    // Tests that error strings are generated successfully and have non-empty values.
    @Test
    public void checkNonemptyTest() throws Throwable {
        Field[] fields = ErrorStrings.class.getFields();
        for (Field f : fields) {
            Assert.assertTrue(f.getType().equals(String.class));
            String errorMessage = (String) f.get(null);
            Assert.assertFalse(TextUtils.isEmpty(errorMessage));
        }
    }
}
