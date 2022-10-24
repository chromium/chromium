// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import static org.junit.Assert.assertNotNull;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests for {@link CreatorMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class CreatorMediatorTest {
    @Mock
    private WebFeedBridge.Natives mWebFeedBridgeJniMock;
    @Mock
    private CreatorApiBridge.Natives mCreatorBridgeJniMock;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private CreatorCoordinator mCreatorCoordinator;
    private CreatorMediator mCreatorMediator;
    private TestActivity mActivity;
    private PropertyModel mCreatorProfileModel;
    private static final byte[] sWebFeedId = "webFeedId".getBytes();

    @Before
    public void setUpTest() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(CreatorApiBridgeJni.TEST_HOOKS, mCreatorBridgeJniMock);
        mJniMocker.mock(WebFeedBridge.getTestHooksForTesting(), mWebFeedBridgeJniMock);

        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mCreatorCoordinator = new CreatorCoordinator(mActivity, sWebFeedId);
        mCreatorProfileModel = mCreatorCoordinator.getCreatorProfileModel();

        mCreatorMediator = new CreatorMediator(mActivity, mCreatorProfileModel);
    }

    @Test
    public void testCreatorMediatorConstruction() {
        assertNotNull("Could not construct CreatorMediator", mCreatorMediator);
    }

    // TODO(crbug.com/1377140): Add tests for followClickHandler and followingClickHandler
}
