// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.util.Size;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Features.JUnitProcessor;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallbacks;
import org.chromium.chrome.browser.tab_resumption.UrlImageProvider.UrlImageCallback;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID})
public class TabResumptionModuleViewUnitTest extends TestSupport {
    @Rule public JUnitProcessor mFeaturesProcessor = new JUnitProcessor();
    @Rule public JniMocker mocker = new JniMocker();
    @Mock UrlUtilities.Natives mUrlUtilitiesJniMock;

    private static final String TAB_TITLE = "Tab Title";
    private static final int TAB_ID = 11;

    @Mock private UrlImageProvider mUrlImageProvider;
    @Mock private ThumbnailProvider mThumbnailProvider;
    @Mock private Tab mTab;

    @Captor private ArgumentCaptor<GURL> mFetchImagePageUrlCaptor;
    @Captor private ArgumentCaptor<Callback<Bitmap>> mThumbnailProviderCaptor;
    @Captor private ArgumentCaptor<UrlImageCallback> mFetchImageCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Bitmap>> mFetchSalientImageCallbackCaptor;

    private TabResumptionModuleView mModuleView;
    private Size mThumbnailSize;
    private TabResumptionTileContainerView mTileContainerView;

    private SuggestionClickCallbacks mClickCallbacks;
    private SuggestionBundle mSuggestionBundle;

    private GURL mLastClickUrl;
    private int mLastClickTabId;
    private int mClickCount;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(UrlUtilitiesJni.TEST_HOOKS, mUrlUtilitiesJniMock);

        Context context = ApplicationProvider.getApplicationContext();
        context.setTheme(R.style.Theme_BrowserUI_DayNight);
        mModuleView =
                (TabResumptionModuleView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.tab_resumption_module_layout, null);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mTab.getTitle()).thenReturn(TAB_TITLE);
        when(mTab.getTimestampMillis()).thenReturn(makeTimestamp(24 - 3, 0, 0));
        when(mTab.getId()).thenReturn(TAB_ID);
        int size =
                context.getResources()
                        .getDimensionPixelSize(
                                org.chromium.chrome.browser.tab_ui.R.dimen
                                        .single_tab_module_tab_thumbnail_size_big);
        mThumbnailSize = new Size(size, size);

        mClickCallbacks =
                new SuggestionClickCallbacks() {
                    @Override
                    public void onSuggestionClickByUrl(GURL gurl) {
                        mLastClickUrl = gurl;
                        ++mClickCount;
                    }

                    @Override
                    public void onSuggestionClickByTabId(int tabId) {
                        mLastClickTabId = tabId;
                        ++mClickCount;
                    }
                };
        mSuggestionBundle = new SuggestionBundle(CURRENT_TIME_MS);
        mModuleView.setUrlImageProvider(mUrlImageProvider);
        mModuleView.setClickCallbacks(mClickCallbacks);
        mModuleView.setThumbnailProvider(mThumbnailProvider);
        mTileContainerView = mModuleView.getTileContainerViewForTesting();
    }

    @After
    public void tearDown() {
        mModuleView.destroy();
        mModuleView = null;
    }

    @Test
    @SmallTest
    public void testSetTitle() {
        String testTitle1 = "This is a test title";
        String testTitle2 = "Here is another test title";
        TextView titleTextView =
                ((TextView) mModuleView.findViewById(R.id.tab_resumption_title_description));

        mModuleView.setTitle(testTitle1);
        Assert.assertEquals(testTitle1, titleTextView.getText());
        mModuleView.setTitle(testTitle2);
        Assert.assertEquals(testTitle2, titleTextView.getText());
        mModuleView.setTitle(null);
        Assert.assertEquals("", titleTextView.getText());
    }

    @Test
    @SmallTest
    public void testRenderSingle() {
        SuggestionEntry entry1 =
                new SuggestionEntry(
                        /* sourceName= */ "Desktop",
                        /* url= */ JUnitTestGURLs.GOOGLE_URL_DOG,
                        /* title= */ "Google Dog",
                        /* timestamp= */ makeTimestamp(24 - 3, 0, 0),
                        /* id= */ 90);
        mSuggestionBundle.entries.add(entry1);

        Assert.assertEquals(0, mTileContainerView.getChildCount());

        mModuleView.setSuggestionBundle(mSuggestionBundle);
        Assert.assertEquals(1, mTileContainerView.getChildCount());

        // Capture call to fetch image.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchImageForUrl(
                        mFetchImagePageUrlCaptor.capture(), mFetchImageCallbackCaptor.capture());
        Assert.assertEquals(1, mFetchImagePageUrlCaptor.getAllValues().size());
        Assert.assertEquals(1, mFetchImageCallbackCaptor.getAllValues().size());
        Assert.assertEquals(
                JUnitTestGURLs.GOOGLE_URL_DOG, mFetchImagePageUrlCaptor.getAllValues().get(0));

        // Check tile texts.
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        Assert.assertEquals(
                "From Desktop", ((TextView) tile1.findViewById(R.id.tile_pre_info_text)).getText());
        Assert.assertEquals(
                "Google Dog", ((TextView) tile1.findViewById(R.id.tile_display_text)).getText());
        // Actual code would remove "www." prefix, but the test's JNI mock doesn't do so.
        Assert.assertEquals(
                "www.google.com \u2022 3 hr ago",
                ((TextView) tile1.findViewById(R.id.tile_post_info_text)).getText());

        // Image is not loaded yet.
        Assert.assertNull(((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable());

        // Provide test image, and check that it's shown as icon.
        Bitmap bitmap1 = makeBitmap(64, 64);
        mFetchImageCallbackCaptor.getAllValues().get(0).onBitmap(bitmap1);
        BitmapDrawable drawable1 =
                (BitmapDrawable) ((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable();
        Assert.assertNotNull(drawable1);
        Assert.assertEquals(bitmap1, drawable1.getBitmap());

        // Simulate click.
        Assert.assertEquals(0, mClickCount);
        Assert.assertEquals(null, mLastClickUrl);
        tile1.performClick();
        Assert.assertEquals(1, mClickCount);
        Assert.assertEquals(JUnitTestGURLs.GOOGLE_URL_DOG, mLastClickUrl);
    }

    @Test
    @SmallTest
    public void testRenderSingle_SalientImage() {
        mModuleView.setUseSalientImage(true);

        SuggestionEntry entry1 =
                new SuggestionEntry(
                        /* sourceName= */ "Desktop",
                        /* url= */ JUnitTestGURLs.GOOGLE_URL_DOG,
                        /* title= */ "Google Dog",
                        /* timestamp= */ makeTimestamp(24 - 3, 0, 0),
                        /* id= */ 90);
        mSuggestionBundle.entries.add(entry1);

        Assert.assertEquals(0, mTileContainerView.getChildCount());

        mModuleView.setSuggestionBundle(mSuggestionBundle);
        Assert.assertEquals(1, mTileContainerView.getChildCount());

        // Capture call to fetch image.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchSalientImageWithFallback(
                        mFetchImagePageUrlCaptor.capture(),
                        eq(true),
                        mFetchSalientImageCallbackCaptor.capture(),
                        mFetchImageCallbackCaptor.capture());
        Assert.assertEquals(1, mFetchImagePageUrlCaptor.getAllValues().size());
        Assert.assertEquals(1, mFetchSalientImageCallbackCaptor.getAllValues().size());
        Assert.assertEquals(
                JUnitTestGURLs.GOOGLE_URL_DOG, mFetchImagePageUrlCaptor.getAllValues().get(0));

        // Check tile texts.
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        Assert.assertEquals(
                "From Desktop", ((TextView) tile1.findViewById(R.id.tile_pre_info_text)).getText());
        Assert.assertEquals(
                "Google Dog", ((TextView) tile1.findViewById(R.id.tile_display_text)).getText());
        // Actual code would remove "www." prefix, but the test's JNI mock doesn't do so.
        Assert.assertEquals(
                "www.google.com \u2022 3 hr ago",
                ((TextView) tile1.findViewById(R.id.tile_post_info_text)).getText());

        // Image is not loaded yet.
        Assert.assertNull(((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable());

        // Provide test image, and check that it's shown as icon.
        Bitmap bitmap1 = makeBitmap(64, 64);
        mFetchSalientImageCallbackCaptor.getAllValues().get(0).onResult(bitmap1);
        BitmapDrawable drawable1 =
                (BitmapDrawable) ((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable();
        Assert.assertNotNull(drawable1);
        Assert.assertEquals(bitmap1, drawable1.getBitmap());

        // Simulate click.
        Assert.assertEquals(0, mClickCount);
        Assert.assertEquals(null, mLastClickUrl);
        tile1.performClick();
        Assert.assertEquals(1, mClickCount);
        Assert.assertEquals(JUnitTestGURLs.GOOGLE_URL_DOG, mLastClickUrl);
    }

    @Test
    @SmallTest
    public void testRenderSingleLocalView() {
        SuggestionEntry entry1 = new LocalTabSuggestionEntry(mTab);
        mSuggestionBundle.entries.add(entry1);

        Assert.assertEquals(0, mTileContainerView.getChildCount());

        mModuleView.setSuggestionBundle(mSuggestionBundle);
        Assert.assertEquals(1, mTileContainerView.getChildCount());

        // Capture call to fetch favicon.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchImageForUrl(
                        mFetchImagePageUrlCaptor.capture(), mFetchImageCallbackCaptor.capture());

        // Capture call to fetch tab thumbnail.
        verify(mThumbnailProvider, atLeastOnce())
                .getTabThumbnailWithCallback(
                        eq(TAB_ID),
                        eq(mThumbnailSize),
                        mThumbnailProviderCaptor.capture(),
                        /* forceUpdate= */ eq(true),
                        /* writeToCache= */ eq(true),
                        /* isSelected= */ eq(false));

        // Check tile texts.
        LocalTileView localTileView = (LocalTileView) mTileContainerView.getChildAt(0);
        Assert.assertEquals(
                TAB_TITLE, ((TextView) localTileView.findViewById(R.id.tab_title_view)).getText());
        // Actual code would remove "www." prefix, but the test's JNI mock doesn't do so.
        Assert.assertEquals(
                "www.one.com \u2022 3 hr ago",
                ((TextView) localTileView.findViewById(R.id.tab_url_view)).getText());
        // Verifies that a placeholder icon drawable is set for the tab thumbnail.
        Assert.assertNotNull(
                ((TabThumbnailView) localTileView.findViewById(R.id.tab_thumbnail))
                        .getIconDrawableForTesting());

        // Provide test image, and check that it's shown as icon.
        Bitmap expectedBitmap = makeBitmap(48, 48);
        mFetchImageCallbackCaptor.getAllValues().get(0).onBitmap(expectedBitmap);
        BitmapDrawable drawable =
                (BitmapDrawable)
                        ((ImageView) localTileView.findViewById(R.id.tab_favicon_view))
                                .getDrawable();
        Assert.assertNotNull(drawable);
        Assert.assertEquals(expectedBitmap, drawable.getBitmap());

        mThumbnailProviderCaptor.getAllValues().get(0).onResult(makeBitmap(64, 64));
        // Verifies that the placeholder icon drawable is removed after setting a foreground bitmap.
        Assert.assertNull(
                ((TabThumbnailView) localTileView.findViewById(R.id.tab_thumbnail))
                        .getIconDrawableForTesting());

        // Simulate click on the local Tab.
        Assert.assertEquals(0, mClickCount);
        Assert.assertEquals(0, mLastClickTabId);
        localTileView.performClick();
        Assert.assertEquals(1, mClickCount);
        Assert.assertEquals(TAB_ID, mLastClickTabId);
    }

    @Test
    @SmallTest
    public void testRenderDouble() {
        SuggestionEntry entry1 =
                new SuggestionEntry(
                        /* sourceName= */ "My Tablet",
                        /* url= */ JUnitTestGURLs.BLUE_3,
                        /* title= */ "Blue website with a very long title that might not fit",
                        /* timestamp= */ makeTimestamp(24 - 1, 60 - 16, 0),
                        /* id= */ 50);
        SuggestionEntry entry2 =
                new SuggestionEntry(
                        /* sourceName= */ "Desktop",
                        /* url= */ JUnitTestGURLs.GOOGLE_URL_DOG,
                        /* title= */ "Google Dog",
                        /* timestamp= */ makeTimestamp(24 - 3, 0, 0),
                        /* id= */ 90);
        mSuggestionBundle.entries.add(entry1);
        mSuggestionBundle.entries.add(entry2);

        Assert.assertEquals(0, mTileContainerView.getChildCount());

        mModuleView.setSuggestionBundle(mSuggestionBundle);
        Assert.assertEquals(3, mTileContainerView.getChildCount()); // 2 tiles, 1 divider.

        // Capture call to fetch image.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchImageForUrl(
                        mFetchImagePageUrlCaptor.capture(), mFetchImageCallbackCaptor.capture());
        Assert.assertEquals(2, mFetchImagePageUrlCaptor.getAllValues().size());
        Assert.assertEquals(2, mFetchImageCallbackCaptor.getAllValues().size());
        Assert.assertEquals(JUnitTestGURLs.BLUE_3, mFetchImagePageUrlCaptor.getAllValues().get(0));
        Assert.assertEquals(
                JUnitTestGURLs.GOOGLE_URL_DOG, mFetchImagePageUrlCaptor.getAllValues().get(1));

        // Check tiles texts, and presence of divider.
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        Assert.assertEquals(
                "Blue website with a very long title that might not fit",
                ((TextView) tile1.findViewById(R.id.tile_display_text)).getText());
        Assert.assertEquals(
                "16 min ago \u2022 From My Tablet",
                ((TextView) tile1.findViewById(R.id.tile_info_text)).getText());

        View divider = (View) mTileContainerView.getChildAt(1);
        Assert.assertEquals(View.VISIBLE, divider.getVisibility());

        TabResumptionTileView tile2 = (TabResumptionTileView) mTileContainerView.getChildAt(2);
        Assert.assertEquals(
                "Google Dog", ((TextView) tile2.findViewById(R.id.tile_display_text)).getText());
        Assert.assertEquals(
                "3 hr ago \u2022 From Desktop",
                ((TextView) tile2.findViewById(R.id.tile_info_text)).getText());

        // Images are not loaded yet.
        Assert.assertNull(((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable());
        Assert.assertNull(((ImageView) tile2.findViewById(R.id.tile_icon)).getDrawable());

        // Provide test images, and check that they're shown as icons.
        Bitmap bitmap1 = makeBitmap(48, 48);
        mFetchImageCallbackCaptor.getAllValues().get(0).onBitmap(bitmap1);
        BitmapDrawable drawable1 =
                (BitmapDrawable) ((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable();
        Assert.assertNotNull(drawable1);
        Assert.assertEquals(bitmap1, drawable1.getBitmap());

        Bitmap bitmap2 = makeBitmap(64, 64);
        mFetchImageCallbackCaptor.getAllValues().get(1).onBitmap(bitmap2);
        BitmapDrawable drawable2 =
                (BitmapDrawable) ((ImageView) tile2.findViewById(R.id.tile_icon)).getDrawable();
        Assert.assertNotNull(drawable2);
        Assert.assertEquals(bitmap2, drawable2.getBitmap());

        // Simulate click.
        Assert.assertEquals(0, mClickCount);
        Assert.assertEquals(null, mLastClickUrl);
        tile1.performClick();
        Assert.assertEquals(1, mClickCount);
        Assert.assertEquals(JUnitTestGURLs.BLUE_3, mLastClickUrl);
    }

    @Test
    @SmallTest
    public void testRenderDoubleWithLocalTab() {
        SuggestionEntry entry1 = new LocalTabSuggestionEntry(mTab);
        SuggestionEntry entry2 =
                new SuggestionEntry(
                        /* sourceName= */ "Desktop",
                        /* url= */ JUnitTestGURLs.GOOGLE_URL_DOG,
                        /* title= */ "Google Dog",
                        /* timestamp= */ makeTimestamp(24 - 3, 0, 0),
                        /* id= */ 90);
        mSuggestionBundle.entries.add(entry1);
        mSuggestionBundle.entries.add(entry2);

        Assert.assertEquals(0, mTileContainerView.getChildCount());

        mModuleView.setSuggestionBundle(mSuggestionBundle);
        Assert.assertEquals(3, mTileContainerView.getChildCount()); // 2 tiles, 1 divider.

        // Capture call to fetch image.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchImageForUrl(
                        mFetchImagePageUrlCaptor.capture(), mFetchImageCallbackCaptor.capture());
        Assert.assertEquals(2, mFetchImagePageUrlCaptor.getAllValues().size());
        Assert.assertEquals(2, mFetchImageCallbackCaptor.getAllValues().size());
        Assert.assertEquals(JUnitTestGURLs.URL_1, mFetchImagePageUrlCaptor.getAllValues().get(0));
        Assert.assertEquals(
                JUnitTestGURLs.GOOGLE_URL_DOG, mFetchImagePageUrlCaptor.getAllValues().get(1));

        // Check tiles texts, and presence of divider.
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        Assert.assertEquals(
                TAB_TITLE, ((TextView) tile1.findViewById(R.id.tile_display_text)).getText());
        Assert.assertEquals(
                "3 hr ago \u2022 Your last tab",
                ((TextView) tile1.findViewById(R.id.tile_info_text)).getText());

        View divider = (View) mTileContainerView.getChildAt(1);
        Assert.assertEquals(View.VISIBLE, divider.getVisibility());

        TabResumptionTileView tile2 = (TabResumptionTileView) mTileContainerView.getChildAt(2);
        Assert.assertEquals(
                "Google Dog", ((TextView) tile2.findViewById(R.id.tile_display_text)).getText());
        Assert.assertEquals(
                "3 hr ago \u2022 From Desktop",
                ((TextView) tile2.findViewById(R.id.tile_info_text)).getText());

        // Images are not loaded yet.
        Assert.assertNull(((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable());
        Assert.assertNull(((ImageView) tile2.findViewById(R.id.tile_icon)).getDrawable());

        // Provide test images, and check that they're shown as icons.
        Bitmap bitmap1 = makeBitmap(48, 48);
        mFetchImageCallbackCaptor.getAllValues().get(0).onBitmap(bitmap1);
        BitmapDrawable drawable1 =
                (BitmapDrawable) ((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable();
        Assert.assertNotNull(drawable1);
        Assert.assertEquals(bitmap1, drawable1.getBitmap());

        Bitmap bitmap2 = makeBitmap(64, 64);
        mFetchImageCallbackCaptor.getAllValues().get(1).onBitmap(bitmap2);
        BitmapDrawable drawable2 =
                (BitmapDrawable) ((ImageView) tile2.findViewById(R.id.tile_icon)).getDrawable();
        Assert.assertNotNull(drawable2);
        Assert.assertEquals(bitmap2, drawable2.getBitmap());

        Assert.assertEquals(0, mClickCount);
        Assert.assertEquals(null, mLastClickUrl);
        Assert.assertEquals(0, mLastClickTabId);

        // Simulate click on a local Tab.
        tile1.performClick();
        Assert.assertEquals(1, mClickCount);
        Assert.assertEquals(TAB_ID, mLastClickTabId);

        // Simulate click on a remote Tab.
        tile2.performClick();
        Assert.assertEquals(2, mClickCount);
        Assert.assertEquals(JUnitTestGURLs.GOOGLE_URL_DOG, mLastClickUrl);
    }
}
