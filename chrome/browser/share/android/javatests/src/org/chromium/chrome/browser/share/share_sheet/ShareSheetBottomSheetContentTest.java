// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.net.Uri;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetPropertyModelBuilder.ContentType;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.test.util.DummyUiActivity;
import org.chromium.url.GURL;

import java.util.ArrayList;

/**
 * Tests {@link ShareSheetBottomSheetContent}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Features.EnableFeatures({ChromeFeatureList.CHROME_SHARING_HUB_V15})
public final class ShareSheetBottomSheetContentTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public BaseActivityTestRule<DummyUiActivity> mActivityTestRule =
            new BaseActivityTestRule<>(DummyUiActivity.class);

    private static final Bitmap.Config sConfig = Bitmap.Config.ALPHA_8;
    private static final Uri sImageUri = Uri.parse("content://testImage.png");
    private static final String sText = "Text";
    private static final String sTitle = "Title";
    private static final String sUrl = "https://www.example.com";
    private String mPreviewUrl;

    private Activity mActivity;
    private ShareParams mShareParams;
    private ShareSheetBottomSheetContent mShareSheetBottomSheetContent;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        mPreviewUrl = UrlFormatter.formatUrlForDisplayOmitSchemeOmitTrivialSubdomains(sUrl);
        mShareParams = new ShareParams.Builder(/*window=*/null, sTitle, sUrl)
                               .setText(sText)
                               .setFileUris(new ArrayList<>(ImmutableList.of(sImageUri)))
                               .build();

        mShareSheetBottomSheetContent = new ShareSheetBottomSheetContent(
                mActivity, new MockLargeIconBridge(), null, mShareParams);
    }

    @Test
    @MediumTest
    public void createRecyclerViews_imageOnlyShare() {
        String fileContentType = "image/jpeg";
        ShareSheetBottomSheetContent shareSheetBottomSheetContent =
                new ShareSheetBottomSheetContent(mActivity, new MockLargeIconBridge(), null,
                        new ShareParams.Builder(/*window=*/null, /*title=*/"", /*url=*/"")
                                .setFileUris(new ArrayList<>(ImmutableList.of(sImageUri)))
                                .setFileContentType(fileContentType)
                                .build());

        shareSheetBottomSheetContent.createRecyclerViews(ImmutableList.of(), ImmutableList.of(),
                ImmutableSet.of(ContentType.IMAGE), fileContentType);

        TextView titleView =
                shareSheetBottomSheetContent.getContentView().findViewById(R.id.title_preview);
        TextView subtitleView =
                shareSheetBottomSheetContent.getContentView().findViewById(R.id.subtitle_preview);
        assertEquals("", titleView.getText());
        assertEquals("image", subtitleView.getText());
    }

    @Test
    @MediumTest
    public void createRecyclerViews_fileShare() {
        String fileContentType = "video/mp4";
        ShareSheetBottomSheetContent shareSheetBottomSheetContent =
                new ShareSheetBottomSheetContent(mActivity, new MockLargeIconBridge(), null,
                        new ShareParams.Builder(/*window=*/null, /*title=*/"", /*url=*/"")
                                .setFileUris(new ArrayList<>(
                                        ImmutableList.of(Uri.parse("content://TestVideo.mp4"))))
                                .setFileContentType(fileContentType)
                                .build());

        shareSheetBottomSheetContent.createRecyclerViews(ImmutableList.of(), ImmutableList.of(),
                ImmutableSet.of(ContentType.IMAGE), fileContentType);

        TextView titleView =
                shareSheetBottomSheetContent.getContentView().findViewById(R.id.title_preview);
        TextView subtitleView =
                shareSheetBottomSheetContent.getContentView().findViewById(R.id.subtitle_preview);
        assertEquals("", titleView.getText());
        assertEquals("video", subtitleView.getText());
    }

    @Test
    @MediumTest
    public void createRecyclerViews_highlightedTextShare() {
        mShareSheetBottomSheetContent.createRecyclerViews(ImmutableList.of(), ImmutableList.of(),
                ImmutableSet.of(ContentType.HIGHLIGHTED_TEXT), "");

        TextView titleView =
                mShareSheetBottomSheetContent.getContentView().findViewById(R.id.title_preview);
        TextView subtitleView =
                mShareSheetBottomSheetContent.getContentView().findViewById(R.id.subtitle_preview);
        assertEquals(View.GONE, titleView.getVisibility());
        assertEquals(mShareParams.getText(), subtitleView.getText());
    }

    @Test
    @MediumTest
    public void createRecyclerViews_textOnlyShare() {
        mShareSheetBottomSheetContent.createRecyclerViews(
                ImmutableList.of(), ImmutableList.of(), ImmutableSet.of(ContentType.TEXT), "");

        TextView titleView =
                mShareSheetBottomSheetContent.getContentView().findViewById(R.id.title_preview);
        TextView subtitleView =
                mShareSheetBottomSheetContent.getContentView().findViewById(R.id.subtitle_preview);
        assertEquals(View.GONE, titleView.getVisibility());
        assertEquals(mShareParams.getText(), subtitleView.getText());
    }

    @Test
    @MediumTest
    public void createRecyclerViews_producesCorrectFavicon() {
        mShareSheetBottomSheetContent.createRecyclerViews(ImmutableList.of(), ImmutableList.of(),
                ImmutableSet.of(ContentType.LINK_PAGE_VISIBLE), "");

        ImageView imageView =
                mShareSheetBottomSheetContent.getContentView().findViewById(R.id.image_preview);
        assertNotNull(imageView.getDrawable());
        Bitmap bitmap = ((BitmapDrawable) imageView.getDrawable()).getBitmap();
        int size = mActivity.getResources().getDimensionPixelSize(
                R.dimen.sharing_hub_preview_inner_icon_size);
        assertEquals(size, bitmap.getWidth());
        assertEquals(size, bitmap.getHeight());
        assertEquals(sConfig, bitmap.getConfig());
    }

    @Test
    @MediumTest
    public void createRecyclerViews_tabShare() {
        mShareSheetBottomSheetContent.createRecyclerViews(ImmutableList.of(), ImmutableList.of(),
                ImmutableSet.of(ContentType.LINK_PAGE_VISIBLE), "");

        TextView titleView =
                mShareSheetBottomSheetContent.getContentView().findViewById(R.id.title_preview);
        TextView subtitleView =
                mShareSheetBottomSheetContent.getContentView().findViewById(R.id.subtitle_preview);
        ImageView imageView =
                mShareSheetBottomSheetContent.getContentView().findViewById(R.id.image_preview);
        assertEquals(mShareParams.getTitle(), titleView.getText());
        assertEquals(mPreviewUrl, subtitleView.getText());
        assertNotNull(imageView.getDrawable());
    }

    @Test
    @MediumTest
    public void createRecyclerViews_webShareTextAndUrl() {
        mShareSheetBottomSheetContent.createRecyclerViews(ImmutableList.of(), ImmutableList.of(),
                ImmutableSet.of(ContentType.LINK_PAGE_NOT_VISIBLE, ContentType.TEXT), "");

        TextView titleView =
                mShareSheetBottomSheetContent.getContentView().findViewById(R.id.title_preview);
        TextView subtitleView =
                mShareSheetBottomSheetContent.getContentView().findViewById(R.id.subtitle_preview);
        ImageView imageView =
                mShareSheetBottomSheetContent.getContentView().findViewById(R.id.image_preview);
        assertEquals(mShareParams.getText(), titleView.getText());
        assertEquals(mPreviewUrl, subtitleView.getText());
        assertNotNull(imageView.getDrawable());
    }

    @Test
    @MediumTest
    public void createRecyclerViews_webShareUrl() {
        ShareSheetBottomSheetContent shareSheetBottomSheetContent =
                new ShareSheetBottomSheetContent(mActivity, new MockLargeIconBridge(), null,
                        new ShareParams.Builder(/*window=*/null, /*title=*/"", sUrl).build());

        shareSheetBottomSheetContent.createRecyclerViews(ImmutableList.of(), ImmutableList.of(),
                ImmutableSet.of(ContentType.LINK_PAGE_NOT_VISIBLE), "");

        TextView titleView =
                shareSheetBottomSheetContent.getContentView().findViewById(R.id.title_preview);
        TextView subtitleView =
                shareSheetBottomSheetContent.getContentView().findViewById(R.id.subtitle_preview);
        ImageView imageView =
                shareSheetBottomSheetContent.getContentView().findViewById(R.id.image_preview);
        assertEquals(View.GONE, titleView.getVisibility());
        assertEquals(mPreviewUrl, subtitleView.getText());
        assertNotNull(imageView.getDrawable());
    }

    private static class MockLargeIconBridge extends LargeIconBridge {
        @Override
        public boolean getLargeIconForUrl(
                GURL pageUrl, int desiredSizePx, final LargeIconBridge.LargeIconCallback callback) {
            callback.onLargeIconAvailable(
                    Bitmap.createBitmap(48, 84, sConfig), 0, false, IconType.INVALID);
            return true;
        }
    }
}
