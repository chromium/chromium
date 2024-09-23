// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.junit.Assert.assertEquals;

import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.url.GURL;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for {@link BottomSheetToolbarViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class BottomSheetToolbarViewBinderTest extends BlankUiTestActivityTestCase {
    private final AtomicBoolean mIconClicked = new AtomicBoolean();

    private BottomSheetToolbarView mItemView;
    private PropertyModel mItemViewModel;
    private PropertyModelChangeProcessor mItemMCP;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        ViewGroup view = new FrameLayout(getActivity());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().setContentView(view);

                    mItemView = new BottomSheetToolbarView(getActivity());
                    view.addView(mItemView.getView());

                    mItemViewModel =
                            new PropertyModel.Builder(BottomSheetToolbarProperties.ALL_KEYS)
                                    .with(BottomSheetToolbarProperties.FAVICON_ICON_VISIBLE, true)
                                    .with(
                                            BottomSheetToolbarProperties.OPEN_IN_NEW_TAB_VISIBLE,
                                            false)
                                    .build();

                    mItemMCP =
                            PropertyModelChangeProcessor.create(
                                    mItemViewModel, mItemView, BottomSheetToolbarViewBinder::bind);
                });
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetTitle() {
        TextView toolbarText = mItemView.getView().findViewById(R.id.title);
        assertEquals("", toolbarText.getText());

        String title = "titleText";
        mItemViewModel.set(BottomSheetToolbarProperties.TITLE, title);
        assertEquals(title, toolbarText.getText());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetUrl() {
        TextView originView = mItemView.getView().findViewById(R.id.origin);
        assertEquals("", originView.getText());

        GURL url = new GURL("www.test.com");
        mItemViewModel.set(BottomSheetToolbarProperties.URL, url);
        assertEquals(
                UrlFormatter.formatUrlForSecurityDisplay(url, SchemeDisplay.OMIT_HTTP_AND_HTTPS),
                originView.getText());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetSecurityIconDescription() {
        ImageView securityIcon = mItemView.getView().findViewById(R.id.security_icon);
        String content = "contentText";
        mItemViewModel.set(BottomSheetToolbarProperties.SECURITY_ICON_CONTENT_DESCRIPTION, content);
        assertEquals(content, securityIcon.getContentDescription());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetSecurityIconClickCallback() {
        ImageView securityIcon = mItemView.getView().findViewById(R.id.security_icon);
        mIconClicked.set(false);
        mItemViewModel.set(
                BottomSheetToolbarProperties.SECURITY_ICON_ON_CLICK_CALLBACK,
                () -> mIconClicked.set(true));
        securityIcon.performClick();
        assertEquals(true, mIconClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testCloseButtonClickCallback() {
        ImageView closeButton = mItemView.getView().findViewById(R.id.close);
        mIconClicked.set(false);
        mItemViewModel.set(
                BottomSheetToolbarProperties.CLOSE_BUTTON_ON_CLICK_CALLBACK,
                () -> mIconClicked.set(true));
        closeButton.performClick();
        assertEquals(true, mIconClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetProgress() {
        ProgressBar progressBar = mItemView.getView().findViewById(R.id.progress_bar);
        assertEquals(0f, progressBar.getProgress(), 0.1);

        float progress = 0.2f;
        mItemViewModel.set(BottomSheetToolbarProperties.LOAD_PROGRESS, progress);
        assertEquals(Math.round(progress * 100), progressBar.getProgress());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetProgressVisible() {
        ProgressBar progressBar = mItemView.getView().findViewById(R.id.progress_bar);
        mItemViewModel.set(BottomSheetToolbarProperties.PROGRESS_VISIBLE, false);
        assertEquals(View.GONE, progressBar.getVisibility());

        mItemViewModel.set(BottomSheetToolbarProperties.PROGRESS_VISIBLE, true);
        assertEquals(View.VISIBLE, progressBar.getVisibility());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetFaviconIconDrawable() {
        ImageView faviconIcon = mItemView.getView().findViewById(R.id.favicon);
        assertEquals(null, faviconIcon.getDrawable());

        Drawable iconDrawable =
                AppCompatResources.getDrawable(getActivity(), R.drawable.ic_globe_24dp);
        mItemViewModel.set(BottomSheetToolbarProperties.FAVICON_ICON_DRAWABLE, iconDrawable);
        assertEquals(iconDrawable, faviconIcon.getDrawable());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetFaviconIconVisible() {
        ImageView faviconIcon = mItemView.getView().findViewById(R.id.favicon);
        assertEquals(View.VISIBLE, faviconIcon.getVisibility());

        mItemViewModel.set(BottomSheetToolbarProperties.FAVICON_ICON_VISIBLE, false);
        assertEquals(View.GONE, faviconIcon.getVisibility());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetOpenInNewTabButtonVisible() {
        ImageView openInNewTabButton = mItemView.getView().findViewById(R.id.open_in_new_tab);
        assertEquals(View.GONE, openInNewTabButton.getVisibility());

        mItemViewModel.set(BottomSheetToolbarProperties.OPEN_IN_NEW_TAB_VISIBLE, true);
        assertEquals(View.VISIBLE, openInNewTabButton.getVisibility());
    }

    @Override
    public void tearDownTest() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(mItemMCP::destroy);
        super.tearDownTest();
    }
}
