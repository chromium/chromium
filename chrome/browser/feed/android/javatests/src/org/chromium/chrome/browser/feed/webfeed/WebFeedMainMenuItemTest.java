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

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.widget.ChipView;
import org.chromium.url.GURL;

/**
 * Tests {@link WebFeedMainMenuItem}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public final class WebFeedMainMenuItemTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock
    private WebFeedBridge mWebFeedBridge;

    private static final Bitmap ICON = Bitmap.createBitmap(48, 84, Bitmap.Config.ALPHA_8);
    private static final GURL TEST_URL = new GURL("http://www.example.com");

    private Activity mActivity;
    private AppMenuHandler mAppMenuHandler;
    private SnackbarManager mSnackBarManager;
    private WebFeedMainMenuItem mWebFeedMainMenuItem;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        mAppMenuHandler = mActivityTestRule.getAppMenuCoordinator().getAppMenuHandler();
        mSnackBarManager = mActivityTestRule.getActivity().getSnackbarManager();
        doAnswer(invocation -> {
            invocation.<Callback<WebFeedBridge.WebFeedMetadata>>getArgument(1).onResult(null);
            return null;
        })
                .when(mWebFeedBridge)
                .getWebFeedMetadataForPage(any(GURL.class), any(Callback.class));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mWebFeedMainMenuItem = (WebFeedMainMenuItem) (LayoutInflater.from(mActivity).inflate(
                    R.layout.web_feed_main_menu_item, null));
        });
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void initialize_hasFavicon_displaysFavicon() {
        initializeWebFeedMainMenuItem(ICON);

        ImageView imageView = mWebFeedMainMenuItem.findViewById(R.id.icon);
        Bitmap actualIcon = ((BitmapDrawable) imageView.getDrawable()).getBitmap();
        assertEquals("Icon should be favicon.", ICON, actualIcon);
        assertEquals("Icon should be visible.", View.VISIBLE, imageView.getVisibility());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void initialize_noFavicon_hasMonogram() {
        initializeWebFeedMainMenuItem(/*bitmap=*/null);

        ImageView imageView = mWebFeedMainMenuItem.findViewById(R.id.icon);
        Bitmap actualIcon = ((BitmapDrawable) imageView.getDrawable()).getBitmap();
        assertNotNull("Icon should not be null.", actualIcon);
        assertEquals("Icon should be visible.", View.VISIBLE, imageView.getVisibility());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void initialize_emptyUrl_removesIcon() {
        mWebFeedMainMenuItem.initialize(GURL.emptyGURL(), mAppMenuHandler,
                new MockLargeIconBridge(null), mSnackBarManager, mWebFeedBridge);

        ImageView imageView = mWebFeedMainMenuItem.findViewById(R.id.icon);
        assertEquals("Icon should be gone.", View.GONE, imageView.getVisibility());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void initialize_displaysCorrectTitle() {
        initializeWebFeedMainMenuItem(/*bitmap=*/null);

        TextView textView = mWebFeedMainMenuItem.findViewById(R.id.menu_item_text);
        assertEquals("Title should be shortened URL.",
                UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(TEST_URL),
                textView.getText());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void initialize_noMetadata_displaysFollowChip() {
        doAnswer(invocation -> {
            invocation.<Callback<WebFeedBridge.WebFeedMetadata>>getArgument(1).onResult(null);
            return null;
        })
                .when(mWebFeedBridge)
                .getWebFeedMetadataForPage(any(GURL.class), any(Callback.class));

        initializeWebFeedMainMenuItem(/*bitmap=*/null);

        verifyFollowChip();
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void initialize_notFollowed_displaysFollowChip() {
        WebFeedBridge.WebFeedMetadata webFeedMetadata =
                createWebFeedMetadata(WebFeedSubscriptionStatus.NOT_SUBSCRIBED);
        doAnswer(invocation -> {
            invocation.<Callback<WebFeedBridge.WebFeedMetadata>>getArgument(1).onResult(
                    webFeedMetadata);
            return null;
        })
                .when(mWebFeedBridge)
                .getWebFeedMetadataForPage(any(GURL.class), any(Callback.class));

        initializeWebFeedMainMenuItem(/*bitmap=*/null);

        verifyFollowChip();
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void initialize_unknownFollowStatus_displaysFollowChip() {
        WebFeedBridge.WebFeedMetadata webFeedMetadata =
                createWebFeedMetadata(WebFeedSubscriptionStatus.UNKNOWN);
        doAnswer(invocation -> {
            invocation.<Callback<WebFeedBridge.WebFeedMetadata>>getArgument(1).onResult(
                    webFeedMetadata);
            return null;
        })
                .when(mWebFeedBridge)
                .getWebFeedMetadataForPage(any(GURL.class), any(Callback.class));

        initializeWebFeedMainMenuItem(/*bitmap=*/null);

        verifyFollowChip();
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void initialize_followed_displaysFollowingChip() {
        WebFeedBridge.WebFeedMetadata webFeedMetadata =
                createWebFeedMetadata(WebFeedSubscriptionStatus.SUBSCRIBED);
        doAnswer(invocation -> {
            invocation.<Callback<WebFeedBridge.WebFeedMetadata>>getArgument(1).onResult(
                    webFeedMetadata);
            return null;
        })
                .when(mWebFeedBridge)
                .getWebFeedMetadataForPage(any(GURL.class), any(Callback.class));

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
    @MediumTest
    @UiThreadTest
    public void initialize_unfollowInProgress_displaysLoadingFollowingChip() {
        WebFeedBridge.WebFeedMetadata webFeedMetadata =
                createWebFeedMetadata(WebFeedSubscriptionStatus.UNSUBSCRIBE_IN_PROGRESS);
        doAnswer(invocation -> {
            invocation.<Callback<WebFeedBridge.WebFeedMetadata>>getArgument(1).onResult(
                    webFeedMetadata);
            return null;
        })
                .when(mWebFeedBridge)
                .getWebFeedMetadataForPage(any(GURL.class), any(Callback.class));

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
    @MediumTest
    @UiThreadTest
    public void initialize_followInProgress_displaysLoadingFollowChip() {
        WebFeedBridge.WebFeedMetadata webFeedMetadata =
                createWebFeedMetadata(WebFeedSubscriptionStatus.SUBSCRIBE_IN_PROGRESS);
        doAnswer(invocation -> {
            invocation.<Callback<WebFeedBridge.WebFeedMetadata>>getArgument(1).onResult(
                    webFeedMetadata);
            return null;
        })
                .when(mWebFeedBridge)
                .getWebFeedMetadataForPage(any(GURL.class), any(Callback.class));

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
    private void verifyFollowChip() {
        ChipView followChipView = mWebFeedMainMenuItem.findViewById(R.id.follow_chip_view);
        ChipView followingChipView = mWebFeedMainMenuItem.findViewById(R.id.following_chip_view);
        TextView textView = followChipView.getPrimaryTextView();
        assertEquals(
                "Follow chip should be visible.", View.VISIBLE, followChipView.getVisibility());
        assertEquals(
                "Following chip should be gone.", View.GONE, followingChipView.getVisibility());
        assertEquals("Chip text should say Follow.",
                mActivity.getResources().getString(R.string.menu_follow), textView.getText());
        assertTrue("Follow chip should be enabled.", followChipView.isEnabled());
    }

    /**
     * Helper method to initialize {@code mWebFeedMainMenuItem} with standard parameters.
     *
     * @param bitmap Bitmap returned by the {@link MockLargeIconBridge}.
     */
    private void initializeWebFeedMainMenuItem(Bitmap bitmap) {
        mWebFeedMainMenuItem.initialize(TEST_URL, mAppMenuHandler, new MockLargeIconBridge(bitmap),
                mSnackBarManager, mWebFeedBridge);
    }

    /**
     * Helper method to create a {@link WebFeedBridge.WebFeedMetadata} with standard parameters.
     *
     * @param subscriptionStatus {@link WebFeedSubscriptionStatus} for the metadata.
     */
    private WebFeedBridge.WebFeedMetadata createWebFeedMetadata(
            @WebFeedSubscriptionStatus int subscriptionStatus) {
        return new WebFeedBridge.WebFeedMetadata("id".getBytes(), "title", TEST_URL,
                subscriptionStatus, /*isActive=*/false, /*isRecommended=*/false);
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
