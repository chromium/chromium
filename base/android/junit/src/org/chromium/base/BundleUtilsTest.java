// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;
import android.content.ContextWrapper;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for the BundleUtils class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BundleUtilsTest {
    private static final String SPLIT_A = "a";
    private static final String SPLIT_B = "b";

    @Test
    public void testIsolatedSplitContextCached() {
        Context baseContext = ContextUtils.getApplicationContext();
        Context context = new ContextWrapper(baseContext) {
            @Override
            public Context createContextForSplit(String splitName) {
                return new ContextWrapper(baseContext);
            }
        };
        Context contextA = BundleUtils.createIsolatedSplitContext(context, SPLIT_A);
        Assert.assertEquals(contextA, BundleUtils.createIsolatedSplitContext(context, SPLIT_A));

        Context contextB = BundleUtils.createIsolatedSplitContext(context, SPLIT_B);
        Assert.assertNotEquals(contextA, contextB);
        Assert.assertEquals(contextB, BundleUtils.createIsolatedSplitContext(context, SPLIT_B));
    }
}
