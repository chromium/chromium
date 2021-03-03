// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

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

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.test.util.DummyUiActivity;
import org.chromium.url.GURL;

/**
 * Tests {@link WebFeedMainMenuItem}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public final class WebFeedMainMenuItemTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public BaseActivityTestRule<DummyUiActivity> mActivityTestRule =
            new BaseActivityTestRule<>(DummyUiActivity.class);

    private static final Bitmap sIcon = Bitmap.createBitmap(48, 84, Bitmap.Config.ALPHA_8);
    private static final GURL sTestUrl = new GURL("http://www.example.com");

    private Activity mActivity;
    private WebFeedMainMenuItem mWebFeedMainMenuItem;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();

        mWebFeedMainMenuItem = (WebFeedMainMenuItem) (LayoutInflater.from(mActivity).inflate(
                R.layout.web_feed_main_menu_item, null));
    }

    @Test
    @MediumTest
    public void initialize_hasFavicon_displaysFavicon() {
        mWebFeedMainMenuItem.initialize(sTestUrl, new MockLargeIconBridge(sIcon));

        ImageView imageView = mWebFeedMainMenuItem.findViewById(R.id.icon);
        Bitmap actualIcon = ((BitmapDrawable) imageView.getDrawable()).getBitmap();
        assertEquals("Icon should be favicon.", sIcon, actualIcon);
        assertEquals("Icon should be visible.", View.VISIBLE, imageView.getVisibility());
    }

    @Test
    @MediumTest
    public void initialize_noFavicon_hasMonogram() {
        mWebFeedMainMenuItem.initialize(sTestUrl, new MockLargeIconBridge(null));

        ImageView imageView = mWebFeedMainMenuItem.findViewById(R.id.icon);
        Bitmap actualIcon = ((BitmapDrawable) imageView.getDrawable()).getBitmap();
        assertNotNull("Icon should not be null.", actualIcon);
        assertEquals("Icon should be visible.", View.VISIBLE, imageView.getVisibility());
    }

    @Test
    @MediumTest
    public void initialize_emptyUrl_removesIcon() {
        mWebFeedMainMenuItem.initialize(GURL.emptyGURL(), new MockLargeIconBridge(null));

        ImageView imageView = mWebFeedMainMenuItem.findViewById(R.id.icon);
        assertEquals("Icon should be gone.", View.GONE, imageView.getVisibility());
    }

    @Test
    @MediumTest
    public void initialize_displaysCorrectTitle() {
        mWebFeedMainMenuItem.initialize(sTestUrl, new MockLargeIconBridge(null));

        TextView textView = mWebFeedMainMenuItem.findViewById(R.id.menu_item_text);
        assertEquals("Title should be shortened URL.",
                UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(sTestUrl),
                textView.getText());
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
