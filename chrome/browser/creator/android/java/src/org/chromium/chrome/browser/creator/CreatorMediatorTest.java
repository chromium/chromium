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

import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.FeedReliabilityLoggingBridge;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.FeedServiceBridgeJni;
import org.chromium.chrome.browser.feed.FeedStream;
import org.chromium.chrome.browser.feed.FeedStreamJni;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
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
    @Mock
    private FeedStream.Natives mFeedStreamJniMock;
    @Mock
    private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock
    private FeedReliabilityLoggingBridge.Natives mFeedReliabilityLoggingBridgeJniMock;
    @Mock
    private WindowAndroid mWindowAndroid;
    @Mock
    private SnackbarManager mSnackbarManager;
    @Mock
    private Profile mProfile;
    @Mock
    private WebContentsCreator mCreatorWebContents;
    @Mock
    private NewTabCreator mCreatorOpenTab;
    @Mock
    private UnownedUserDataSupplier<ShareDelegate> mShareDelegateSupplier;

    private final String mTitle = "Example";
    private final String mUrl = "example.com";

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private CreatorCoordinator mCreatorCoordinator;
    private CreatorMediator mCreatorMediator;
    private TestActivity mActivity;
    private PropertyModel mCreatorModel;
    private static final byte[] sWebFeedId = "webFeedId".getBytes();

    @Before
    public void setUpTest() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(CreatorApiBridgeJni.TEST_HOOKS, mCreatorBridgeJniMock);
        mJniMocker.mock(FeedStreamJni.TEST_HOOKS, mFeedStreamJniMock);
        mJniMocker.mock(FeedServiceBridgeJni.TEST_HOOKS, mFeedServiceBridgeJniMock);
        mJniMocker.mock(WebFeedBridge.getTestHooksForTesting(), mWebFeedBridgeJniMock);
        mJniMocker.mock(FeedReliabilityLoggingBridge.getTestHooksForTesting(),
                mFeedReliabilityLoggingBridgeJniMock);

        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mCreatorCoordinator = new CreatorCoordinator(mActivity, sWebFeedId, mSnackbarManager,
                mWindowAndroid, mProfile, mTitle, mUrl, mCreatorWebContents, mCreatorOpenTab,
                mShareDelegateSupplier);
        mCreatorModel = mCreatorCoordinator.getCreatorModel();

        mCreatorMediator = new CreatorMediator(mActivity, mCreatorModel);
    }

    @Test
    public void testCreatorMediatorConstruction() {
        assertNotNull("Could not construct CreatorMediator", mCreatorMediator);
    }

    // TODO(crbug.com/1377140): Add tests for followClickHandler and followingClickHandler
}
