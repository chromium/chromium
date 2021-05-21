// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController.FeedLauncher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/**
 * Tests {@link WebFeedSnackbarController}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {})
public final class WebFeedSnackbarControllerTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final GURL sTestUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
    private static final String sTitle = "Example Title";
    private static final byte[] sFollowId = new byte[] {1, 2, 3};

    @Mock
    private FeedLauncher mFeedLauncher;
    @Mock
    private Tracker mTracker;
    @Mock
    public WebFeedBridge mWebFeedBridge;
    private Context mContext;
    @Mock
    private Profile mProfile;
    private ModalDialogManager mDialogManager =
            new ModalDialogManager(Mockito.mock(ModalDialogManager.Presenter.class), 0);
    @Mock
    private SnackbarManager mSnackbarManager;
    private WebFeedSnackbarController mWebFeedSnackbarController;

    @Captor
    private ArgumentCaptor<Snackbar> mSnackbarCaptor;

    @Before
    public void setUp() {
        Profile.setLastUsedProfileForTesting(mProfile);
        MockitoAnnotations.initMocks(this);
        mContext = Robolectric.setupActivity(Activity.class);
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE))
                .thenReturn(false);
        TrackerFactory.setTrackerForTests(mTracker);

        mWebFeedSnackbarController = new WebFeedSnackbarController(RuntimeEnvironment.application,
                mFeedLauncher, mDialogManager, mSnackbarManager, mWebFeedBridge);
    }

    @After
    public void tearDown() {
        TrackerFactory.setTrackerForTests(null);
        Profile.setLastUsedProfileForTesting(null);
    }

    @Test
    @SmallTest
    public void showSnackbarForFollow_successful_withMetadata() {
        mWebFeedSnackbarController.showPostFollowHelp(
                getSuccessfulFollowResult(), sFollowId, sTestUrl, sTitle);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals("Snackbar should be for successful follow.",
                Snackbar.UMA_WEB_FEED_FOLLOW_SUCCESS, snackbar.getIdentifierForTesting());
        assertEquals("Snackbar message should be for successful follow with title from metadata.",
                mContext.getString(R.string.web_feed_follow_success_snackbar_message,
                        getSuccessfulFollowResult().metadata.title),
                snackbar.getTextForTesting());
    }

    @Test
    @SmallTest
    public void showSnackbarForFollow_successful_noMetadata() {
        WebFeedBridge.FollowResults followResults = new WebFeedBridge.FollowResults(
                WebFeedSubscriptionRequestStatus.SUCCESS, /*metadata=*/null);

        mWebFeedSnackbarController.showPostFollowHelp(followResults, sFollowId, sTestUrl, sTitle);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals("Snackbar should be for successful follow.",
                Snackbar.UMA_WEB_FEED_FOLLOW_SUCCESS, snackbar.getIdentifierForTesting());
        assertEquals("Snackbar message should be for successful follow with title from input.",
                mContext.getString(R.string.web_feed_follow_success_snackbar_message, sTitle),
                snackbar.getTextForTesting());
    }

    @Test
    @SmallTest
    public void showSnackbarForFollow_correctDuration() {
        WebFeedBridge.FollowResults followResults = new WebFeedBridge.FollowResults(
                WebFeedSubscriptionRequestStatus.SUCCESS, /*metadata=*/null);

        mWebFeedSnackbarController.showPostFollowHelp(followResults, sFollowId, sTestUrl, sTitle);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        assertEquals("Snackbar duration for follow should be correct.",
                WebFeedSnackbarController.SNACKBAR_DURATION_MS,
                mSnackbarCaptor.getValue().getDuration());
    }

    @Test
    @SmallTest
    public void showPromoDialogForFollow_successful_active() {
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE))
                .thenReturn(true);
        WebFeedBridge.FollowResults followResults =
                new WebFeedBridge.FollowResults(WebFeedSubscriptionRequestStatus.SUCCESS,
                        new WebFeedBridge.WebFeedMetadata(sFollowId, sTitle, sTestUrl,
                                WebFeedSubscriptionRequestStatus.SUCCESS, /*isActive=*/
                                true, /*isRecommended=*/false));

        mWebFeedSnackbarController.showPostFollowHelp(followResults, sFollowId, sTestUrl, sTitle);

        View currentDialog =
                mDialogManager.getCurrentDialogForTest().get(ModalDialogProperties.CUSTOM_VIEW);
        verify(mSnackbarManager, times(0)).showSnackbar(any());
        assertTrue("Dialog should be showing.", mDialogManager.isShowing());
        assertEquals("Dialog title should be correct for active follow.",
                mContext.getString(R.string.web_feed_post_follow_dialog_title, sTitle),
                ((TextView) currentDialog.findViewById(R.id.web_feed_dialog_title)).getText());
        assertEquals("Dialog details should be for active follow.",
                mContext.getString(
                        R.string.web_feed_post_follow_dialog_stories_ready_description, sTitle),
                ((TextView) currentDialog.findViewById(R.id.web_feed_dialog_details)).getText());
    }

    @Test
    @SmallTest
    public void showPromoDialogForFollow_successful_notActive() {
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE))
                .thenReturn(true);
        WebFeedBridge.FollowResults followResults =
                new WebFeedBridge.FollowResults(WebFeedSubscriptionRequestStatus.SUCCESS,
                        new WebFeedBridge.WebFeedMetadata(sFollowId, sTitle, sTestUrl,
                                WebFeedSubscriptionRequestStatus.SUCCESS, /*isActive=*/
                                false, /*isRecommended=*/false));

        mWebFeedSnackbarController.showPostFollowHelp(followResults, sFollowId, sTestUrl, sTitle);

        View currentDialog =
                mDialogManager.getCurrentDialogForTest().get(ModalDialogProperties.CUSTOM_VIEW);
        verify(mSnackbarManager, times(0)).showSnackbar(any());
        assertTrue("Dialog should be showing.", mDialogManager.isShowing());
        assertEquals("Dialog title should be correct for inactive follow.",
                mContext.getString(R.string.web_feed_post_follow_dialog_title, sTitle),
                ((TextView) currentDialog.findViewById(R.id.web_feed_dialog_title)).getText());
        assertEquals("Dialog details should be for inactive follow.",
                mContext.getString(
                        R.string.web_feed_post_follow_dialog_stories_not_ready_description, sTitle),
                ((TextView) currentDialog.findViewById(R.id.web_feed_dialog_details)).getText());
    }

    @Test
    @SmallTest
    public void showPromoDialogForFollow_successful_noMetadata() {
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE))
                .thenReturn(true);
        WebFeedBridge.FollowResults followResults = new WebFeedBridge.FollowResults(
                WebFeedSubscriptionRequestStatus.SUCCESS, /*metadata=*/null);

        mWebFeedSnackbarController.showPostFollowHelp(followResults, sFollowId, sTestUrl, sTitle);

        View currentDialog =
                mDialogManager.getCurrentDialogForTest().get(ModalDialogProperties.CUSTOM_VIEW);
        verify(mSnackbarManager, times(0)).showSnackbar(any());
        assertTrue("Dialog should be showing.", mDialogManager.isShowing());
        assertEquals("Dialog title should be correct.",
                mContext.getString(R.string.web_feed_post_follow_dialog_title, sTitle),
                ((TextView) currentDialog.findViewById(R.id.web_feed_dialog_title)).getText());
        assertEquals("Dialog details should be for active follow.",
                mContext.getString(
                        R.string.web_feed_post_follow_dialog_stories_not_ready_description, sTitle),
                ((TextView) currentDialog.findViewById(R.id.web_feed_dialog_details)).getText());
    }

    @Test
    @SmallTest
    public void showSnackbarForFollow_noId_unsuccessful() {
        WebFeedBridge.FollowResults followResults = new WebFeedBridge.FollowResults(/*success=*/
                false);

        mWebFeedSnackbarController.showPostFollowHelp(
                followResults, "".getBytes(), sTestUrl, sTitle);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals("Snackbar should be for unsuccessful follow.",
                Snackbar.UMA_WEB_FEED_FOLLOW_FAILURE, snackbar.getIdentifierForTesting());
        assertEquals("Snackbar message should be for unsuccessful follow.",
                mContext.getString(R.string.web_feed_follow_generic_failure_snackbar_message),
                snackbar.getTextForTesting());

        // Click follow try again button.
        snackbar.getController().onAction(null);
        verify(mWebFeedBridge,
                description("FollowFromUrl should be called on follow try again when ID is not "
                        + "available."))
                .followFromUrl(eq(null), eq(sTestUrl), any());
    }

    @Test
    @SmallTest
    public void showSnackbarForFollow_withId_unsuccessful() {
        WebFeedBridge.FollowResults followResults = new WebFeedBridge.FollowResults(/*success=*/
                false);

        mWebFeedSnackbarController.showPostFollowHelp(followResults, sFollowId, sTestUrl, sTitle);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals("Snackbar should be for unsuccessful follow.",
                Snackbar.UMA_WEB_FEED_FOLLOW_FAILURE, snackbar.getIdentifierForTesting());
        assertEquals("Snackbar message should be for unsuccessful follow.",
                mContext.getString(R.string.web_feed_follow_generic_failure_snackbar_message),
                snackbar.getTextForTesting());

        // Click follow try again button.
        snackbar.getController().onAction(null);
        verify(mWebFeedBridge,
                description(
                        "FollowFromId should be called on follow try again when ID is available."))
                .followFromId(eq(sFollowId), any());
    }

    @Test
    @SmallTest
    public void showSnackbarForUnfollow_successful() {
        mWebFeedSnackbarController.showSnackbarForUnfollow(/*successfulUnfollow=*/
                true, sFollowId, sTestUrl, sTitle);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals("Snackbar should be for successful unfollow.",
                Snackbar.UMA_WEB_FEED_UNFOLLOW_SUCCESS, snackbar.getIdentifierForTesting());

        // Click refollow button.
        snackbar.getController().onAction(null);
        verify(mWebFeedBridge, description("Follow should be called on refollow."))
                .followFromId(eq(sFollowId), any());
    }

    @Test
    @SmallTest
    public void showSnackbarForUnfollow_unsuccessful() {
        mWebFeedSnackbarController.showSnackbarForUnfollow(/*successfulUnfollow=*/
                false, sFollowId, sTestUrl, sTitle);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals("Snackbar should be for unsuccessful unfollow.",
                Snackbar.UMA_WEB_FEED_UNFOLLOW_FAILURE, snackbar.getIdentifierForTesting());

        // Click unfollow try again button.
        snackbar.getController().onAction(null);
        verify(mWebFeedBridge, description("Unfollow should be called on unfollow try again."))
                .unfollow(eq(sFollowId), any());
    }

    @Test
    @SmallTest
    public void showSnackbarForUnfollow_correctDuration() {
        mWebFeedSnackbarController.showSnackbarForUnfollow(/*successfulUnfollow=*/
                true, sFollowId, sTestUrl, sTitle);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals("Snackbar duration for unfollow should be correct.",
                WebFeedSnackbarController.SNACKBAR_DURATION_MS, snackbar.getDuration());
    }

    private WebFeedBridge.FollowResults getSuccessfulFollowResult() {
        return new WebFeedBridge.FollowResults(WebFeedSubscriptionRequestStatus.SUCCESS,
                new WebFeedBridge.WebFeedMetadata("id1".getBytes(), "Title1", sTestUrl,
                        WebFeedSubscriptionStatus.SUBSCRIBED, /*isActive=*/true,
                        /*isRecommended=*/true));
    }
}
