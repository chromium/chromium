// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController.FeedLauncher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.url.GURL;

/**
 * Tests {@link WebFeedSnackbarController}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class WebFeedSnackbarControllerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock
    private FeedLauncher mFeedLauncher;
    @Mock
    private Tracker mTracker;
    @Mock
    public WebFeedBridge mWebFeedBridge;

    private static final GURL sTestUrl = new GURL("http://www.example.com");
    private static final String sTitle = "Example Title";
    private static final byte[] sFollowId = new byte[] {1, 2, 3};

    private Context mContext;
    private ModalDialogManager mDialogManager;
    private SnackbarManager mSnackbarManager;
    private WebFeedSnackbarController mWebFeedSnackbarController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        mContext = mActivityTestRule.getActivity();
        mDialogManager = mActivityTestRule.getActivity().getModalDialogManager();
        mSnackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.IPH_WEB_FEED_POST_FOLLOW_DIALOG_FEATURE))
                .thenReturn(false);
        TrackerFactory.setTrackerForTests(mTracker);

        mWebFeedSnackbarController = new WebFeedSnackbarController(mActivityTestRule.getActivity(),
                mFeedLauncher, mDialogManager, mSnackbarManager, mWebFeedBridge);
    }

    @After
    public void tearDown() {
        TrackerFactory.setTrackerForTests(null);
    }

    @Test
    @SmallTest
    public void showSnackbarForFollow_successful_withMetadata() {
        WebFeedBridge.FollowResults followResults = new WebFeedBridge.FollowResults(/*success=*/
                true);

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mWebFeedSnackbarController.showPostFollowHelp(
                                followResults, sFollowId, sTestUrl, sTitle));

        Snackbar snackbar = mSnackbarManager.getCurrentSnackbarForTesting();
        assertTrue("Snackbar should be showing.", mSnackbarManager.isShowing());
        assertEquals("Snackbar should be for successful follow.",
                Snackbar.UMA_WEB_FEED_FOLLOW_SUCCESS, snackbar.getIdentifierForTesting());
        assertEquals("Snackbar message should be for successful follow with title from metadata.",
                mContext.getString(R.string.web_feed_follow_success_snackbar_message,
                        followResults.metadata.title),
                snackbar.getTextForTesting());
    }

    @Test
    @SmallTest
    public void showSnackbarForFollow_successful_noMetadata() {
        WebFeedBridge.FollowResults followResults = new WebFeedBridge.FollowResults(
                WebFeedSubscriptionRequestStatus.SUCCESS, /*metadata=*/null);

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mWebFeedSnackbarController.showPostFollowHelp(
                                followResults, sFollowId, sTestUrl, sTitle));

        Snackbar snackbar = mSnackbarManager.getCurrentSnackbarForTesting();
        assertTrue("Snackbar should be showing.", mSnackbarManager.isShowing());
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

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mWebFeedSnackbarController.showPostFollowHelp(
                                followResults, sFollowId, sTestUrl, sTitle));

        Snackbar snackbar = mSnackbarManager.getCurrentSnackbarForTesting();
        assertEquals("Snackbar duration for follow should be correct.",
                WebFeedSnackbarController.SNACKBAR_DURATION_MS, snackbar.getDuration());
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

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mWebFeedSnackbarController.showPostFollowHelp(
                                followResults, sFollowId, sTestUrl, sTitle));

        View currentDialog =
                mDialogManager.getCurrentDialogForTest().get(ModalDialogProperties.CUSTOM_VIEW);
        assertFalse("Snackbar should not be showing.", mSnackbarManager.isShowing());
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

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mWebFeedSnackbarController.showPostFollowHelp(
                                followResults, sFollowId, sTestUrl, sTitle));

        View currentDialog =
                mDialogManager.getCurrentDialogForTest().get(ModalDialogProperties.CUSTOM_VIEW);
        assertFalse("Snackbar should not be showing.", mSnackbarManager.isShowing());
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

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mWebFeedSnackbarController.showPostFollowHelp(
                                followResults, sFollowId, sTestUrl, sTitle));

        View currentDialog =
                mDialogManager.getCurrentDialogForTest().get(ModalDialogProperties.CUSTOM_VIEW);
        assertFalse("Snackbar should not be showing.", mSnackbarManager.isShowing());
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

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mWebFeedSnackbarController.showPostFollowHelp(
                                followResults, "".getBytes(), sTestUrl, sTitle));

        Snackbar snackbar = mSnackbarManager.getCurrentSnackbarForTesting();
        assertTrue("Snackbar should be showing.", mSnackbarManager.isShowing());
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

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mWebFeedSnackbarController.showPostFollowHelp(
                                followResults, sFollowId, sTestUrl, sTitle));

        Snackbar snackbar = mSnackbarManager.getCurrentSnackbarForTesting();
        assertTrue("Snackbar should be showing.", mSnackbarManager.isShowing());
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
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mWebFeedSnackbarController
                                   .showSnackbarForUnfollow(/*successfulUnfollow=*/
                                           true, sFollowId, sTestUrl, sTitle));

        Snackbar snackbar = mSnackbarManager.getCurrentSnackbarForTesting();
        assertTrue("Snackbar should be showing.", mSnackbarManager.isShowing());
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
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mWebFeedSnackbarController
                                   .showSnackbarForUnfollow(/*successfulUnfollow=*/
                                           false, sFollowId, sTestUrl, sTitle));

        Snackbar snackbar = mSnackbarManager.getCurrentSnackbarForTesting();
        assertTrue("Snackbar should be showing.", mSnackbarManager.isShowing());
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
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mWebFeedSnackbarController
                                   .showSnackbarForUnfollow(/*successfulUnfollow=*/
                                           true, sFollowId, sTestUrl, sTitle));

        Snackbar snackbar = mSnackbarManager.getCurrentSnackbarForTesting();
        assertEquals("Snackbar duration for unfollow should be correct.",
                WebFeedSnackbarController.SNACKBAR_DURATION_MS, snackbar.getDuration());
    }
}
