// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridge;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridgeJni;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.commerce.core.ShoppingService;

/** Unit tests for {@link BookmarkPopupCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkPopupCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;

    @Mock private ShoppingService mShoppingService;
    @Mock private PriceDropNotificationManager mPriceDropNotificationManager;

    @Mock private BookmarkManagerOpener mBookmarkManagerOpener;
    @Mock private BookmarkModel mBookmarkModel;
    @Mock private FaviconHelperJni mFaviconHelperJni;
    @Mock private ImageServiceBridge.Natives mImageServiceBridgeJni;

    private Activity mActivity;
    private BookmarkPopupCoordinator mCoordinator;
    private View mAnchor;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        BookmarkModel.setInstanceForTesting(mBookmarkModel);
        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJni);
        Mockito.doReturn(1L).when(mFaviconHelperJni).init();
        ImageServiceBridgeJni.setInstanceForTesting(mImageServiceBridgeJni);
        mAnchor = new View(mActivity);
        mActivity.setContentView(mAnchor);

        mCoordinator =
                new BookmarkPopupCoordinator(
                        mActivity,
                        mProfile,
                        mAnchor,
                        mBookmarkManagerOpener,
                        mShoppingService,
                        mPriceDropNotificationManager);
    }

    @Test
    public void testShow() {
        BookmarkId bookmarkId = new BookmarkId(1, BookmarkType.NORMAL);
        // show() doesn't return anything or have observable side-effects easily mocked without
        // injecting Mediator,
        // but calling it ensures no crash happens.
        mCoordinator.show(bookmarkId, true);
    }

    @Test
    public void testPopupDismissesOnScreenSizeChange() {
        BookmarkId bookmarkId = new BookmarkId(1, BookmarkType.NORMAL);
        mCoordinator.show(bookmarkId, true);
        assertTrue(mCoordinator.getPopupWindowForTesting().isShowing());

        mAnchor.layout(0, 0, 500, 500);
        ShadowLooper.shadowMainLooper().idle();

        assertFalse(mCoordinator.getPopupWindowForTesting().isShowing());
    }
}
