// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.creator.test.R;
import org.chromium.chrome.browser.feed.FeedReliabilityLoggingBridge;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.FeedServiceBridgeJni;
import org.chromium.chrome.browser.feed.FeedSurfaceRendererBridge;
import org.chromium.chrome.browser.feed.FeedSurfaceRendererBridgeJni;
import org.chromium.chrome.browser.feed.SingleWebFeedEntryPoint;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.FollowResults;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.UnfollowResults;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionRequestStatus;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.url.JUnitTestGURLs;

/** Tests for {@link CreatorMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CreatorMediatorTest {
    @Mock private WebFeedBridge.Natives mWebFeedBridgeJniMock;
    @Mock private FeedSurfaceRendererBridge.Natives mFeedSurfaceRendererBridgeJniMock;
    @Mock private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock private FeedReliabilityLoggingBridge.Natives mFeedReliabilityLoggingBridgeJniMock;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private CreatorSnackbarController mCreatorSnackbarController;
    @Mock private Profile mProfile;
    @Mock private WebContentsCreator mCreatorWebContents;
    @Mock private NewTabCreator mCreatorOpenTab;
    @Mock private UnownedUserDataSupplier<ShareDelegate> mShareDelegateSupplier;
    @Mock private SignInInterstitialInitiator mSignInInterstitialInitiator;

    @Captor private ArgumentCaptor<Callback<FollowResults>> mFollowResultsCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<UnfollowResults>> mUnfollowResultsCallbackCaptor;

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private final String mUrl = JUnitTestGURLs.EXAMPLE_URL.getSpec();
    private final byte[] mWebFeedId = "webFeedId".getBytes();
    private CreatorCoordinator mCreatorCoordinator;
    private CreatorMediator mCreatorMediator;
    private TestActivity mActivity;
    private PropertyModel mCreatorModel;

    @Before
    public void setUpTest() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(FeedSurfaceRendererBridgeJni.TEST_HOOKS, mFeedSurfaceRendererBridgeJniMock);
        mJniMocker.mock(FeedServiceBridgeJni.TEST_HOOKS, mFeedServiceBridgeJniMock);
        mJniMocker.mock(WebFeedBridge.getTestHooksForTesting(), mWebFeedBridgeJniMock);
        mJniMocker.mock(
                FeedReliabilityLoggingBridge.getTestHooksForTesting(),
                mFeedReliabilityLoggingBridgeJniMock);

        when(mFeedServiceBridgeJniMock.isSignedIn()).thenReturn(true);

        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mCreatorCoordinator =
                new CreatorCoordinator(
                        mActivity,
                        mWebFeedId,
                        mSnackbarManager,
                        mWindowAndroid,
                        mProfile,
                        mUrl,
                        mCreatorWebContents,
                        mCreatorOpenTab,
                        mShareDelegateSupplier,
                        SingleWebFeedEntryPoint.OTHER,
                        /* isFollowing= */ false,
                        mSignInInterstitialInitiator);
        mCreatorModel = mCreatorCoordinator.getCreatorModel();

        mCreatorMediator =
                new CreatorMediator(
                        mActivity,
                        mCreatorModel,
                        mCreatorSnackbarController,
                        mSignInInterstitialInitiator);
    }

    @Test
    public void testCreatorMediatorConstruction() {
        assertNotNull("Could not construct CreatorMediator", mCreatorMediator);
    }

    @Test
    public void testCreatorModel_FollowClickHandler_Setup() {
        Runnable runnableFollow = mCreatorModel.get(CreatorProperties.ON_FOLLOW_CLICK_KEY);
        assertNotNull(runnableFollow);
    }

    @Test
    public void testCreatorModel_UnfollowClickHandler_Setup() {
        Runnable runnableUnfollow = mCreatorModel.get(CreatorProperties.ON_FOLLOWING_CLICK_KEY);
        assertNotNull(runnableUnfollow);
    }

    @Test
    public void testCreatorModel_FollowClickHandler_CallWebFeedBridgeCorrectly_Profile() {
        View creatorProfileView = mCreatorCoordinator.getProfileView();
        ButtonCompat followButton = creatorProfileView.findViewById(R.id.creator_follow_button);
        followButton.performClick();

        verify(mWebFeedBridgeJniMock)
                .followWebFeedById(
                        eq(mWebFeedId),
                        eq(false),
                        eq(WebFeedBridge.CHANGE_REASON_SINGLE_WEB_FEED),
                        any());
    }

    @Test
    public void testCreatorModel_UnfollowClickHandler_CallWebFeedBridgeCorrectly_Profile() {
        View creatorProfileView = mCreatorCoordinator.getProfileView();
        ButtonCompat followingButton =
                creatorProfileView.findViewById(R.id.creator_following_button);
        followingButton.performClick();

        verify(mWebFeedBridgeJniMock)
                .unfollowWebFeed(
                        eq(mWebFeedId),
                        eq(false),
                        eq(WebFeedBridge.CHANGE_REASON_SINGLE_WEB_FEED),
                        any());
    }

    @Test
    public void testCreatorModel_FollowClickHandler_CallWebFeedBridgeCorrectly_Toolbar() {
        View creatorView = mCreatorCoordinator.getView();
        ButtonCompat followButton = creatorView.findViewById(R.id.creator_follow_button_toolbar);
        followButton.performClick();

        verify(mWebFeedBridgeJniMock)
                .followWebFeedById(
                        eq(mWebFeedId),
                        eq(false),
                        eq(WebFeedBridge.CHANGE_REASON_SINGLE_WEB_FEED),
                        any());
    }

    @Test
    public void testCreatorModel_UnfollowClickHandler_CallWebFeedBridgeCorrectly_Toolbar() {
        View creatorView = mCreatorCoordinator.getView();
        ButtonCompat followingButton =
                creatorView.findViewById(R.id.creator_following_button_toolbar);
        followingButton.performClick();

        verify(mWebFeedBridgeJniMock)
                .unfollowWebFeed(
                        eq(mWebFeedId),
                        eq(false),
                        eq(WebFeedBridge.CHANGE_REASON_SINGLE_WEB_FEED),
                        any());
    }

    @Test
    public void testCreatorModel_FollowClickHandler_ExpectedBehavior_Profile() {
        View creatorProfileView = mCreatorCoordinator.getProfileView();
        ButtonCompat followButton = creatorProfileView.findViewById(R.id.creator_follow_button);
        ButtonCompat followingButton =
                creatorProfileView.findViewById(R.id.creator_following_button);
        followButton.performClick();

        verify(mWebFeedBridgeJniMock)
                .followWebFeedById(
                        eq(mWebFeedId),
                        eq(false),
                        eq(WebFeedBridge.CHANGE_REASON_SINGLE_WEB_FEED),
                        mFollowResultsCallbackCaptor.capture());
        mFollowResultsCallbackCaptor
                .getValue()
                .onResult(new FollowResults(WebFeedSubscriptionRequestStatus.SUCCESS, null));

        assertTrue(followButton.getVisibility() == View.GONE);
        assertTrue(followingButton.getVisibility() == View.VISIBLE);
    }

    @Test
    public void testCreatorModel_UnfollowClickHandler_ExpectedBehavior_Profile() {
        View creatorProfileView = mCreatorCoordinator.getProfileView();
        ButtonCompat followButton = creatorProfileView.findViewById(R.id.creator_follow_button);
        ButtonCompat followingButton =
                creatorProfileView.findViewById(R.id.creator_following_button);
        followingButton.performClick();

        verify(mWebFeedBridgeJniMock)
                .unfollowWebFeed(
                        eq(mWebFeedId),
                        eq(false),
                        eq(WebFeedBridge.CHANGE_REASON_SINGLE_WEB_FEED),
                        mUnfollowResultsCallbackCaptor.capture());
        mUnfollowResultsCallbackCaptor
                .getValue()
                .onResult(new UnfollowResults(WebFeedSubscriptionRequestStatus.SUCCESS));

        assertTrue(followButton.getVisibility() == View.VISIBLE);
        assertTrue(followingButton.getVisibility() == View.GONE);
    }

    @Test
    public void testCreatorModel_FollowClickHandler_ExpectedBehavior_Toolbar() {
        View creatorView = mCreatorCoordinator.getView();
        ButtonCompat followButton = creatorView.findViewById(R.id.creator_follow_button_toolbar);
        ButtonCompat followingButton =
                creatorView.findViewById(R.id.creator_following_button_toolbar);
        followButton.performClick();

        verify(mWebFeedBridgeJniMock)
                .followWebFeedById(
                        eq(mWebFeedId),
                        eq(false),
                        eq(WebFeedBridge.CHANGE_REASON_SINGLE_WEB_FEED),
                        mFollowResultsCallbackCaptor.capture());
        mFollowResultsCallbackCaptor
                .getValue()
                .onResult(new FollowResults(WebFeedSubscriptionRequestStatus.SUCCESS, null));

        assertTrue(followButton.getVisibility() == View.GONE);
        assertTrue(followingButton.getVisibility() == View.VISIBLE);
    }

    @Test
    public void testCreatorModel_UnfollowClickHandler_ExpectedBehavior_Toolbar() {
        View creatorView = mCreatorCoordinator.getView();
        ButtonCompat followButton = creatorView.findViewById(R.id.creator_follow_button_toolbar);
        ButtonCompat followingButton =
                creatorView.findViewById(R.id.creator_following_button_toolbar);
        followingButton.performClick();

        verify(mWebFeedBridgeJniMock)
                .unfollowWebFeed(
                        eq(mWebFeedId),
                        eq(false),
                        eq(WebFeedBridge.CHANGE_REASON_SINGLE_WEB_FEED),
                        mUnfollowResultsCallbackCaptor.capture());
        mUnfollowResultsCallbackCaptor
                .getValue()
                .onResult(new UnfollowResults(WebFeedSubscriptionRequestStatus.SUCCESS));

        assertTrue(followButton.getVisibility() == View.VISIBLE);
        assertTrue(followingButton.getVisibility() == View.GONE);
    }
}
