// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.sync.LocalDataDescription;

@RunWith(BaseRobolectricTestRunner.class)
public class LocalDataDescriptionTest {
    @Test
    public void testConstructorAndDataType() {
        int itemCount = 123;
        String[] domains = {"example.com", "test.org"};
        int domainCount = 2;

        LocalDataDescription localData = new LocalDataDescription(itemCount, domains, domainCount);

        assertEquals(itemCount, localData.itemCount());
    }
}
