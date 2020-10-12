// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeBrowserTestRule;

/**
 * Test relating to {@link ShoppingPersistedTabData}
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ShoppingPersistedTabDataTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private static final int TAB_ID = 1;
    private static final boolean IS_INCOGNITO = false;
    private static final String PRICE_STRING = "$2.87";

    @SmallTest
    @Test
    public void testShoppingProto() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = new MockTab(TAB_ID, IS_INCOGNITO);
            ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
            shoppingPersistedTabData.setPriceString(PRICE_STRING);
            byte[] serialized = shoppingPersistedTabData.serialize();
            ShoppingPersistedTabData deserialized = new ShoppingPersistedTabData(tab);
            deserialized.deserialize(serialized);
            Assert.assertEquals(PRICE_STRING, deserialized.getPriceString());
        });
    }
}
