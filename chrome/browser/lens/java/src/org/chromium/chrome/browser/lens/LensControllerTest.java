// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens;

import static org.junit.Assert.assertNotEquals;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public class LensControllerTest {

    @Test
    public void testGetLensIconResourceId() {
        int resourceId = LensController.getInstance().getLensIconResourceId();
        assertNotEquals("Lens icon resource ID should be valid", 0, resourceId);
    }
}
