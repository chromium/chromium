// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.LoadUrlParams;

/** Tests that the AsyncTabCreationParamsManager works as expected. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AsyncTabCreationParamsManagerTest {
    @Test
    @SmallTest
    @UiThreadTest
    public void testBasicAddingAndRemoval() {
        AsyncTabParamsManager subject = AsyncTabParamsManagerSingleton.getInstance();

        AsyncTabCreationParams asyncParams =
                new AsyncTabCreationParams(new LoadUrlParams("http://google.com"));
        subject.add(11684, asyncParams);

        AsyncTabParams retrievedParams = subject.remove(11684);
        Assert.assertEquals(
                "Removed incorrect parameters from the map", asyncParams, retrievedParams);

        AsyncTabParams failedParams = subject.remove(11684);
        Assert.assertNull("Removed same parameters twice", failedParams);
    }
}
