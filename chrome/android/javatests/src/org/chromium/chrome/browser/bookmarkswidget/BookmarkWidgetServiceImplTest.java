// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarkswidget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.content.Context;
import android.content.SharedPreferences;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.RemoteViews;
import android.widget.TextView;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.FakeBookmarkModel;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.url.GURL;

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

    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private Context mContext;
    private FakeBookmarkModel mBookmarkModel;
    private BookmarkWidgetServiceImpl.BookmarkAdapter mFactory;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel = FakeBookmarkModel.createModel();
                    mFactory = new BookmarkWidgetServiceImpl.BookmarkAdapter(mContext, WIDGET_ID);
                });
        CriteriaHelper.pollUiThread(mBookmarkModel::isBookmarkModelLoaded);
        ThreadUtils.runOnUiThreadBlocking(() -> mFactory.onCreate());
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
        assertEquals(1, mFactory.getCount());

        RemoteViews views = mFactory.getViewAt(0);
        assertNotNull(views);

        FrameLayout parent = new FrameLayout(mContext);
        View itemView = views.apply(mContext, parent);

        TextView titleView = itemView.findViewById(R.id.title);
        ImageView favicon = itemView.findViewById(R.id.favicon);

        assertNotNull(titleView);
        assertNotNull(favicon);

        assertEquals(TEST_TITLE, titleView.getText().toString());
        assertEquals(
                "Favicon should be visible for a bookmark.", View.VISIBLE, favicon.getVisibility());
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
        assertEquals(1, mFactory.getCount());
        RemoteViews views = mFactory.getViewAt(0);
        assertNotNull(views);

        FrameLayout parent = new FrameLayout(mContext);
        View itemView = views.apply(mContext, parent);

        TextView titleView = itemView.findViewById(R.id.title);
        ImageView favicon = itemView.findViewById(R.id.favicon);

        assertNotNull(titleView);
        assertNotNull(favicon);

        assertEquals(FOLDER_TITLE, titleView.getText().toString());
        assertEquals(
                "Favicon should be visible for a bookmark folder.",
                View.VISIBLE,
                favicon.getVisibility());
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

        BookmarkItem subfolder =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mBookmarkModel.getBookmarkById(subfolderId));
        BookmarkItem parent =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mBookmarkModel.getBookmarkById(subfolder.getParentId()));

        RemoteViews remoteViews =
                BookmarkWidgetServiceImpl.createBookmarkWidgetRemoteView(
                        mContext, WIDGET_ID, subfolder, parent, 0);
        assertNotNull(remoteViews);

        FrameLayout parentView = new FrameLayout(mContext);
        View widgetView = remoteViews.apply(mContext, parentView);

        View upNavigation = widgetView.findViewById(R.id.up_navigation);
        ImageView backButton = widgetView.findViewById(R.id.back_button);
        TextView titleView = widgetView.findViewById(R.id.folder_title);

        assertNotNull(upNavigation);
        assertNotNull(backButton);
        assertNotNull(titleView);

        assertEquals(
                "Up navigation should be visible.", View.VISIBLE, upNavigation.getVisibility());
        assertEquals(
                "Title for up navigation item should be the current folder's title.",
                FOLDER_TITLE,
                titleView.getText().toString());
    }

    @Test
    @MediumTest
    public void testRootFolderNavigationItem_shouldNotBeVisible() {
        BookmarkItem rootFolder =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mBookmarkModel.getBookmarkById(mBookmarkModel.getRootFolderId()));

        RemoteViews remoteViews =
                BookmarkWidgetServiceImpl.createBookmarkWidgetRemoteView(
                        mContext, WIDGET_ID, rootFolder, null, 0);
        assertNotNull(remoteViews);

        FrameLayout parentView = new FrameLayout(mContext);
        View widgetView = remoteViews.apply(mContext, parentView);

        View upNavigation = widgetView.findViewById(R.id.up_navigation);
        assertNotNull(upNavigation);
        assertEquals(
                "Up navigation should be gone for root folder.",
                View.GONE,
                upNavigation.getVisibility());
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
}
