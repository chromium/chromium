// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;

/** Unit tests for OptimizationGuideBridgeFactory */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class OptimizationGuideBridgeFactoryUnitTest {
    @Rule public JniMocker mocker = new JniMocker();

    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"OptimizationHints"})
    public void testFactoryMethod() {
        OptimizationGuideBridge bridge =
                OptimizationGuideBridgeFactory.getForProfile(
                        ProfileManager.getLastUsedRegularProfile());
        Assert.assertNotNull(bridge);
        Assert.assertEquals(
                bridge,
                OptimizationGuideBridgeFactory.getForProfile(
                        ProfileManager.getLastUsedRegularProfile()));
    }
}
