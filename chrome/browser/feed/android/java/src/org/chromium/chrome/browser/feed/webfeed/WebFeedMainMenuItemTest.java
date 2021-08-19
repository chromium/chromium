// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController.FeedLauncher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.embedder_support.util.ShadowUrlUtilities;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.ChipView;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/**
 * Tests {@link WebFeedMainMenuItem}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowUrlUtilities.class})
@SmallTest
public final class WebFeedMainMenuItemTest {
    private static final GURL TEST_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    private Activity mActivity;
    @Mock
    private FeedLauncher mFeedLauncher;
    @Mock
    private Bitmap mBitmap;
    @Mock
    private AppMenuHandler mAppMenuHandler;
    @Mock
    private ModalDialogManager mDialogManager;
    @Mock
    private SnackbarManager mSnackBarManager;
    @Mock
    private Tab mTab;
    @Mock
    public WebFeedBridge.Natives mWebFeedBridgeJniMock;
    @Mock
    public UrlFormatter.Natives mUrlFormatterJniMock;

    private WebFeedMainMenuItem mWebFeedMainMenuItem;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(WebFeedBridge.getTestHooksForTesting(), mWebFeedBridgeJniMock);

        mJniMocker.mock(UrlFormatterJni.TEST_HOOKS, mUrlFormatterJniMock);
        doAnswer(invocation -> { return invocation.<GURL>getArgument(0).getHost(); })
                .when(mUrlFormatterJniMock)
                .formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(any());

        doReturn(GURL.emptyGURL()).when(mTab).getOriginalUrl();
        doReturn(false).when(mTab).isShowingErrorPage();

        mActivity = Robolectric.setupActivity(Activity.class);
        // Required for resolving an attribute used in AppMenuItemText.
        mActivity.setTheme(R.style.Theme_BrowserUI);

        setGetWebFeedMetadataForPageRepsonse(null);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mWebFeedMainMenuItem = (WebFeedMainMenuItem) (LayoutInflater.from(mActivity).inflate(
                    R.layout.web_feed_main_menu_item, null));
        });
    }

    @Test
    @UiThreadTest
    public void initialize_hasFavicon_displaysFavicon() {
        initializeWebFeedMainMenuItem(mBitmap);

        ImageView imageView = mWebFeedMainMenuItem.findViewById(R.id.icon);
        Bitmap actualIcon = ((BitmapDrawable) imageView.getDrawable()).getBitmap();
        assertEquals("Icon should be favicon.", mBitmap, actualIcon);
        assertEquals("Icon should be visible.", View.VISIBLE, imageView.getVisibility());
    }

    @Test
    @UiThreadTest
    public void initialize_noFavicon_hasMonogram() {
        initializeWebFeedMainMenuItem(/*bitmap=*/null);

        ImageView imageView = mWebFeedMainMenuItem.findViewById(R.id.icon);
        Bitmap actualIcon = ((BitmapDrawable) imageView.getDrawable()).getBitmap();
        assertNotNull("Icon should not be null.", actualIcon);
        assertEquals("Icon should be visible.", View.VISIBLE, imageView.getVisibility());
    }

    @Test
    @UiThreadTest
    public void initialize_emptyUrl_removesIcon() {
        doReturn(GURL.emptyGURL()).when(mTab).getOriginalUrl();
        mWebFeedMainMenuItem.initialize(mTab, mAppMenuHandler, new MockLargeIconBridge(null),
                mFeedLauncher, mDialogManager, mSnackBarManager);

        ImageView imageView = mWebFeedMainMenuItem.findViewById(R.id.icon);
        assertEquals("Icon should be gone.", View.GONE, imageView.getVisibility());
    }

    @Test
    @UiThreadTest
    public void initialize_displaysCorrectTitle() {
        initializeWebFeedMainMenuItem(/*bitmap=*/null);

        TextView textView = mWebFeedMainMenuItem.findViewById(R.id.menu_item_text);
        assertEquals("Title should be shortened URL.",
                UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(TEST_URL),
                textView.getText());
    }

    @Test
    @UiThreadTest
    public void initialize_noMetadata_displaysFollowChip() {
        setGetWebFeedMetadataForPageRepsonse(null);

        initializeWebFeedMainMenuItem(/*bitmap=*/null);

        verifyFollowChip(/*enabled=*/true);
    }

    @Test
    @UiThreadTest
    public void initialize_notFollowed_displaysFollowChip() {
        setGetWebFeedMetadataForPageRepsonse(
                createWebFeedMetadata(WebFeedSubscriptionStatus.NOT_SUBSCRIBED));

        initializeWebFeedMainMenuItem(/*bitmap=*/null);

        verifyFollowChip(/*enabled=*/true);
    }

    @Test
    @UiThreadTest
    public void initialize_errorPage_displaysDisabledFollowChip() {
        doReturn(true).when(mTab).isShowingErrorPage();
        setGetWebFeedMetadataForPageRepsonse(
                createWebFeedMetadata(WebFeedSubscriptionStatus.NOT_SUBSCRIBED));

        initializeWebFeedMainMenuItem(/*bitmap=*/null);

        verifyFollowChip(/*enabled=*/false);
    }

    @Test
    @UiThreadTest
    public void initialize_unknownFollowStatus_displaysFollowChip() {
        setGetWebFeedMetadataForPageRepsonse(
                createWebFeedMetadata(WebFeedSubscriptionStatus.UNKNOWN));

        initializeWebFeedMainMenuItem(/*bitmap=*/null);

        verifyFollowChip(/*enabled=*/true);
    }

    @Test
    @UiThreadTest
    public void initialize_followed_displaysFollowingChip() {
        setGetWebFeedMetadataForPageRepsonse(
                createWebFeedMetadata(WebFeedSubscriptionStatus.SUBSCRIBED));

        initializeWebFeedMainMenuItem(/*bitmap=*/null);

        ChipView followChipView = mWebFeedMainMenuItem.findViewById(R.id.follow_chip_view);
        ChipView followingChipView = mWebFeedMainMenuItem.findViewById(R.id.following_chip_view);
        TextView textView = followingChipView.getPrimaryTextView();
        assertEquals("Follow chip should be gone.", View.GONE, followChipView.getVisibility());
        assertEquals("Following chip should be visible.", View.VISIBLE,
                followingChipView.getVisibility());
        assertEquals("Chip text should say Following.",
                mActivity.getResources().getString(R.string.menu_following), textView.getText());
        assertTrue("Following chip should be enabled.", followingChipView.isEnabled());
    }

    @Test
    @UiThreadTest
    public void initialize_unfollowInProgress_displaysLoadingFollowingChip() {
        setGetWebFeedMetadataForPageRepsonse(
                createWebFeedMetadata(WebFeedSubscriptionStatus.UNSUBSCRIBE_IN_PROGRESS));

        initializeWebFeedMainMenuItem(/*bitmap=*/null);

        ChipView followChipView = mWebFeedMainMenuItem.findViewById(R.id.follow_chip_view);
        ChipView followingChipView = mWebFeedMainMenuItem.findViewById(R.id.following_chip_view);
        TextView textView = followingChipView.getPrimaryTextView();
        assertEquals("Follow chip should be gone.", View.GONE, followChipView.getVisibility());
        assertEquals("Following chip should be invisible at first.", View.INVISIBLE,
                followingChipView.getVisibility());
        assertEquals("Chip text should say Following",
                mActivity.getResources().getString(R.string.menu_following), textView.getText());
        assertFalse("Following chip should be disabled.", followingChipView.isEnabled());
    }

    @Test
    @UiThreadTest
    public void initialize_followInProgress_displaysLoadingFollowChip() {
        setGetWebFeedMetadataForPageRepsonse(
                createWebFeedMetadata(WebFeedSubscriptionStatus.SUBSCRIBE_IN_PROGRESS));

        initializeWebFeedMainMenuItem(/*bitmap=*/null);

        ChipView followChipView = mWebFeedMainMenuItem.findViewById(R.id.follow_chip_view);
        ChipView followingChipView = mWebFeedMainMenuItem.findViewById(R.id.following_chip_view);
        TextView textView = followChipView.getPrimaryTextView();
        assertEquals(
                "Following chip should be gone.", View.GONE, followingChipView.getVisibility());
        assertEquals("Follow chip should be invisible at first.", View.INVISIBLE,
                followChipView.getVisibility());
        assertEquals("Chip text should say Follow",
                mActivity.getResources().getString(R.string.menu_follow), textView.getText());
        assertFalse("Follow chip should be disabled.", followChipView.isEnabled());
    }

    /**
     * Verifies that the follow chip is showing.
     */
    private void verifyFollowChip(boolean enabled) {
        ChipView followChipView = mWebFeedMainMenuItem.findViewById(R.id.follow_chip_view);
        ChipView followingChipView = mWebFeedMainMenuItem.findViewById(R.id.following_chip_view);
        TextView textView = followChipView.getPrimaryTextView();
        assertEquals(
                "Follow chip should be visible.", View.VISIBLE, followChipView.getVisibility());
        assertEquals(
                "Following chip should be gone.", View.GONE, followingChipView.getVisibility());
        assertEquals("Chip text should say Follow.",
                mActivity.getResources().getString(R.string.menu_follow), textView.getText());
        assertEquals(String.format("Follow chip isEnabled should be %b", enabled), enabled,
                followChipView.isEnabled());
    }

    /**
     * Helper method to initialize {@code mWebFeedMainMenuItem} with standard parameters.
     *
     * @param bitmap Bitmap returned by the {@link MockLargeIconBridge}.
     */
    private void initializeWebFeedMainMenuItem(Bitmap bitmap) {
        doReturn(TEST_URL).when(mTab).getOriginalUrl();
        mWebFeedMainMenuItem.initialize(mTab, mAppMenuHandler, new MockLargeIconBridge(bitmap),
                mFeedLauncher, mDialogManager, mSnackBarManager);
    }

    /**
     * Helper method to create a {@link WebFeedBridge.WebFeedMetadata} with standard parameters.
     *
     * @param subscriptionStatus {@link WebFeedSubscriptionStatus} for the metadata.
     */
    private WebFeedBridge.WebFeedMetadata createWebFeedMetadata(
            @WebFeedSubscriptionStatus int subscriptionStatus) {
        return new WebFeedBridge.WebFeedMetadata("id".getBytes(), "title", TEST_URL,
                subscriptionStatus, WebFeedAvailabilityStatus.INACTIVE, /*isRecommended=*/false);
    }

    private void setGetWebFeedMetadataForPageRepsonse(WebFeedBridge.WebFeedMetadata metadata) {
        doAnswer(invocation -> {
            invocation.<Callback<WebFeedBridge.WebFeedMetadata>>getArgument(1).onResult(metadata);
            return null;
        })
                .when(mWebFeedBridgeJniMock)
                .findWebFeedInfoForPage(any(), any(Callback.class));
    }

    private static class MockLargeIconBridge extends LargeIconBridge {
        private final Bitmap mBitmap;

        MockLargeIconBridge(Bitmap bitmap) {
            mBitmap = bitmap;
        }

        @Override
        public boolean getLargeIconForUrl(
                GURL pageUrl, int desiredSizePx, final LargeIconBridge.LargeIconCallback callback) {
            callback.onLargeIconAvailable(mBitmap, Color.BLACK, false, IconType.FAVICON);
            return true;
        }
    }
}
