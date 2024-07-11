// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/** Tests for ContextMenuHeader view and {@link ContextMenuHeaderViewBinder} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ContextMenuHeaderViewTest extends BlankUiTestActivityTestCase {
    private static final String TITLE_STRING = "Some Very Cool Title";
    private static final String URL_STRING = "www.website.com";

    private View mHeaderView;
    private TextView mTitle;
    private TextView mUrl;
    private View mTitleAndUrl;
    private ImageView mImage;
    private View mCircleBg;
    private View mImageContainer;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mMCP;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().setContentView(R.layout.context_menu_header);
                    mHeaderView = getActivity().findViewById(android.R.id.content);
                    mTitle = mHeaderView.findViewById(R.id.menu_header_title);
                    mUrl = mHeaderView.findViewById(R.id.menu_header_url);
                    mTitleAndUrl = mHeaderView.findViewById(R.id.title_and_url);
                    mImage = mHeaderView.findViewById(R.id.menu_header_image);
                    mCircleBg = mHeaderView.findViewById(R.id.circle_background);
                    mImageContainer = mHeaderView.findViewById(R.id.menu_header_image_container);
                    mModel =
                            new PropertyModel.Builder(ContextMenuHeaderProperties.ALL_KEYS)
                                    .with(ContextMenuHeaderProperties.TITLE, "")
                                    .with(ContextMenuHeaderProperties.URL, "")
                                    .with(
                                            ContextMenuHeaderProperties
                                                    .TITLE_AND_URL_CLICK_LISTENER,
                                            null)
                                    .with(ContextMenuHeaderProperties.IMAGE, null)
                                    .with(ContextMenuHeaderProperties.CIRCLE_BG_VISIBLE, false)
                                    .build();

                    mMCP =
                            PropertyModelChangeProcessor.create(
                                    mModel, mHeaderView, ContextMenuHeaderViewBinder::bind);
                });
    }

    @Override
    public void tearDownTest() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(mMCP::destroy);
        super.tearDownTest();
    }

    @Test
    @SmallTest
    public void testTitle() {
        assertThat(
                "Incorrect initial title visibility.", mTitle.getVisibility(), equalTo(View.GONE));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ContextMenuHeaderProperties.TITLE, TITLE_STRING);
                    mModel.set(ContextMenuHeaderProperties.TITLE_MAX_LINES, 2);
                });

        assertThat("Incorrect title visibility.", mTitle.getVisibility(), equalTo(View.VISIBLE));
        assertThat("Incorrect title string.", mTitle.getText(), equalTo(TITLE_STRING));
        assertThat("Incorrect max line count for title.", mTitle.getMaxLines(), equalTo(2));
        assertThat(
                "Incorrect title ellipsize mode.",
                mTitle.getEllipsize(),
                equalTo(TextUtils.TruncateAt.END));
    }

    @Test
    @SmallTest
    public void testUrl() {
        assertThat("Incorrect initial URL visibility.", mUrl.getVisibility(), equalTo(View.GONE));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ContextMenuHeaderProperties.URL, URL_STRING);
                    mModel.set(ContextMenuHeaderProperties.URL_MAX_LINES, 1);
                });

        assertThat("Incorrect URL visibility.", mUrl.getVisibility(), equalTo(View.VISIBLE));
        assertThat("Incorrect URL string.", mUrl.getText(), equalTo(URL_STRING));
        assertThat("Incorrect max line count for URL.", mUrl.getMaxLines(), equalTo(1));
        assertThat(
                "Incorrect URL ellipsize mode.",
                mUrl.getEllipsize(),
                equalTo(TextUtils.TruncateAt.END));

        ThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(ContextMenuHeaderProperties.URL_MAX_LINES, Integer.MAX_VALUE));

        assertThat(
                "Incorrect max line count for URL.",
                mUrl.getMaxLines(),
                equalTo(Integer.MAX_VALUE));
        assertNull("URL is ellipsized when it shouldn't be.", mUrl.getEllipsize());
    }

    @Test
    @SmallTest
    public void testTitleAndUrlClick() {
        // Clicking on the title or the URL expands/shrinks both of them.
        assertFalse(
                "Title and URL have onClickListeners when it shouldn't, yet, have.",
                mTitleAndUrl.hasOnClickListeners());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ContextMenuHeaderProperties.TITLE, TITLE_STRING);
                    mModel.set(ContextMenuHeaderProperties.TITLE_MAX_LINES, 1);
                    mModel.set(ContextMenuHeaderProperties.URL, URL_STRING);
                    mModel.set(ContextMenuHeaderProperties.URL_MAX_LINES, 1);
                    mModel.set(
                            ContextMenuHeaderProperties.TITLE_AND_URL_CLICK_LISTENER,
                            (v) -> {
                                if (mModel.get(ContextMenuHeaderProperties.URL_MAX_LINES)
                                        == Integer.MAX_VALUE) {
                                    mModel.set(ContextMenuHeaderProperties.TITLE_MAX_LINES, 1);
                                    mModel.set(ContextMenuHeaderProperties.URL_MAX_LINES, 1);
                                } else {
                                    mModel.set(
                                            ContextMenuHeaderProperties.TITLE_MAX_LINES,
                                            Integer.MAX_VALUE);
                                    mModel.set(
                                            ContextMenuHeaderProperties.URL_MAX_LINES,
                                            Integer.MAX_VALUE);
                                }
                            });
                    mTitleAndUrl.callOnClick();
                });

        assertThat(
                "Incorrect max line count for title.",
                mTitle.getMaxLines(),
                equalTo(Integer.MAX_VALUE));
        assertNull("Title is ellipsized when it shouldn't be.", mTitle.getEllipsize());
        assertThat(
                "Incorrect max line count for URL.",
                mUrl.getMaxLines(),
                equalTo(Integer.MAX_VALUE));
        assertNull("URL is ellipsized when it shouldn't be.", mUrl.getEllipsize());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTitleAndUrl.callOnClick();
                });

        assertThat("Incorrect max line count for title.", mTitle.getMaxLines(), equalTo(1));
        assertThat(
                "Incorrect title ellipsize mode.",
                mTitle.getEllipsize(),
                equalTo(TextUtils.TruncateAt.END));
        assertThat("Incorrect max line count for URL.", mUrl.getMaxLines(), equalTo(1));
        assertThat(
                "Incorrect URL ellipsize mode.",
                mUrl.getEllipsize(),
                equalTo(TextUtils.TruncateAt.END));
    }

    @Test
    @SmallTest
    public void testImage() {
        assertThat(
                "Incorrect initial circle background visibility.",
                mCircleBg.getVisibility(),
                equalTo(View.INVISIBLE));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(ContextMenuHeaderProperties.CIRCLE_BG_VISIBLE, true));
        assertThat(
                "Incorrect circle background visibility.",
                mCircleBg.getVisibility(),
                equalTo(View.VISIBLE));

        assertFalse(
                "Thumbnail drawable should use fallback color initially.",
                mImage.getDrawable() instanceof BitmapDrawable);
        final Bitmap bitmap = Bitmap.createBitmap(4, 4, Bitmap.Config.ARGB_8888);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(ContextMenuHeaderProperties.IMAGE, bitmap));
        assertThat(
                "Incorrect thumbnail bitmap.",
                ((BitmapDrawable) mImage.getDrawable()).getBitmap(),
                equalTo(bitmap));
    }
}
