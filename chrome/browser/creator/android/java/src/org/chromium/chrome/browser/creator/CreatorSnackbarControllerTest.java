// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionRequestStatus.FAILED_OFFLINE;
import static org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionRequestStatus.FAILED_UNKNOWN_ERROR;
import static org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionRequestStatus.SUCCESS;

import android.app.Activity;
import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.creator.test.R;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

/** Tests for {@link CreatorSnackbarController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CreatorSnackbarControllerTest {
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private WebFeedBridge.Natives mWebFeedBridgeJniMock;
    @Mock private SnackbarManager mSnackbarManager;

    @Captor private ArgumentCaptor<Snackbar> mSnackbarCaptor;

    private Context mContext;
    private String mTitle;
    private CreatorSnackbarController mCreatorSnackbarController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(WebFeedBridge.getTestHooksForTesting(), mWebFeedBridgeJniMock);
        mContext = Robolectric.setupActivity(Activity.class);
        mTitle = "Example Title";

        mCreatorSnackbarController = new CreatorSnackbarController(mContext, mSnackbarManager);
    }

    @Test
    public void showSnackbarForFollow_successful() {
        mCreatorSnackbarController.showSnackbarForFollow(SUCCESS, mTitle);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Snackbar should be for successful follow.",
                Snackbar.UMA_CREATOR_FOLLOW_SUCCESS,
                snackbar.getIdentifierForTesting());
        assertEquals(
                "Snackbar message should be for successful follow with correct title.",
                mContext.getString(R.string.cormorant_creator_follow_success_snackbar, mTitle),
                snackbar.getTextForTesting());
    }

    @Test
    public void showSnackbarForFollow_failure_offline() {
        mCreatorSnackbarController.showSnackbarForFollow(FAILED_OFFLINE, mTitle);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Snackbar should be for failed follow.",
                Snackbar.UMA_CREATOR_FOLLOW_FAILURE,
                snackbar.getIdentifierForTesting());
        assertEquals(
                "Snackbar message should be for offline status.",
                mContext.getString(R.string.cormorant_creator_offline_failure_snackbar),
                snackbar.getTextForTesting());
    }

    @Test
    public void showSnackbarForFollow_failure_general() {
        mCreatorSnackbarController.showSnackbarForFollow(FAILED_UNKNOWN_ERROR, mTitle);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Snackbar should be for failed follow.",
                Snackbar.UMA_CREATOR_FOLLOW_FAILURE,
                snackbar.getIdentifierForTesting());
        assertEquals(
                "Snackbar message should be for failed follow.",
                mContext.getString(R.string.cormorant_creator_follow_failure_snackbar),
                snackbar.getTextForTesting());
    }

    @Test
    public void showSnackbarForUnfollow_successful() {
        mCreatorSnackbarController.showSnackbarForUnfollow(SUCCESS, mTitle);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Snackbar should be for successful unfollow.",
                Snackbar.UMA_CREATOR_UNFOLLOW_SUCCESS,
                snackbar.getIdentifierForTesting());
        assertEquals(
                "Snackbar message should be for successful unfollow with correct title.",
                mContext.getString(R.string.cormorant_creator_unfollow_success_snackbar, mTitle),
                snackbar.getTextForTesting());
    }

    @Test
    public void showSnackbarForUnfollow_failure_offline() {
        mCreatorSnackbarController.showSnackbarForUnfollow(FAILED_OFFLINE, mTitle);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Snackbar should be for failed unfollow.",
                Snackbar.UMA_CREATOR_UNFOLLOW_FAILURE,
                snackbar.getIdentifierForTesting());
        assertEquals(
                "Snackbar message should be for offline status.",
                mContext.getString(R.string.cormorant_creator_offline_failure_snackbar),
                snackbar.getTextForTesting());
    }

    @Test
    public void showSnackbarForUnfollow_failure_general() {
        mCreatorSnackbarController.showSnackbarForUnfollow(FAILED_UNKNOWN_ERROR, mTitle);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "Snackbar should be for failed unfollow.",
                Snackbar.UMA_CREATOR_UNFOLLOW_FAILURE,
                snackbar.getIdentifierForTesting());
        assertEquals(
                "Snackbar message should be for failed unfollow.",
                mContext.getString(R.string.cormorant_creator_unfollow_failure_snackbar),
                snackbar.getTextForTesting());
    }
}
