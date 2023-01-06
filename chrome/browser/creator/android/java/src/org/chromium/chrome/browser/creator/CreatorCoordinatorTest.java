// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

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
import org.chromium.chrome.browser.creator.test.R;
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
import org.chromium.ui.widget.ButtonCompat;

/**
 * Tests for {@link CreatorCoordinator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class CreatorCoordinatorTest {
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

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    private final byte[] mWebFeedIdDefault = "webFeedId".getBytes();
    private final String mTitleDefault = "Example";
    private final String mUrlDefault = "example.com";
    private TestActivity mActivity;

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
    }

    private CreatorCoordinator newCreatorCoordinator(String title, String url, byte[] webFeedId) {
        return new CreatorCoordinator(mActivity, webFeedId, mSnackbarManager, mWindowAndroid,
                mProfile, title, url, mCreatorWebContents, mCreatorOpenTab, mShareDelegateSupplier);
    }

    @Test
    public void testCreatorCoordinatorConstruction() {
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(mTitleDefault, mUrlDefault, mWebFeedIdDefault);
        assertNotNull("Could not construct CreatorCoordinator", creatorCoordinator);
    }

    @Test
    public void testActionBar() {
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(mTitleDefault, mUrlDefault, mWebFeedIdDefault);
        View outerView = creatorCoordinator.getView();
        ViewGroup actionBar = (ViewGroup) outerView.findViewById(R.id.action_bar);
        assertNotNull("Could not retrieve ActionBar", actionBar);
    }

    @Test
    public void testCreatorModel_Creation() {
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(mTitleDefault, mUrlDefault, mWebFeedIdDefault);
        PropertyModel creatorModel = creatorCoordinator.getCreatorModel();
        assertNotNull("Could not retrieve CreatorModel", creatorModel);
    }

    @Test
    public void testCreatorModel_DefaultTitle() {
        String creatorTitle = "creatorTitle";
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(creatorTitle, mUrlDefault, mWebFeedIdDefault);
        PropertyModel creatorModel = creatorCoordinator.getCreatorModel();
        String modelTitle = creatorModel.get(CreatorProperties.TITLE_KEY);
        assertEquals(creatorTitle, modelTitle);

        View creatorProfileView = creatorCoordinator.getProfileView();
        TextView profileTitleView = creatorProfileView.findViewById(R.id.creator_name);
        assertEquals(creatorTitle, profileTitleView.getText());

        View creatorView = creatorCoordinator.getView();
        TextView toolbarTitleView = creatorView.findViewById(R.id.creator_title_toolbar);
        assertEquals(creatorTitle, toolbarTitleView.getText());
    }

    @Test
    public void testCreatorModel_DefaultUrl() {
        String creatorUrl = "creatorUrl.com";
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(mTitleDefault, creatorUrl, mWebFeedIdDefault);
        PropertyModel creatorModel = creatorCoordinator.getCreatorModel();
        String modelUrl = creatorModel.get(CreatorProperties.URL_KEY);
        assertEquals(creatorUrl, modelUrl);

        View creatorProfileView = creatorCoordinator.getProfileView();
        TextView urlView = creatorProfileView.findViewById(R.id.creator_url);
        assertEquals(creatorUrl, urlView.getText());
    }

    @Test
    public void testCreatorModel_DefaultWebFeedId() {
        byte[] creatorWebFeedId = "creatorWebFeedId".getBytes();
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(mTitleDefault, mUrlDefault, creatorWebFeedId);
        PropertyModel creatorModel = creatorCoordinator.getCreatorModel();
        byte[] modelWebFeedId = creatorModel.get(CreatorProperties.WEB_FEED_ID_KEY);
        assertEquals(creatorWebFeedId, modelWebFeedId);
    }

    @Test
    public void testCreatorModel_NewTitle() {
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(mTitleDefault, mUrlDefault, mWebFeedIdDefault);
        String newTitle = "creatorTitle 2.0";
        PropertyModel creatorModel = creatorCoordinator.getCreatorModel();
        creatorModel.set(CreatorProperties.TITLE_KEY, newTitle);
        String title = creatorModel.get(CreatorProperties.TITLE_KEY);
        assertEquals(newTitle, title);

        View creatorProfileView = creatorCoordinator.getProfileView();
        TextView profileTitleView = creatorProfileView.findViewById(R.id.creator_name);
        assertEquals(newTitle, profileTitleView.getText());

        View creatorView = creatorCoordinator.getView();
        TextView toolbarTitleView = creatorView.findViewById(R.id.creator_title_toolbar);
        assertEquals(newTitle, toolbarTitleView.getText());
    }

    @Test
    public void testCreatorModel_NewUrl() {
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(mTitleDefault, mUrlDefault, mWebFeedIdDefault);
        String newUrl = "newCreatorUrl.com";
        PropertyModel creatorModel = creatorCoordinator.getCreatorModel();
        creatorModel.set(CreatorProperties.URL_KEY, newUrl);
        String url = creatorModel.get(CreatorProperties.URL_KEY);
        assertEquals(newUrl, url);

        View creatorProfileView = creatorCoordinator.getProfileView();
        TextView urlView = creatorProfileView.findViewById(R.id.creator_url);
        assertEquals(newUrl, urlView.getText());
    }

    @Test
    public void testCreatorModel_ToolbarVisibility() {
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(mTitleDefault, mUrlDefault, mWebFeedIdDefault);
        PropertyModel creatorModel = creatorCoordinator.getCreatorModel();
        View creatorView = creatorCoordinator.getView();
        FrameLayout mButtonsContainer = creatorView.findViewById(R.id.creator_all_buttons_toolbar);
        assertEquals(mButtonsContainer.getVisibility(), View.GONE);

        creatorModel.set(CreatorProperties.IS_TOOLBAR_VISIBLE_KEY, true);
        assertEquals(mButtonsContainer.getVisibility(), View.VISIBLE);
    }

    @Test
    public void testCreatorModel_IsFollowedStatus() {
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(mTitleDefault, mUrlDefault, mWebFeedIdDefault);
        PropertyModel creatorModel = creatorCoordinator.getCreatorModel();
        View creatorProfileView = creatorCoordinator.getProfileView();
        ButtonCompat followButton = creatorProfileView.findViewById(R.id.creator_follow_button);
        ButtonCompat followingButton =
                creatorProfileView.findViewById(R.id.creator_following_button);

        creatorModel.set(CreatorProperties.IS_FOLLOWED_KEY, false);
        assertEquals(followButton.getVisibility(), View.VISIBLE);
        assertEquals(followingButton.getVisibility(), View.GONE);

        creatorModel.set(CreatorProperties.IS_FOLLOWED_KEY, true);
        assertEquals(followButton.getVisibility(), View.GONE);
        assertEquals(followingButton.getVisibility(), View.VISIBLE);
    }
}
