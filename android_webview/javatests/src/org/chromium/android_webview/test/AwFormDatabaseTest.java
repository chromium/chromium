// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwFormDatabase;

/** AwFormDatabaseTest. */
@RunWith(AwJUnit4ClassRunner.class)
public class AwFormDatabaseTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    @Test
    @SmallTest
    public void testSmoke() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            AwFormDatabase.clearFormData();
            Assert.assertFalse(AwFormDatabase.hasFormData());
        });
    }
}
