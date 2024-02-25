// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.FeedServiceBridgeJni;
import org.chromium.chrome.browser.feed.webfeed.TestWebFeedFaviconFetcher;
import org.chromium.chrome.browser.feed.webfeed.WebFeedAvailabilityStatus;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.WebFeedMetadata;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridgeJni;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionRequestStatus;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionStatus;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;

/** Tests {@link FollowManagementMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FollowManagementMediatorTest {
    private Activity mActivity;
    private ModelList mModelList;
    private FollowManagementMediator mFollowManagementMediator;

    static final byte[] ID1 = "ID1".getBytes(StandardCharsets.US_ASCII);
    static final byte[] ID2 = "ID2".getBytes(StandardCharsets.US_ASCII);
    static final GURL URL1 = JUnitTestGURLs.URL_1;
    static final GURL FAVICON1 = JUnitTestGURLs.RED_1;
    static final GURL URL2 = JUnitTestGURLs.URL_2;
    static final GURL FAVICON2 = JUnitTestGURLs.RED_2;

    @Captor ArgumentCaptor<Callback<WebFeedBridge.UnfollowResults>> mUnfollowCallbackCaptor;

    @Captor ArgumentCaptor<Callback<WebFeedBridge.FollowResults>> mFollowCallbackCaptor;

    @Rule public JniMocker mocker = new JniMocker();

    @Mock WebFeedBridge.Natives mWebFeedBridgeJni;

    @Mock private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock private FollowManagementMediator.Observer mObserver;

    TestWebFeedFaviconFetcher mFaviconFetcher = new TestWebFeedFaviconFetcher();

    @Before
    public void setUpTest() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mModelList = new ModelList();
        MockitoAnnotations.initMocks(this);
        mocker.mock(WebFeedBridgeJni.TEST_HOOKS, mWebFeedBridgeJni);
        mocker.mock(FeedServiceBridgeJni.TEST_HOOKS, mFeedServiceBridgeJniMock);

        mFollowManagementMediator =
                new FollowManagementMediator(mActivity, mModelList, mObserver, mFaviconFetcher);

        // WebFeedBridge.refreshFollowedWebFeeds() gets called once with non-null pointer to a
        // callback.
        verify(mWebFeedBridgeJni).refreshSubscriptions(notNull());
    }

    @Test
    public void testLoadingState() {
        // Loading state is set upon construction.
        assertEquals("<loading>", modelListToString());
    }

    @Test
    public void testEmptyWebFeedList() {
        mFollowManagementMediator.fillRecyclerView(new ArrayList<WebFeedMetadata>());

        assertEquals("<empty>", modelListToString());
    }

    @Test
    public void testWebFeedList() {
        mFollowManagementMediator.fillRecyclerView(
                Arrays.asList(
                        new WebFeedMetadata[] {
                            new WebFeedMetadata(
                                    ID1,
                                    "Title1",
                                    URL1,
                                    WebFeedSubscriptionStatus.SUBSCRIBED,
                                    WebFeedAvailabilityStatus.ACTIVE,
                                    false,
                                    FAVICON1),
                            new WebFeedMetadata(
                                    ID2,
                                    "Title2",
                                    URL2,
                                    WebFeedSubscriptionStatus.NOT_SUBSCRIBED,
                                    WebFeedAvailabilityStatus.INACTIVE,
                                    false,
                                    FAVICON2)
                        }));

        assertEquals(
                "ID1 title=Title1 url=https://www.one.com/ subscribed\n"
                        + "ID2 title=Title2 url=https://www.two.com/ status=Updates Unavailable"
                        + " not-subscribed",
                modelListToString());
    }

    @Test
    public void testUnsubscribeInProgressAfterClick() {
        mFollowManagementMediator.fillRecyclerView(
                Arrays.asList(
                        new WebFeedMetadata[] {
                            new WebFeedMetadata(
                                    ID1,
                                    "Title1",
                                    URL1,
                                    WebFeedSubscriptionStatus.SUBSCRIBED,
                                    WebFeedAvailabilityStatus.ACTIVE,
                                    false,
                                    FAVICON1),
                            new WebFeedMetadata(
                                    ID2,
                                    "Title2",
                                    URL2,
                                    WebFeedSubscriptionStatus.NOT_SUBSCRIBED,
                                    WebFeedAvailabilityStatus.ACTIVE,
                                    false,
                                    FAVICON2)
                        }));

        mFollowManagementMediator.clickHandler(mModelList.get(0).model);

        assertEquals(
                "ID1 title=Title1 url=https://www.one.com/ not-subscribed disabled\n"
                        + "ID2 title=Title2 url=https://www.two.com/ not-subscribed",
                modelListToString());
    }

    @Test
    public void testUnsubscribeFailure() {
        mFollowManagementMediator.fillRecyclerView(
                Arrays.asList(
                        new WebFeedMetadata[] {
                            new WebFeedMetadata(
                                    ID1,
                                    "Title1",
                                    URL1,
                                    WebFeedSubscriptionStatus.SUBSCRIBED,
                                    WebFeedAvailabilityStatus.ACTIVE,
                                    false,
                                    FAVICON1)
                        }));

        mFollowManagementMediator.clickHandler(mModelList.get(0).model);

        verify(mWebFeedBridgeJni)
                .unfollowWebFeed(
                        eq(ID1),
                        /* isDurable= */ eq(false),
                        eq(WebFeedBridge.CHANGE_REASON_MANAGEMENT),
                        mUnfollowCallbackCaptor.capture());
        mUnfollowCallbackCaptor
                .getValue()
                .onResult(
                        new WebFeedBridge.UnfollowResults(
                                WebFeedSubscriptionRequestStatus.FAILED_OFFLINE));

        assertEquals("ID1 title=Title1 url=https://www.one.com/ subscribed", modelListToString());
        verify(mObserver, times(1)).networkConnectionError();
    }

    @Test
    public void testUnsubscribeSuccess() {
        mFollowManagementMediator.fillRecyclerView(
                Arrays.asList(
                        new WebFeedMetadata[] {
                            new WebFeedMetadata(
                                    ID1,
                                    "Title1",
                                    URL1,
                                    WebFeedSubscriptionStatus.SUBSCRIBED,
                                    WebFeedAvailabilityStatus.ACTIVE,
                                    false,
                                    FAVICON1)
                        }));

        mFollowManagementMediator.clickHandler(mModelList.get(0).model);

        verify(mWebFeedBridgeJni)
                .unfollowWebFeed(
                        eq(ID1),
                        /* isDurable= */ eq(false),
                        eq(WebFeedBridge.CHANGE_REASON_MANAGEMENT),
                        mUnfollowCallbackCaptor.capture());
        mUnfollowCallbackCaptor
                .getValue()
                .onResult(
                        new WebFeedBridge.UnfollowResults(
                                WebFeedSubscriptionRequestStatus.SUCCESS));

        assertEquals(
                "ID1 title=Title1 url=https://www.one.com/ not-subscribed", modelListToString());
    }

    @Test
    public void testSubscribeInProgress() {
        mFollowManagementMediator.fillRecyclerView(
                Arrays.asList(
                        new WebFeedMetadata[] {
                            new WebFeedMetadata(
                                    ID1,
                                    "Title1",
                                    URL1,
                                    WebFeedSubscriptionStatus.NOT_SUBSCRIBED,
                                    WebFeedAvailabilityStatus.ACTIVE,
                                    false,
                                    FAVICON1)
                        }));

        mFollowManagementMediator.clickHandler(mModelList.get(0).model);

        assertEquals(
                "ID1 title=Title1 url=https://www.one.com/ subscribed disabled",
                modelListToString());
    }

    @Test
    public void testSubscribeFailure() {
        mFollowManagementMediator.fillRecyclerView(
                Arrays.asList(
                        new WebFeedMetadata[] {
                            new WebFeedMetadata(
                                    ID1,
                                    "Title1",
                                    URL1,
                                    WebFeedSubscriptionStatus.NOT_SUBSCRIBED,
                                    WebFeedAvailabilityStatus.ACTIVE,
                                    false,
                                    FAVICON1)
                        }));

        mFollowManagementMediator.clickHandler(mModelList.get(0).model);

        verify(mWebFeedBridgeJni)
                .followWebFeedById(
                        eq(ID1),
                        /* isDurable= */ eq(false),
                        eq(WebFeedBridge.CHANGE_REASON_MANAGEMENT),
                        mFollowCallbackCaptor.capture());
        mFollowCallbackCaptor
                .getValue()
                .onResult(
                        new WebFeedBridge.FollowResults(
                                WebFeedSubscriptionRequestStatus.FAILED_UNKNOWN_ERROR, null));

        assertEquals(
                "ID1 title=Title1 url=https://www.one.com/ not-subscribed", modelListToString());
        verify(mObserver, times(1)).otherOperationError();
    }

    @Test
    public void testSubscribeSuccess() {
        mFollowManagementMediator.fillRecyclerView(
                Arrays.asList(
                        new WebFeedMetadata[] {
                            new WebFeedMetadata(
                                    ID1,
                                    "Title1",
                                    URL1,
                                    WebFeedSubscriptionStatus.NOT_SUBSCRIBED,
                                    WebFeedAvailabilityStatus.ACTIVE,
                                    false,
                                    FAVICON1)
                        }));

        mFollowManagementMediator.clickHandler(mModelList.get(0).model);

        verify(mWebFeedBridgeJni)
                .followWebFeedById(
                        eq(ID1),
                        /* isDurable= */ eq(false),
                        eq(WebFeedBridge.CHANGE_REASON_MANAGEMENT),
                        mFollowCallbackCaptor.capture());
        mFollowCallbackCaptor
                .getValue()
                .onResult(
                        new WebFeedBridge.FollowResults(
                                WebFeedSubscriptionRequestStatus.SUCCESS, null));

        assertEquals("ID1 title=Title1 url=https://www.one.com/ subscribed", modelListToString());
    }

    private String modelListToString() {
        ModelList modelList = mModelList;
        StringBuilder builder = new StringBuilder();
        for (int i = 0; i < modelList.size(); i++) {
            int type = modelList.get(i).type;
            if (type == FollowManagementItemProperties.EMPTY_ITEM_TYPE) {
                builder.append("<empty>");
            } else if (type == FollowManagementItemProperties.LOADING_ITEM_TYPE) {
                builder.append("<loading>");
            } else {
                assertEquals(FollowManagementItemProperties.DEFAULT_ITEM_TYPE, type);
                builder.append(modelToString(modelList.get(i).model));
            }
            if (i + 1 < modelList.size()) {
                builder.append("\n");
            }
        }
        return builder.toString();
    }

    private String modelToString(PropertyModel itemModel) {
        StringBuilder builder = new StringBuilder();
        builder.append(new String(itemModel.get(FollowManagementItemProperties.ID_KEY)));
        builder.append(" title=");
        builder.append(itemModel.get(FollowManagementItemProperties.TITLE_KEY));
        builder.append(" url=");
        builder.append(itemModel.get(FollowManagementItemProperties.URL_KEY));
        String statusString = itemModel.get(FollowManagementItemProperties.STATUS_KEY);
        if (!statusString.isEmpty()) {
            builder.append(" status=");
            builder.append(statusString);
        }
        builder.append(" ");
        if (itemModel.get(FollowManagementItemProperties.SUBSCRIBED_KEY)) {
            builder.append("subscribed");
        } else {
            builder.append("not-subscribed");
        }
        if (!itemModel.get(FollowManagementItemProperties.CHECKBOX_ENABLED_KEY)) {
            builder.append(" disabled");
        }
        return builder.toString();
    }

    // TODO(harringtond): Test favicons.
}
