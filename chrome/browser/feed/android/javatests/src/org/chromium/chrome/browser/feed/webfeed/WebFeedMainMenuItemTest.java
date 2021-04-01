// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

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

    private static final Bitmap sIcon = Bitmap.createBitmap(48, 84, Bitmap.Config.ALPHA_8);
    private static final GURL sTestUrl = new GURL("http://www.example.com");

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
        when(mWebFeedBridge.getFollowedIds(any(GURL.class))).thenReturn(null);

        mWebFeedMainMenuItem = (WebFeedMainMenuItem) (LayoutInflater.from(mActivity).inflate(
                R.layout.web_feed_main_menu_item, null));
    }

    @Test
    @MediumTest
    public void initialize_hasFavicon_displaysFavicon() {
        initializeWebFeedMainMenuItem(sIcon);

        ImageView imageView = mWebFeedMainMenuItem.findViewById(R.id.icon);
        Bitmap actualIcon = ((BitmapDrawable) imageView.getDrawable()).getBitmap();
        assertEquals("Icon should be favicon.", sIcon, actualIcon);
        assertEquals("Icon should be visible.", View.VISIBLE, imageView.getVisibility());
    }

    @Test
    @MediumTest
    public void initialize_noFavicon_hasMonogram() {
        initializeWebFeedMainMenuItem(/*bitmap=*/null);

        ImageView imageView = mWebFeedMainMenuItem.findViewById(R.id.icon);
        Bitmap actualIcon = ((BitmapDrawable) imageView.getDrawable()).getBitmap();
        assertNotNull("Icon should not be null.", actualIcon);
        assertEquals("Icon should be visible.", View.VISIBLE, imageView.getVisibility());
    }

    @Test
    @MediumTest
    public void initialize_emptyUrl_removesIcon() {
        mWebFeedMainMenuItem.initialize(GURL.emptyGURL(), mAppMenuHandler,
                new MockLargeIconBridge(null), mSnackBarManager, mWebFeedBridge);

        ImageView imageView = mWebFeedMainMenuItem.findViewById(R.id.icon);
        assertEquals("Icon should be gone.", View.GONE, imageView.getVisibility());
    }

    @Test
    @MediumTest
    public void initialize_displaysCorrectTitle() {
        initializeWebFeedMainMenuItem(/*bitmap=*/null);

        TextView textView = mWebFeedMainMenuItem.findViewById(R.id.menu_item_text);
        assertEquals("Title should be shortened URL.",
                UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(sTestUrl),
                textView.getText());
    }

    @Test
    @MediumTest
    public void initialize_notFollowed_displaysFollowChip() {
        when(mWebFeedBridge.getFollowedIds(sTestUrl)).thenReturn(null);

        initializeWebFeedMainMenuItem(/*bitmap=*/null);

        ChipView followChipView = mWebFeedMainMenuItem.findViewById(R.id.follow_chip_view);
        ChipView followingChipView = mWebFeedMainMenuItem.findViewById(R.id.following_chip_view);
        TextView textView = followChipView.getPrimaryTextView();
        assertEquals(
                "Follow chip should be visible.", View.VISIBLE, followChipView.getVisibility());
        assertEquals(
                "Following chip should be gone.", View.GONE, followingChipView.getVisibility());
        assertEquals("Chip text should say Follow.",
                mActivity.getResources().getString(R.string.menu_follow), textView.getText());
    }

    @Test
    @MediumTest
    public void initialize_followed_displaysFollowingChip() {
        when(mWebFeedBridge.getFollowedIds(sTestUrl))
                .thenReturn(new WebFeedBridge.FollowedIds("followId", "webFeedId"));

        initializeWebFeedMainMenuItem(/*bitmap=*/null);

        ChipView followChipView = mWebFeedMainMenuItem.findViewById(R.id.follow_chip_view);
        ChipView followingChipView = mWebFeedMainMenuItem.findViewById(R.id.following_chip_view);
        TextView textView = followingChipView.getPrimaryTextView();
        assertEquals("Follow chip should be gone.", View.GONE, followChipView.getVisibility());
        assertEquals("Following chip should be visible.", View.VISIBLE,
                followingChipView.getVisibility());
        assertEquals("Chip text should say Follow.",
                mActivity.getResources().getString(R.string.menu_following), textView.getText());
    }

    /**
     * Helper method to initialize {@code mWebFeedMainMenuItem} with standard parameters.
     *
     * @param bitmap Bitmap returned by the {@link MockLargeIconBridge}.
     */
    private void initializeWebFeedMainMenuItem(Bitmap bitmap) {
        mWebFeedMainMenuItem.initialize(sTestUrl, mAppMenuHandler, new MockLargeIconBridge(bitmap),
                mSnackBarManager, mWebFeedBridge);
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
