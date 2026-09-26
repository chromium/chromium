// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.mockito.Mockito.verify;

import android.graphics.drawable.Drawable;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Tests for {@link BottomSheetToolbarViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomSheetToolbarViewBinderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock BottomSheetToolbarView mToolbarView;
    private @Mock Drawable mDrawable;
    private PropertyModel mItemViewModel;
    private PropertyModelChangeProcessor mItemMCP;

    @Before
    public void setUp() {
        mItemViewModel =
                new PropertyModel.Builder(BottomSheetToolbarProperties.ALL_KEYS)
                        .with(BottomSheetToolbarProperties.FAVICON_ICON_VISIBLE, true)
                        .with(BottomSheetToolbarProperties.OPEN_IN_NEW_TAB_VISIBLE, false)
                        .build();

        mItemMCP =
                PropertyModelChangeProcessor.create(
                        mItemViewModel, mToolbarView, BottomSheetToolbarViewBinder::bind);
    }

    @Test
    public void testSetTitle() {
        String title = "titleText";
        mItemViewModel.set(BottomSheetToolbarProperties.TITLE, title);
        verify(mToolbarView).setTitle(title);
    }

    @Test
    public void testSetUrl() {
        GURL url = JUnitTestGURLs.EXAMPLE_URL;
        mItemViewModel.set(BottomSheetToolbarProperties.URL, url);
        verify(mToolbarView).setUrl(url);
    }

    @Test
    public void testSetSecurityIconDescription() {
        String content = "contentText";
        mItemViewModel.set(BottomSheetToolbarProperties.SECURITY_ICON_CONTENT_DESCRIPTION, content);
        verify(mToolbarView).setSecurityIconDescription(content);
    }

    @Test
    public void testSetSecurityIconClickCallback() {
        Runnable callback = () -> {};
        mItemViewModel.set(BottomSheetToolbarProperties.SECURITY_ICON_ON_CLICK_CALLBACK, callback);
        verify(mToolbarView).setSecurityIconClickCallback(callback);
    }

    @Test
    public void testCloseButtonClickCallback() {
        Runnable callback = () -> {};
        mItemViewModel.set(BottomSheetToolbarProperties.CLOSE_BUTTON_ON_CLICK_CALLBACK, callback);
        verify(mToolbarView).setCloseButtonClickCallback(callback);
    }

    @Test
    public void testSetProgress() {
        float progress = 0.2f;
        mItemViewModel.set(BottomSheetToolbarProperties.LOAD_PROGRESS, progress);
        verify(mToolbarView).setProgress(progress);
    }

    @Test
    public void testSetProgressVisible() {
        mItemViewModel.set(BottomSheetToolbarProperties.PROGRESS_VISIBLE, false);
        verify(mToolbarView).setProgressVisible(false);

        mItemViewModel.set(BottomSheetToolbarProperties.PROGRESS_VISIBLE, true);
        verify(mToolbarView).setProgressVisible(true);
    }

    @Test
    public void testSetFaviconIconDrawable() {
        mItemViewModel.set(BottomSheetToolbarProperties.FAVICON_ICON_DRAWABLE, mDrawable);
        verify(mToolbarView).setFaviconIconDrawable(mDrawable);
    }

    @Test
    public void testSetFaviconIconVisible() {
        verify(mToolbarView).setFaviconIconVisible(true);

        mItemViewModel.set(BottomSheetToolbarProperties.FAVICON_ICON_VISIBLE, false);
        verify(mToolbarView).setFaviconIconVisible(false);
    }

    @Test
    public void testSetOpenInNewTabButtonVisible() {
        verify(mToolbarView).setOpenInNewTabButtonVisible(false);

        mItemViewModel.set(BottomSheetToolbarProperties.OPEN_IN_NEW_TAB_VISIBLE, true);
        verify(mToolbarView).setOpenInNewTabButtonVisible(true);
    }

    @After
    public void tearDownTest() {
        mItemMCP.destroy();
    }
}
