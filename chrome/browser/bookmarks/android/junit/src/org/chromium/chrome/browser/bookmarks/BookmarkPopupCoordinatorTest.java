// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridge;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;

/** Unit tests for {@link BookmarkPopupCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkPopupCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private BookmarkManagerOpener mBookmarkManagerOpener;
    @Mock private BookmarkModel mBookmarkModel;
    @Mock private FaviconHelperJni mFaviconHelperJni;
    @Mock private ImageServiceBridge.Natives mImageServiceBridgeJni;

    private Activity mActivity;
    private BookmarkPopupCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        BookmarkModel.setInstanceForTesting(mBookmarkModel);
        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJni);
        ImageServiceBridgeJni.setInstanceForTesting(mImageServiceBridgeJni);
        View anchor = new View(mActivity);

        mCoordinator =
                new BookmarkPopupCoordinator(mActivity, mProfile, anchor, mBookmarkManagerOpener);
    }

    @Test
    public void testShow() {
        BookmarkId bookmarkId = new BookmarkId(1, BookmarkType.NORMAL);
        // show() doesn't return anything or have observable side-effects easily mocked without
        // injecting Mediator,
        // but calling it ensures no crash happens.
        mCoordinator.show(bookmarkId, true);
    }
}
