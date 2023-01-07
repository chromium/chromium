// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;

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

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetPropertyModelBuilder.ContentType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerDetails;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.url.GURL;

import java.util.ArrayList;

/**
 * Unit tests {@link ShareSheetBottomSheetContent}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public final class ShareSheetBottomSheetContentTest {
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock
    private ShareSheetLinkToggleCoordinator mShareSheetLinkToggleCoordinator;
    @Mock
    private Tracker mFeatureEngagementTracker;

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
        MockitoAnnotations.initMocks(this);
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();

        mPreviewUrl = UrlFormatter.formatUrlForDisplayOmitSchemeOmitTrivialSubdomains(sUrl);
        mShareParams = new ShareParams.Builder(/*window=*/null, sTitle, sUrl)
                               .setText(sText)
                               .setFileUris(new ArrayList<>(ImmutableList.of(sImageUri)))
                               .setLinkToTextSuccessful(true)
                               .build();
        // Pretend the feature engagement feature is already initialized. Otherwise
        // UserEducationHelper#requestShowIPH() calls get dropped during test.
        doAnswer(invocation -> {
            invocation.<Callback<Boolean>>getArgument(0).onResult(true);
            return null;
        })
                .when(mFeatureEngagementTracker)
                .addOnInitializedCallback(any());
        TrackerFactory.setTrackerForTests(mFeatureEngagementTracker);

        mShareSheetBottomSheetContent = new ShareSheetBottomSheetContent(mActivity,
                new MockLargeIconBridge(), null, mShareParams, mFeatureEngagementTracker);
    }

    @After
    public void tearDown() {
        TrackerFactory.setTrackerForTests(null);
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
                                .build(),
                        mFeatureEngagementTracker);

        shareSheetBottomSheetContent.createRecyclerViews(ImmutableList.of(), ImmutableList.of(),
                ImmutableSet.of(ContentType.IMAGE), fileContentType, DetailedContentType.IMAGE,
                mShareSheetLinkToggleCoordinator);

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
                                .build(),
                        mFeatureEngagementTracker);

        shareSheetBottomSheetContent.createRecyclerViews(ImmutableList.of(), ImmutableList.of(),
                ImmutableSet.of(ContentType.IMAGE), fileContentType,
                DetailedContentType.NOT_SPECIFIED, mShareSheetLinkToggleCoordinator);

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
                ImmutableSet.of(ContentType.HIGHLIGHTED_TEXT), "",
                DetailedContentType.HIGHLIGHTED_TEXT, mShareSheetLinkToggleCoordinator);

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
        mShareSheetBottomSheetContent.createRecyclerViews(ImmutableList.of(), ImmutableList.of(),
                ImmutableSet.of(ContentType.TEXT), "", DetailedContentType.NOT_SPECIFIED,
                mShareSheetLinkToggleCoordinator);

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
                ImmutableSet.of(ContentType.LINK_PAGE_VISIBLE), "",
                DetailedContentType.NOT_SPECIFIED, mShareSheetLinkToggleCoordinator);

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
                ImmutableSet.of(ContentType.LINK_PAGE_VISIBLE), "",
                DetailedContentType.NOT_SPECIFIED, mShareSheetLinkToggleCoordinator);

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
                ImmutableSet.of(ContentType.LINK_PAGE_NOT_VISIBLE, ContentType.TEXT), "",
                DetailedContentType.NOT_SPECIFIED, mShareSheetLinkToggleCoordinator);

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
                        new ShareParams.Builder(/*window=*/null, /*title=*/"", sUrl).build(),
                        mFeatureEngagementTracker);

        shareSheetBottomSheetContent.createRecyclerViews(ImmutableList.of(), ImmutableList.of(),
                ImmutableSet.of(ContentType.LINK_PAGE_NOT_VISIBLE), "",
                DetailedContentType.NOT_SPECIFIED, mShareSheetLinkToggleCoordinator);

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

    @Test
    @MediumTest
    @DisabledTest(message = "Fails on official builders: https://crbug.com/1281875")
    public void createRecyclerViews_toggleOff_showsIph() {
        String fileContentType = "image/gif";
        ShareSheetBottomSheetContent shareSheetBottomSheetContent =
                new ShareSheetBottomSheetContent(mActivity, new MockLargeIconBridge(), null,
                        new ShareParams.Builder(/*window=*/null, /*title=*/"", /*url=*/"")
                                .setFileUris(new ArrayList<>(ImmutableList.of(sImageUri)))
                                .setFileContentType(fileContentType)
                                .build(),
                        mFeatureEngagementTracker);
        when(mShareSheetLinkToggleCoordinator.shouldShowToggle()).thenReturn(true);
        when(mShareSheetLinkToggleCoordinator.shouldEnableToggleByDefault()).thenReturn(false);
        when(mFeatureEngagementTracker.shouldTriggerHelpUIWithSnooze(any()))
                .thenReturn(new TriggerDetails(true, false));

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> shareSheetBottomSheetContent.createRecyclerViews(ImmutableList.of(),
                                ImmutableList.of(), ImmutableSet.of(ContentType.IMAGE),
                                fileContentType, DetailedContentType.GIF,
                                mShareSheetLinkToggleCoordinator));

        ImageView toggleView =
                shareSheetBottomSheetContent.getContentView().findViewById(R.id.link_toggle_view);
        assertEquals(View.VISIBLE, toggleView.getVisibility());
        verify(mFeatureEngagementTracker)
                .shouldTriggerHelpUIWithSnooze(
                        FeatureConstants.IPH_SHARING_HUB_LINK_TOGGLE_FEATURE);
    }

    @Test
    @MediumTest
    public void createRecyclerViews_toggleOn_doesNotShowIph() {
        String fileContentType = "image/jpeg";
        ShareSheetBottomSheetContent shareSheetBottomSheetContent =
                new ShareSheetBottomSheetContent(mActivity, new MockLargeIconBridge(), null,
                        new ShareParams.Builder(/*window=*/null, /*title=*/"", /*url=*/"")
                                .setFileUris(new ArrayList<>(ImmutableList.of(sImageUri)))
                                .setFileContentType(fileContentType)
                                .build(),
                        mFeatureEngagementTracker);
        when(mShareSheetLinkToggleCoordinator.shouldShowToggle()).thenReturn(true);
        when(mShareSheetLinkToggleCoordinator.shouldEnableToggleByDefault()).thenReturn(true);

        shareSheetBottomSheetContent.createRecyclerViews(ImmutableList.of(), ImmutableList.of(),
                ImmutableSet.of(ContentType.IMAGE), fileContentType, DetailedContentType.IMAGE,
                mShareSheetLinkToggleCoordinator);

        ImageView toggleView =
                shareSheetBottomSheetContent.getContentView().findViewById(R.id.link_toggle_view);
        assertEquals(View.VISIBLE, toggleView.getVisibility());
        verifyZeroInteractions(mFeatureEngagementTracker);
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
