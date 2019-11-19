// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivity;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Tests for RevampedContextMenu item view, {@link RevampedContextMenuItemViewBinder}, and {@link
 * RevampedContextMenuShareItemViewBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class RevampedContextMenuItemViewTest extends DummyUiActivityTestCase {
    private static final String TEXT = "Useful menu item";
    private static final String APP = "Some app";

    private View mShareItemView;
    private TextView mText;
    private ImageView mIcon;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mMCP;

    private boolean mIsClicked;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        DummyUiActivity.setTestLayout(org.chromium.chrome.R.layout.revamped_context_menu_share_row);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mShareItemView = getActivity().findViewById(android.R.id.content);
            mText = mShareItemView.findViewById(R.id.menu_row_text);
            mIcon = mShareItemView.findViewById(R.id.menu_row_share_icon);
        });
        mModel = new PropertyModel.Builder(RevampedContextMenuShareItemProperties.ALL_KEYS)
                         .with(RevampedContextMenuShareItemProperties.TEXT, "")
                         .with(RevampedContextMenuShareItemProperties.IMAGE, null)
                         .with(RevampedContextMenuShareItemProperties.CONTENT_DESC, "")
                         .with(RevampedContextMenuShareItemProperties.CLICK_LISTENER, null)
                         .build();
        mMCP = PropertyModelChangeProcessor.create(
                mModel, mShareItemView, RevampedContextMenuShareItemViewBinder::bind);
    }

    @Override
    public void tearDownTest() throws Exception {
        mMCP.destroy();
        super.tearDownTest();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testText() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(RevampedContextMenuShareItemProperties.TEXT, TEXT));
        assertThat("Incorrect item text.", mText.getText(), equalTo(TEXT));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testShareIcon() {
        assertThat("Incorrect initial icon visibility.", mIcon.getVisibility(), equalTo(View.GONE));
        final Bitmap bitmap = Bitmap.createBitmap(4, 4, Bitmap.Config.ARGB_8888);
        final BitmapDrawable drawable = new BitmapDrawable(mIcon.getResources(), bitmap);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(RevampedContextMenuShareItemProperties.IMAGE, drawable);
            mModel.set(RevampedContextMenuShareItemProperties.CONTENT_DESC, APP);
        });
        assertThat("Incorrect icon drawable.", mIcon.getDrawable(), equalTo(drawable));
        assertThat("Incorrect icon visibility.", mIcon.getVisibility(), equalTo(View.VISIBLE));
        assertThat("Incorrect icon content description.", mIcon.getContentDescription(),
                equalTo(mShareItemView.getContext().getString(
                        R.string.accessibility_menu_share_via, APP)));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testShareIconClick() {
        assertFalse("Icon has onClickListeners when it shouldn't, yet, have.",
                mIcon.hasOnClickListeners());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(RevampedContextMenuShareItemProperties.CLICK_LISTENER, this::click);
            mIcon.callOnClick();
        });
        assertTrue("Icon hasn't been clicked.", mIsClicked);
    }

    private void click(View v) {
        mIsClicked = true;
    }
}
