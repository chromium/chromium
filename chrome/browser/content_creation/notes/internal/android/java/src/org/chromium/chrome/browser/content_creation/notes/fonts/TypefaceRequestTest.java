// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes.fonts;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;

/** Tests working of TypefaceRequest. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class TypefaceRequestTest {
    @Test
    @SmallTest
    public void testRobotoStandardQuery() {
        TypefaceRequest request = new TypefaceRequest("Roboto", 400);
        Assert.assertTrue("name=Roboto&weight=400".equals(request.toQuery()));
    }

    @Test
    @SmallTest
    public void testRockSaltBoldQuery() {
        TypefaceRequest request = new TypefaceRequest("Rock Salt", 700);
        Assert.assertTrue("name=Rock Salt&weight=700".equals(request.toQuery()));
    }

    @Test
    @SmallTest
    public void testRobotoStandardCondensedQuery() {
        TypefaceRequest request = new TypefaceRequest("Roboto Condensed", 400);
        Assert.assertTrue("name=Roboto&weight=400&width=75".equals(request.toQuery()));
    }

    @Test
    @SmallTest
    public void testRockSaltBoldCondensedQuery() {
        TypefaceRequest request = new TypefaceRequest("Rock Salt Condensed", 700);
        Assert.assertTrue("name=Rock Salt&weight=700&width=75".equals(request.toQuery()));
    }
}
