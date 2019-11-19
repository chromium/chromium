// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.browser.tabmodel.document.AsyncTabCreationParams;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Tests that the AsyncTabCreationParamsManager works as expected.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class AsyncTabCreationParamsManagerTest {
    @Rule
    public UiThreadTestRule mRule = new UiThreadTestRule();

    @Test
    @SmallTest
    @UiThreadTest
    public void testBasicAddingAndRemoval() {
        AsyncTabCreationParams asyncParams =
                new AsyncTabCreationParams(new LoadUrlParams("http://google.com"));
        AsyncTabParamsManager.add(11684, asyncParams);

        AsyncTabParams retrievedParams = AsyncTabParamsManager.remove(11684);
        Assert.assertEquals(
                "Removed incorrect parameters from the map", asyncParams, retrievedParams);

        AsyncTabParams failedParams = AsyncTabParamsManager.remove(11684);
        Assert.assertNull("Removed same parameters twice", failedParams);
    }
}
