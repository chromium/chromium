// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.optimization_guide.proto.HintsProto;

import java.util.Arrays;

/** Unit tests for OptimizationGuideBridgeFactory */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class OptimizationGuideBridgeFactoryUnitTest {
    @Rule public JniMocker mocker = new JniMocker();

    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Mock OptimizationGuideBridge.Natives mOptimizationGuideBridgeJniMock;

    @Mock private Profile mProfile1;

    @Mock private Profile mProfile2;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(OptimizationGuideBridgeJni.TEST_HOOKS, mOptimizationGuideBridgeJniMock);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"OptimizationHints"})
    public void testFactoryMethod() {
        OptimizationGuideBridgeFactory bridgeFactory =
                new OptimizationGuideBridgeFactory(
                        Arrays.asList(HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR));
        OptimizationGuideBridge bridgeRegularProfile = bridgeFactory.create();
        Assert.assertEquals(bridgeRegularProfile, bridgeFactory.create());

        Profile.setLastUsedProfileForTesting(mProfile1);
        OptimizationGuideBridge bridgeRegularMockProfile1 = bridgeFactory.create();
        Assert.assertNotEquals(bridgeRegularProfile, bridgeRegularMockProfile1);
        Assert.assertEquals(bridgeRegularMockProfile1, bridgeFactory.create());

        Profile.setLastUsedProfileForTesting(mProfile2);
        OptimizationGuideBridge bridgeRegularMockProfile2 = bridgeFactory.create();
        Assert.assertNotEquals(bridgeRegularProfile, bridgeRegularMockProfile2);
        Assert.assertEquals(bridgeRegularMockProfile2, bridgeFactory.create());

        // Back to regular profile
        Profile.setLastUsedProfileForTesting(null);
        Assert.assertEquals(bridgeRegularProfile, bridgeFactory.create());

        // Mock profile 1 again
        Profile.setLastUsedProfileForTesting(mProfile1);
        Assert.assertEquals(bridgeRegularMockProfile1, bridgeFactory.create());

        // Mock profile 2 again
        Profile.setLastUsedProfileForTesting(mProfile2);
        Assert.assertEquals(bridgeRegularMockProfile2, bridgeFactory.create());
    }
}
