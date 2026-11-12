// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarkswidget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.RemoteViews;
import android.widget.TextView;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.FakeBookmarkModel;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/** Tests for the BookmarkWidgetServiceImpl, which populates the widget's list view. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
public class BookmarkWidgetServiceImplTest {
    private static final int WIDGET_ID = 1;
    private static final String TEST_TITLE = "Test Bookmark Title";
    private static final String FOLDER_TITLE = "Test Bookmark Folder";
    private static final GURL TEST_URL = new GURL("https://www.test.com");

    private Context mContext;
    private FakeBookmarkModel mBookmarkModel;
    private BookmarkWidgetServiceImpl.BookmarkAdapter mFactory;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel = FakeBookmarkModel.createModel();
                    mFactory = new BookmarkWidgetServiceImpl.BookmarkAdapter(mContext, WIDGET_ID);
                });
        CriteriaHelper.pollUiThread(mBookmarkModel::isBookmarkModelLoaded);
        ThreadUtils.runOnUiThreadBlocking(() -> mFactory.onCreate());
    }

    @After
    public void tearDown() {
        if (mFactory != null) {
            mFactory.onDestroy();
        }
    }

    @Test
    @MediumTest
    public void testBookmark() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BookmarkId otherBookmarksFolder = mBookmarkModel.getOtherFolderId();
                    mBookmarkModel.addBookmark(otherBookmarksFolder, 0, TEST_TITLE, TEST_URL);
                    SharedPreferences prefs = BookmarkWidgetServiceImpl.getWidgetState(WIDGET_ID);
                    prefs.edit()
                            .putString(
                                    "bookmarkswidget.current_folder",
                                    otherBookmarksFolder.toString())
                            .apply();
                });

        mFactory.onDataSetChanged();

        // Position 0 is the label for the bookmark folder, so bookmark is at position 1.
        RemoteViews views = mFactory.getViewAt(1);
        assertNotNull(views);

        FrameLayout parent = new FrameLayout(mContext);
        View itemView = views.apply(mContext, parent);

        TextView titleView = itemView.findViewById(R.id.title);
        ImageView favicon = itemView.findViewById(R.id.favicon);
        ImageView backButton = itemView.findViewById(R.id.back_button);

        assertNotNull(titleView);
        assertNotNull(favicon);
        assertNotNull(backButton);

        assertEquals(TEST_TITLE, titleView.getText().toString());
        assertEquals(
                "Favicon should be visible for a bookmark.", View.VISIBLE, favicon.getVisibility());
        assertEquals(
                "Back button should be hidden for a bookmark.",
                View.GONE,
                backButton.getVisibility());
    }

    @Test
    @MediumTest
    public void testBookmarkWithNtpUrl() {
        // LargeIconBridge declines favicon requests for NTP URLs without running the callback.
        // The widget must still finish loading, reset mIconGenerator's fallback color, and cache
        // the generated fallback icon in mFaviconCache.
        GURL ntpUrl = new GURL("chrome://newtab/");
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BookmarkId otherBookmarksFolder = mBookmarkModel.getOtherFolderId();
                    mBookmarkModel.addBookmark(otherBookmarksFolder, 0, "Regular", TEST_URL);
                    mBookmarkModel.addBookmark(otherBookmarksFolder, 1, TEST_TITLE, ntpUrl);
                    SharedPreferences prefs = BookmarkWidgetServiceImpl.getWidgetState(WIDGET_ID);
                    prefs.edit()
                            .putString(
                                    "bookmarkswidget.current_folder",
                                    otherBookmarksFolder.toString())
                            .apply();
                });

        mFactory.onDataSetChanged();

        // Bind the regular bookmark first so mIconGenerator's background color is exercised.
        assertNotNull(mFactory.getViewAt(1));

        // Position 2 is the NTP bookmark (after position 0 up-navigation and position 1 Regular).
        RemoteViews views = mFactory.getViewAt(2);
        assertNotNull(views);

        View itemView = views.apply(mContext, new FrameLayout(mContext));
        TextView titleView = itemView.findViewById(R.id.title);
        ImageView favicon = itemView.findViewById(R.id.favicon);

        assertNotNull(titleView);
        assertNotNull(favicon);
        assertEquals(TEST_TITLE, titleView.getText().toString());
        assertEquals(
                "Fallback favicon should be visible for an NTP bookmark.",
                View.VISIBLE,
                favicon.getVisibility());

        Bitmap firstBitmap = ((BitmapDrawable) favicon.getDrawable()).getBitmap();
        assertNotNull(firstBitmap);

        // Fetching the NTP favicon again should hit mFaviconCache and return the same Bitmap
        // instance.
        Bitmap cachedBitmap = mFactory.loadFavicon(ntpUrl);
        assertNotNull(cachedBitmap);
        assertTrue(firstBitmap.sameAs(cachedBitmap));
        assertSame(
                "Declined URL fallback icon should be cached in mFaviconCache.",
                cachedBitmap,
                mFactory.loadFavicon(ntpUrl));
    }

    @Test
    @MediumTest
    public void testFolder() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BookmarkId otherBookmarksFolder = mBookmarkModel.getOtherFolderId();
                    mBookmarkModel.addFolder(otherBookmarksFolder, 0, FOLDER_TITLE);
                    SharedPreferences prefs = BookmarkWidgetServiceImpl.getWidgetState(WIDGET_ID);
                    prefs.edit()
                            .putString(
                                    "bookmarkswidget.current_folder",
                                    otherBookmarksFolder.toString())
                            .apply();
                });

        mFactory.onDataSetChanged();

        // Position 0 is the label for the bookmark folder, so bookmark folder is at position 1.
        RemoteViews views = mFactory.getViewAt(1);
        assertNotNull(views);

        FrameLayout parent = new FrameLayout(mContext);
        View itemView = views.apply(mContext, parent);

        TextView titleView = itemView.findViewById(R.id.title);
        ImageView favicon = itemView.findViewById(R.id.favicon);
        ImageView backButton = itemView.findViewById(R.id.back_button);

        assertNotNull(titleView);
        assertNotNull(favicon);
        assertNotNull(backButton);

        assertEquals(FOLDER_TITLE, titleView.getText().toString());
        assertEquals(
                "Favicon should be visible for a bookmark folder.",
                View.VISIBLE,
                favicon.getVisibility());
        assertEquals(
                "Back button should be hidden for a bookmark folder.",
                View.GONE,
                backButton.getVisibility());
    }

    @Test
    @MediumTest
    public void testParentFolderNavigationItem() {
        BookmarkId subfolderId =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            BookmarkId mobileFolderId = mBookmarkModel.getMobileFolderId();
                            return mBookmarkModel.addFolder(mobileFolderId, 0, FOLDER_TITLE);
                        });

        SharedPreferences prefs = BookmarkWidgetServiceImpl.getWidgetState(WIDGET_ID);
        prefs.edit().putString("bookmarkswidget.current_folder", subfolderId.toString()).apply();

        mFactory.onDataSetChanged();

        // Subfolder with no bookmarks has 2 items: 1 up-navigation header + 1 empty message item.
        assertEquals(
                "Factory should have a parent folder navigation item plus the empty message item.",
                2,
                mFactory.getCount());

        RemoteViews views = mFactory.getViewAt(0);
        assertNotNull(views);

        FrameLayout parent = new FrameLayout(mContext);

        View itemView = views.apply(mContext, parent);

        TextView titleView = itemView.findViewById(R.id.title);
        ImageView favicon = itemView.findViewById(R.id.favicon);
        ImageView backButton = itemView.findViewById(R.id.back_button);

        assertNotNull(titleView);
        assertNotNull(favicon);
        assertNotNull(backButton);

        assertEquals(
                "Title for parent folder navigation item should be the current folder's title.",
                FOLDER_TITLE,
                titleView.getText().toString());
        assertEquals("Favicon should be hidden.", View.GONE, favicon.getVisibility());
        assertEquals("Back button should be visible.", View.VISIBLE, backButton.getVisibility());
    }

    @Test
    @MediumTest
    public void testEmptyFolder() {
        BookmarkId subfolderId =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            BookmarkId mobileFolderId = mBookmarkModel.getMobileFolderId();
                            return mBookmarkModel.addFolder(mobileFolderId, 0, FOLDER_TITLE);
                        });

        SharedPreferences prefs = BookmarkWidgetServiceImpl.getWidgetState(WIDGET_ID);
        prefs.edit().putString("bookmarkswidget.current_folder", subfolderId.toString()).apply();

        mFactory.onDataSetChanged();

        // The empty message is delivered atomically inside the collection (position 1) to eliminate
        // any out-of-band IPC desync.
        assertEquals(
                "Empty subfolder should report 2 items (up-navigation header + empty message)",
                2,
                mFactory.getCount());

        RemoteViews emptyViews = mFactory.getViewAt(1);
        assertNotNull(emptyViews);
        assertEquals(R.layout.bookmark_widget_empty_item, emptyViews.getLayoutId());

        View emptyItemView = emptyViews.apply(mContext, new FrameLayout(mContext));
        TextView emptyMessage = emptyItemView.findViewById(R.id.empty_message);
        assertNotNull(emptyMessage);
        assertEquals(
                mContext.getString(R.string.bookmark_widget_empty_msg),
                emptyMessage.getText().toString());
    }

    @Test
    @MediumTest
    public void testRootFolder_withoutAccountBookmarkFoldersActive() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Point the widget to the root folder.
                    SharedPreferences prefs = BookmarkWidgetServiceImpl.getWidgetState(WIDGET_ID);
                    prefs.edit()
                            .putString(
                                    "bookmarkswidget.current_folder",
                                    mBookmarkModel.getRootFolderId().toString())
                            .apply();
                });

        mFactory.onDataSetChanged();

        assertEquals(
                "Widget item count should be one of each bookmark folder type",
                4,
                mFactory.getCount());
    }

    @Test
    @MediumTest
    public void testRootFolder_withAccountBookmarkFoldersActive() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Point the widget to the root folder.
                    SharedPreferences prefs = BookmarkWidgetServiceImpl.getWidgetState(WIDGET_ID);
                    prefs.edit()
                            .putString(
                                    "bookmarkswidget.current_folder",
                                    mBookmarkModel.getRootFolderId().toString())
                            .apply();
                    // Enable account bookmark folders.
                    mBookmarkModel.setAreAccountBookmarkFoldersActive(true);
                });

        mFactory.onDataSetChanged();

        assertEquals(
                "Widget item count should be 2x each bookmark folder type and 2 section headers.",
                10,
                mFactory.getCount());

        // Check the first header for account bookmark folders ("In your Google Account")
        RemoteViews header1 = mFactory.getViewAt(0);
        assertNotNull(header1);
        assertEquals(R.layout.bookmark_widget_section_header, header1.getLayoutId());
        View header1View = header1.apply(mContext, new FrameLayout(mContext));
        TextView header1Title = header1View.findViewById(R.id.title);
        assertEquals("In your Google Account", header1Title.getText().toString());

        // Check the second header for local bookmark folders ("Only on this device")
        RemoteViews header2 = mFactory.getViewAt(5);
        assertNotNull(header2);
        assertEquals(R.layout.bookmark_widget_section_header, header2.getLayoutId());
        View header2View = header2.apply(mContext, new FrameLayout(mContext));
        TextView header2Title = header2View.findViewById(R.id.title);
        assertEquals("Only on this device", header2Title.getText().toString());
    }

    @Test
    @MediumTest
    public void testManyBookmarks_doesNotRetainFaviconBitmaps() {
        int bookmarkCount = 50;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BookmarkId otherBookmarksFolder = mBookmarkModel.getOtherFolderId();
                    for (int i = 0; i < bookmarkCount; i++) {
                        mBookmarkModel.addBookmark(
                                otherBookmarksFolder,
                                i,
                                "Bookmark " + i,
                                new GURL("https://www.example.com/page" + i));
                    }
                    SharedPreferences prefs = BookmarkWidgetServiceImpl.getWidgetState(WIDGET_ID);
                    prefs.edit()
                            .putString(
                                    "bookmarkswidget.current_folder",
                                    otherBookmarksFolder.toString())
                            .apply();
                });

        mFactory.onDataSetChanged();

        // Position 0 is the parent folder navigation row, followed by the 50 bookmark rows.
        assertEquals(
                "Factory should report the parent navigation row plus all bookmarks.",
                bookmarkCount + 1,
                mFactory.getCount());

        List<WeakReference<Bitmap>> faviconRefs = bindBookmarkViewsAndCollectFavicons(32);
        assertEquals(
                "Each bound bookmark view should have produced a favicon bitmap.",
                32,
                faviconRefs.size());

        // While mFactory is still live (widget active on the home screen), BookmarkAdapter must
        // bound its LRU favicon cache and allow evicted favicon bitmaps (and all 18 unrendered
        // bookmarks) to be garbage collected after the RemoteViews are consumed.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Runtime.getRuntime().gc();
                    System.runFinalization();
                    for (int i = 0; i < 16; i++) {
                        Criteria.checkThat(
                                "Live BookmarkAdapter should evict older favicon bitmaps.",
                                faviconRefs.get(i).get(),
                                Matchers.nullValue());
                    }
                });
    }

    private List<WeakReference<Bitmap>> bindBookmarkViewsAndCollectFavicons(int count) {
        List<WeakReference<Bitmap>> refs = new ArrayList<>();
        for (int i = 1; i <= count; i++) {
            RemoteViews views = mFactory.getViewAt(i);
            assertNotNull(views);

            View itemView = views.apply(mContext, new FrameLayout(mContext));
            TextView titleView = itemView.findViewById(R.id.title);
            ImageView favicon = itemView.findViewById(R.id.favicon);

            assertNotNull(titleView);
            assertNotNull(favicon);
            assertEquals("Bookmark " + (i - 1), titleView.getText().toString());
            assertEquals(View.VISIBLE, favicon.getVisibility());

            BitmapDrawable drawable = (BitmapDrawable) favicon.getDrawable();
            assertNotNull(drawable);
            Bitmap bitmap = drawable.getBitmap();
            assertNotNull(bitmap);
            assertTrue(bitmap.getAllocationByteCount() > 0);
            refs.add(new WeakReference<>(bitmap));
        }
        return refs;
    }
}
