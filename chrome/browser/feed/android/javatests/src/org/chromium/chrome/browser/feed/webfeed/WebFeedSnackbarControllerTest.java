// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
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
    public WebFeedBridge mWebFeedBridge;

    private static final GURL sTestUrl = new GURL("http://www.example.com");
    private static final String sTitle = "Example Title";
    private static final byte[] sFollowId = new byte[] {1, 2, 3};

    private Context mContext;
    private SnackbarManager mSnackbarManager;
    private WebFeedSnackbarController mWebFeedSnackbarController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        mContext = mActivityTestRule.getActivity();
        mSnackbarManager = mActivityTestRule.getActivity().getSnackbarManager();

        mWebFeedSnackbarController = new WebFeedSnackbarController(
                mActivityTestRule.getActivity(), mSnackbarManager, mWebFeedBridge);
    }

    @Test
    @SmallTest
    public void showSnackbarForFollow_successful_withMetadata() {
        WebFeedBridge.FollowResults followResults = new WebFeedBridge.FollowResults(/*success=*/
                true);

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mWebFeedSnackbarController.showSnackbarForFollow(
                                followResults, sTestUrl, sTitle));

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
                        -> mWebFeedSnackbarController.showSnackbarForFollow(
                                followResults, sTestUrl, sTitle));

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
    public void showSnackbarForFollow_unsuccessful() {
        WebFeedBridge.FollowResults followResults = new WebFeedBridge.FollowResults(/*success=*/
                false);

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mWebFeedSnackbarController.showSnackbarForFollow(
                                followResults, sTestUrl, sTitle));

        Snackbar snackbar = mSnackbarManager.getCurrentSnackbarForTesting();
        assertTrue("Snackbar should be showing.", mSnackbarManager.isShowing());
        assertEquals("Snackbar should be for unsuccessful follow.",
                Snackbar.UMA_WEB_FEED_FOLLOW_FAILURE, snackbar.getIdentifierForTesting());
        assertEquals("Snackbar message should be for unsuccessful follow.",
                mContext.getString(R.string.web_feed_follow_generic_failure_snackbar_message),
                snackbar.getTextForTesting());

        // Click follow try again button.
        snackbar.getController().onAction(null);
        verify(mWebFeedBridge, description("Follow should be called on follow try again."))
                .followFromUrl(eq(sTestUrl), any());
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
                .followFromUrl(eq(sTestUrl), any());
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
}
