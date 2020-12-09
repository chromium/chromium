// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.res.ColorStateList;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.UiThreadTest;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for {@link TabGroupUiViewBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabGroupUiViewBinderTest extends DummyUiActivityTestCase {
    private ImageView mLeftButton;
    private ImageView mRightButton;
    private ViewGroup mContainerView;
    private View mMainContent;

    private PropertyModel mModel;
    private PropertyModelChangeProcessor mMCP;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        ViewGroup parentView = new FrameLayout(getActivity());
        TabGroupUiToolbarView toolbarView =
                (TabGroupUiToolbarView) LayoutInflater.from(getActivity())
                        .inflate(R.layout.bottom_tab_strip_toolbar, parentView, false);
        mLeftButton = toolbarView.findViewById(R.id.toolbar_left_button);
        mRightButton = toolbarView.findViewById(R.id.toolbar_right_button);
        mContainerView = toolbarView.findViewById(R.id.toolbar_container_view);
        mMainContent = toolbarView.findViewById(R.id.main_content);
        RecyclerView recyclerView =
                (TabListRecyclerView) LayoutInflater.from(getActivity())
                        .inflate(R.layout.tab_list_recycler_view_layout, parentView, false);
        recyclerView.setLayoutManager(
                new LinearLayoutManager(getActivity(), LinearLayoutManager.HORIZONTAL, false));

        mModel = new PropertyModel(TabGroupUiProperties.ALL_KEYS);
        mMCP = PropertyModelChangeProcessor.create(mModel,
                new TabGroupUiViewBinder.ViewHolder(toolbarView, recyclerView),
                TabGroupUiViewBinder::bind);
    }

    @Override
    public void tearDownTest() throws Exception {
        mMCP.destroy();
        super.tearDownTest();
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetLeftButtonOnClickListener() {
        AtomicBoolean leftButtonClicked = new AtomicBoolean();
        leftButtonClicked.set(false);
        mLeftButton.performClick();
        assertFalse(leftButtonClicked.get());

        mModel.set(TabGroupUiProperties.LEFT_BUTTON_ON_CLICK_LISTENER,
                (View view) -> leftButtonClicked.set(true));

        mLeftButton.performClick();
        assertTrue(leftButtonClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetRightButtonOnClickListener() {
        AtomicBoolean rightButtonClicked = new AtomicBoolean();
        rightButtonClicked.set(false);
        mRightButton.performClick();
        assertFalse(rightButtonClicked.get());

        mModel.set(TabGroupUiProperties.RIGHT_BUTTON_ON_CLICK_LISTENER,
                (View view) -> rightButtonClicked.set(true));

        mRightButton.performClick();
        assertTrue(rightButtonClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetMainContentVisibility() {
        View contentView = new View(getActivity());
        mContainerView.addView(contentView);
        contentView.setVisibility(View.GONE);

        mModel.set(TabGroupUiProperties.IS_MAIN_CONTENT_VISIBLE, true);
        assertEquals(View.VISIBLE, contentView.getVisibility());

        mModel.set(TabGroupUiProperties.IS_MAIN_CONTENT_VISIBLE, false);
        assertEquals(View.INVISIBLE, contentView.getVisibility());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetLeftButtonDrawable() {
        int expandLessDrawableId = R.drawable.ic_expand_less_black_24dp;
        int expandMoreDrawableId = R.drawable.ic_expand_more_black_24dp;

        mModel.set(TabGroupUiProperties.LEFT_BUTTON_DRAWABLE_ID, expandLessDrawableId);
        Drawable expandLessDrawable = mLeftButton.getDrawable();
        mModel.set(TabGroupUiProperties.LEFT_BUTTON_DRAWABLE_ID, expandMoreDrawableId);
        Drawable expandMoreDrawable = mLeftButton.getDrawable();

        assertNotEquals(expandLessDrawable, expandMoreDrawable);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetTint() {
        ColorStateList tint = ThemeUtils.getThemedToolbarIconTint(getActivity(), true);
        Assert.assertNotEquals(tint, mLeftButton.getImageTintList());
        Assert.assertNotEquals(tint, mRightButton.getImageTintList());

        mModel.set(TabGroupUiProperties.TINT, tint);

        Assert.assertEquals(tint, mLeftButton.getImageTintList());
        Assert.assertEquals(tint, mRightButton.getImageTintList());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetPrimaryColor() {
        int colorGrey = R.color.modern_grey_300;
        int colorBlue = R.color.modern_blue_300;

        mModel.set(TabGroupUiProperties.PRIMARY_COLOR, colorGrey);
        int greyDrawableId = ((ColorDrawable) mMainContent.getBackground()).getColor();
        mModel.set(TabGroupUiProperties.PRIMARY_COLOR, colorBlue);
        int blueDrawableId = ((ColorDrawable) mMainContent.getBackground()).getColor();

        assertNotEquals(greyDrawableId, blueDrawableId);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetLeftButtonContentDescription() {
        assertNull(mLeftButton.getContentDescription());

        String string = "left button content";
        mModel.set(TabGroupUiProperties.LEFT_BUTTON_CONTENT_DESCRIPTION, string);

        assertEquals(string, mLeftButton.getContentDescription());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetRightButtonContentDescription() {
        assertNull(mRightButton.getContentDescription());

        String string = "right button content";
        mModel.set(TabGroupUiProperties.RIGHT_BUTTON_CONTENT_DESCRIPTION, string);

        assertEquals(string, mRightButton.getContentDescription());
    }
}
