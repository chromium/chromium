// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.listmenu.ListMenuItemViewBinder;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

/**
 * Tests for ContextMenu item view, {@link ListMenuItemViewBinder}, and {@link
 * ContextMenuItemWithIconButtonViewBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ContextMenuItemViewTest {
    private static final String TEXT = "Useful menu item";
    private static final String APP = "Some app";

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    private View mShareItemView;
    private TextView mText;
    private ImageView mIcon;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mMCP;

    private boolean mIsClicked;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity.setContentView(R.layout.context_menu_row);
                    mShareItemView = sActivity.findViewById(android.R.id.content);
                    mText = mShareItemView.findViewById(R.id.menu_row_text);
                    mIcon = mShareItemView.findViewById(R.id.menu_row_share_icon);
                    mModel =
                            new PropertyModel.Builder(
                                            ContextMenuItemWithIconButtonProperties.ALL_KEYS)
                                    .with(ContextMenuItemWithIconButtonProperties.TITLE, "")
                                    .with(ContextMenuItemWithIconButtonProperties.ENABLED, true)
                                    .with(
                                            ContextMenuItemWithIconButtonProperties
                                                    .END_BUTTON_IMAGE,
                                            null)
                                    .with(
                                            ContextMenuItemWithIconButtonProperties
                                                    .END_BUTTON_CONTENT_DESC,
                                            "")
                                    .with(
                                            ContextMenuItemWithIconButtonProperties
                                                    .END_BUTTON_CLICK_LISTENER,
                                            null)
                                    .build();
                    mMCP =
                            PropertyModelChangeProcessor.create(
                                    mModel, mShareItemView, ContextMenuItemViewBinder::bind);
                });
    }

    @After
    public void tearDown() throws Exception {
        if (mMCP != null) ThreadUtils.runOnUiThreadBlocking(mMCP::destroy);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testText() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(ContextMenuItemWithIconButtonProperties.TITLE, TEXT));
        assertThat("Incorrect item text.", mText.getText(), equalTo(TEXT));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testShareIcon() {
        assertThat("Incorrect initial icon visibility.", mIcon.getVisibility(), equalTo(View.GONE));
        final Bitmap bitmap = Bitmap.createBitmap(4, 4, Bitmap.Config.ARGB_8888);
        final BitmapDrawable drawable = new BitmapDrawable(mIcon.getResources(), bitmap);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ContextMenuItemWithIconButtonProperties.END_BUTTON_IMAGE, drawable);
                    mModel.set(
                            ContextMenuItemWithIconButtonProperties.END_BUTTON_CONTENT_DESC, APP);
                });
        assertThat("Incorrect icon drawable.", mIcon.getDrawable(), equalTo(drawable));
        assertThat("Incorrect icon visibility.", mIcon.getVisibility(), equalTo(View.VISIBLE));
        assertThat(
                "Incorrect icon content description.",
                mIcon.getContentDescription(),
                equalTo(
                        mShareItemView
                                .getContext()
                                .getString(R.string.accessibility_menu_share_via, APP)));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testShareIconClick() {
        assertFalse(
                "Icon has onClickListeners when it shouldn't, yet, have.",
                mIcon.hasOnClickListeners());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(
                            ContextMenuItemWithIconButtonProperties.END_BUTTON_CLICK_LISTENER,
                            this::click);
                    mIcon.callOnClick();
                });
        assertTrue("Icon hasn't been clicked.", mIsClicked);
    }

    private void click(View v) {
        mIsClicked = true;
    }
}
