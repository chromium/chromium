// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

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
import org.chromium.chrome.browser.creator.CreatorCoordinator.ContentChangedListener;
import org.chromium.chrome.browser.creator.test.R;
import org.chromium.chrome.browser.feed.FeedActionDelegate;
import org.chromium.chrome.browser.feed.FeedListContentManager.ExternalViewContent;
import org.chromium.chrome.browser.feed.FeedListContentManager.FeedContent;
import org.chromium.chrome.browser.feed.FeedListContentManager.NativeViewContent;
import org.chromium.chrome.browser.feed.FeedReliabilityLoggingBridge;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.FeedServiceBridgeJni;
import org.chromium.chrome.browser.feed.FeedStream;
import org.chromium.chrome.browser.feed.FeedSurfaceRendererBridge;
import org.chromium.chrome.browser.feed.FeedSurfaceRendererBridgeJni;
import org.chromium.chrome.browser.feed.SingleWebFeedEntryPoint;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link CreatorCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CreatorCoordinatorTest {
    @Mock private WebFeedBridge.Natives mWebFeedBridgeJniMock;
    @Mock private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock private FeedSurfaceRendererBridge.Natives mFeedSurfaceRendererBridgeJniMock;
    @Mock private FeedReliabilityLoggingBridge.Natives mFeedReliabilityLoggingBridgeJniMock;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Profile mProfile;
    @Mock private WebContentsCreator mCreatorWebContents;
    @Mock private NewTabCreator mCreatorOpenTab;
    @Mock private UnownedUserDataSupplier<ShareDelegate> mShareDelegateSupplier;
    @Mock private FeedStream mStreamMock;
    @Mock private SignInInterstitialInitiator mSignInInterstitialInitiator;
    @Mock private FeedActionDelegate mFeedActionDelegate;

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public JniMocker mJniMocker = new JniMocker();

    private static final GURL DEFAULT_URL = JUnitTestGURLs.EXAMPLE_URL;
    private static final GURL TEST_URL = JUnitTestGURLs.URL_1;

    private final byte[] mWebFeedIdDefault = "webFeedId".getBytes();
    private final boolean mFollowingDefault = false;
    private final int mEntryPointDefault = SingleWebFeedEntryPoint.OTHER;
    private TestActivity mActivity;

    @Before
    public void setUpTest() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(FeedServiceBridgeJni.TEST_HOOKS, mFeedServiceBridgeJniMock);
        mJniMocker.mock(FeedSurfaceRendererBridgeJni.TEST_HOOKS, mFeedSurfaceRendererBridgeJniMock);
        mJniMocker.mock(WebFeedBridge.getTestHooksForTesting(), mWebFeedBridgeJniMock);
        mJniMocker.mock(
                FeedReliabilityLoggingBridge.getTestHooksForTesting(),
                mFeedReliabilityLoggingBridgeJniMock);

        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
    }

    private CreatorCoordinator newCreatorCoordinator(
            GURL url, byte[] webFeedId, int entryPoint, boolean following) {
        return new CreatorCoordinator(
                mActivity,
                webFeedId,
                mSnackbarManager,
                mWindowAndroid,
                mProfile,
                url == null ? null : url.getSpec(),
                mCreatorWebContents,
                mCreatorOpenTab,
                mShareDelegateSupplier,
                entryPoint,
                following,
                mSignInInterstitialInitiator);
    }

    @Test
    public void testCreatorCoordinatorConstruction() {
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(
                        DEFAULT_URL, mWebFeedIdDefault, mEntryPointDefault, mFollowingDefault);
        assertNotNull("Could not construct CreatorCoordinator", creatorCoordinator);
    }

    @Test
    public void testActionBar() {
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(
                        DEFAULT_URL, mWebFeedIdDefault, mEntryPointDefault, mFollowingDefault);
        View outerView = creatorCoordinator.getView();
        ViewGroup actionBar = (ViewGroup) outerView.findViewById(R.id.action_bar);
        assertNotNull("Could not retrieve ActionBar", actionBar);
    }

    @Test
    public void testOnChangeListener_noError() {
        CreatorCoordinator coordinator =
                newCreatorCoordinator(
                        DEFAULT_URL, mWebFeedIdDefault, mEntryPointDefault, mFollowingDefault);
        coordinator.setStreamForTest(mStreamMock);
        ContentChangedListener listener = coordinator.new ContentChangedListener();

        List<FeedContent> contents = new ArrayList<>();
        contents.add(new NativeViewContent(0, "header", new View(mActivity)));
        contents.add(new ExternalViewContent("content", new byte[0], null));

        listener.onContentChanged(contents);

        verify(mStreamMock).removeOnContentChangedListener(listener);
        verify(mStreamMock).notifyNewHeaderCount(2);
    }

    @Test
    public void testOnChangeListener_error() {
        CreatorCoordinator coordinator =
                newCreatorCoordinator(
                        DEFAULT_URL, mWebFeedIdDefault, mEntryPointDefault, mFollowingDefault);
        coordinator.setStreamForTest(mStreamMock);
        ContentChangedListener listener = coordinator.new ContentChangedListener();

        List<FeedContent> contents = new ArrayList<>();
        contents.add(new NativeViewContent(0, "header", new View(mActivity)));
        contents.add(new NativeViewContent(0, "error", new View(mActivity)));

        // Set the title to avoid deleting the profile section.
        coordinator.getCreatorModel().set(CreatorProperties.TITLE_KEY, "creatorTitle");

        listener.onContentChanged(contents);

        verify(mStreamMock, never()).notifyNewHeaderCount(anyInt());
        verify(mStreamMock, never()).removeOnContentChangedListener(any());
    }

    @Test
    public void testOnChangeListener_RemoveHeader() {
        CreatorCoordinator coordinator =
                newCreatorCoordinator(
                        DEFAULT_URL, mWebFeedIdDefault, mEntryPointDefault, mFollowingDefault);
        coordinator.setStreamForTest(mStreamMock);
        ContentChangedListener listener = coordinator.new ContentChangedListener();

        List<FeedContent> contents = new ArrayList<>();
        contents.add(new NativeViewContent(0, "header", new View(mActivity)));
        contents.add(new NativeViewContent(0, "error", new View(mActivity)));

        // Make sure title is unavailable to test deletion of profile section.
        coordinator.getCreatorModel().set(CreatorProperties.TITLE_KEY, null);

        listener.onContentChanged(contents);

        verify(mStreamMock).notifyNewHeaderCount(0);
        verify(mStreamMock).removeOnContentChangedListener(listener);
    }

    @Test
    public void testCreatorModel_Creation() {
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(
                        DEFAULT_URL, mWebFeedIdDefault, mEntryPointDefault, mFollowingDefault);
        PropertyModel creatorModel = creatorCoordinator.getCreatorModel();
        assertNotNull("Could not retrieve CreatorModel", creatorModel);
    }

    @Test
    public void testCreatorModel_DefaultFollowing() {
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(DEFAULT_URL, mWebFeedIdDefault, mEntryPointDefault, true);
        PropertyModel creatorModel = creatorCoordinator.getCreatorModel();
        assertTrue(creatorModel.get(CreatorProperties.IS_FOLLOWED_KEY));

        View creatorProfileView = creatorCoordinator.getProfileView();
        ButtonCompat followButton = creatorProfileView.findViewById(R.id.creator_follow_button);
        ButtonCompat followingButton =
                creatorProfileView.findViewById(R.id.creator_following_button);

        assertEquals(followButton.getVisibility(), View.GONE);
        assertEquals(followingButton.getVisibility(), View.VISIBLE);
    }

    @Test
    public void testCreatorModel_DefaultUrl() {
        GURL creatorUrl = TEST_URL;
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(
                        creatorUrl, mWebFeedIdDefault, mEntryPointDefault, mFollowingDefault);
        PropertyModel creatorModel = creatorCoordinator.getCreatorModel();
        String modelUrl = creatorModel.get(CreatorProperties.URL_KEY);
        assertEquals(creatorUrl.getSpec(), modelUrl);

        View creatorProfileView = creatorCoordinator.getProfileView();
        TextView urlView = creatorProfileView.findViewById(R.id.creator_url);
        assertEquals(
                UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(creatorUrl),
                urlView.getText());
    }

    @Test
    public void testCreatorModel_DefaultWebFeedId() {
        byte[] creatorWebFeedId = "creatorWebFeedId".getBytes();
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(
                        DEFAULT_URL, creatorWebFeedId, mEntryPointDefault, mFollowingDefault);
        PropertyModel creatorModel = creatorCoordinator.getCreatorModel();
        byte[] modelWebFeedId = creatorModel.get(CreatorProperties.WEB_FEED_ID_KEY);
        assertEquals(creatorWebFeedId, modelWebFeedId);
    }

    @Test
    public void testCreatorModel_NewTitle() {
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(
                        DEFAULT_URL, mWebFeedIdDefault, mEntryPointDefault, mFollowingDefault);
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
                newCreatorCoordinator(
                        DEFAULT_URL, mWebFeedIdDefault, mEntryPointDefault, mFollowingDefault);
        String newUrl = TEST_URL.getSpec();
        PropertyModel creatorModel = creatorCoordinator.getCreatorModel();
        creatorModel.set(CreatorProperties.URL_KEY, newUrl);
        String url = creatorModel.get(CreatorProperties.URL_KEY);
        assertEquals(newUrl, url);

        View creatorProfileView = creatorCoordinator.getProfileView();
        TextView urlView = creatorProfileView.findViewById(R.id.creator_url);
        assertEquals(
                UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(DEFAULT_URL),
                urlView.getText());
    }

    @Test
    public void testCreatorModel_ToolbarVisibility() {
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(
                        DEFAULT_URL, mWebFeedIdDefault, mEntryPointDefault, mFollowingDefault);
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
                newCreatorCoordinator(
                        DEFAULT_URL, mWebFeedIdDefault, mEntryPointDefault, mFollowingDefault);
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

    @Test
    public void testCreatorCoordinator_QueryFeed_nullUrl() {
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(
                        null, mWebFeedIdDefault, mEntryPointDefault, mFollowingDefault);
        PropertyModel creatorModel = creatorCoordinator.getCreatorModel();
        creatorCoordinator.queryFeedStream(mFeedActionDelegate, mShareDelegateSupplier);
        verify(mWebFeedBridgeJniMock).queryWebFeedId(anyString(), any());
    }

    @Test
    public void testCreatorCoordinator_QueryFeed_nullWebFeedId() {
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(DEFAULT_URL, null, mEntryPointDefault, mFollowingDefault);
        PropertyModel creatorModel = creatorCoordinator.getCreatorModel();
        creatorCoordinator.queryFeedStream(mFeedActionDelegate, mShareDelegateSupplier);
        verify(mWebFeedBridgeJniMock).queryWebFeed(anyString(), any());
    }

    @Test
    public void testCreatorCoordinator_InitializeBottomSheetView() {
        CreatorCoordinator creatorCoordinator =
                newCreatorCoordinator(
                        DEFAULT_URL, mWebFeedIdDefault, mEntryPointDefault, mFollowingDefault);
        ViewGroup creatorViewGroup = creatorCoordinator.getView();
        assertEquals(creatorViewGroup.getChildCount(), 2);
        View contentPreviewsBottomSheet =
                creatorViewGroup.findViewById(R.id.creator_content_preview_bottom_sheet);
        assertNotNull(
                "Content Previews Bottom Sheet is not initialized", contentPreviewsBottomSheet);
    }
}
