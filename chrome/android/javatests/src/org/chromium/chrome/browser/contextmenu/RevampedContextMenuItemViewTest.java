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
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.UiThreadTest;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.DummyUiActivity;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

/**
 * Tests for RevampedContextMenu item view, {@link RevampedContextMenuItemViewBinder}, and {@link
 * RevampedContextMenuItemWithIconButtonViewBinder}.
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
            mModel = new PropertyModel
                             .Builder(RevampedContextMenuItemWithIconButtonProperties.ALL_KEYS)
                             .with(RevampedContextMenuItemWithIconButtonProperties.TEXT, "")
                             .with(RevampedContextMenuItemWithIconButtonProperties.BUTTON_IMAGE,
                                     null)
                             .with(RevampedContextMenuItemWithIconButtonProperties
                                             .BUTTON_CONTENT_DESC,
                                     "")
                             .with(RevampedContextMenuItemWithIconButtonProperties
                                             .BUTTON_CLICK_LISTENER,
                                     null)
                             .build();
            mMCP = PropertyModelChangeProcessor.create(
                    mModel, mShareItemView, RevampedContextMenuItemWithIconButtonViewBinder::bind);
        });
    }

    @Override
    public void tearDownTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(mMCP::destroy);
        super.tearDownTest();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testText() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(RevampedContextMenuItemWithIconButtonProperties.TEXT, TEXT));
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
            mModel.set(RevampedContextMenuItemWithIconButtonProperties.BUTTON_IMAGE, drawable);
            mModel.set(RevampedContextMenuItemWithIconButtonProperties.BUTTON_CONTENT_DESC, APP);
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
            mModel.set(RevampedContextMenuItemWithIconButtonProperties.BUTTON_CLICK_LISTENER,
                    this::click);
            mIcon.callOnClick();
        });
        assertTrue("Icon hasn't been clicked.", mIsClicked);
    }

    private void click(View v) {
        mIsClicked = true;
    }
}
