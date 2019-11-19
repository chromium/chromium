// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.View.SCROLLBARS_INSIDE_OVERLAY;
import static android.view.View.SCROLLBARS_OUTSIDE_OVERLAY;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.areAnimatorsEnabled;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.isKitKatAndBelow;

import android.content.res.ColorStateList;
import android.graphics.drawable.Animatable;
import android.graphics.drawable.Drawable;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.PopupWindow;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ChromeImageView;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for {@link TabGridIphItemViewBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabGridIphItemViewBinderTest extends DummyUiActivityTestCase {
    private TextView mShowIPHDialogButton;
    private TextView mCloseIPHDialogButton;
    private TextView mIphIntroduction;
    private ChromeImageView mCloseIPHEntranceButton;
    private TabListRecyclerView mContentView;
    private TabGridIphItemView mIphView;
    private PopupWindow mIphWindow;
    private ViewGroup mDialogParentView;
    private Animatable mIphAnimation;

    private PropertyModel mModel;
    private PropertyModelChangeProcessor mMCP;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        ViewGroup parentView = new FrameLayout(getActivity());
        getActivity().getLayoutInflater().inflate(R.layout.iph_card_item_layout, parentView, true);
        mIphView = (TabGridIphItemView) parentView.findViewById(R.id.tab_grid_iph_item);
        mContentView = (TabListRecyclerView) LayoutInflater.from(getActivity())
                               .inflate(R.layout.tab_list_recycler_view_layout, parentView, false);
        mCloseIPHEntranceButton = mIphView.findViewById(R.id.close_iph_button);
        mShowIPHDialogButton = mIphView.findViewById(R.id.show_me_button);
        mIphIntroduction = mIphView.findViewById(R.id.iph_description);
        mIphWindow = mIphView.getIphWindowForTesting();
        mDialogParentView = (ViewGroup) mIphWindow.getContentView();
        mCloseIPHDialogButton = mDialogParentView.findViewById(R.id.close_button);
        mIphAnimation =
                (Animatable) ((ImageView) mDialogParentView.findViewById(R.id.animation_drawable))
                        .getDrawable();

        mModel = new PropertyModel(TabGridIphItemProperties.ALL_KEYS);
        mMCP = PropertyModelChangeProcessor.create(mModel,
                new TabGridIphItemViewBinder.ViewHolder(mContentView, mIphView),
                TabGridIphItemViewBinder::bind);
    }

    @Override
    public void tearDownTest() throws Exception {
        mMCP.destroy();
        super.tearDownTest();
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetIphEntranceCloseButtonListener() {
        AtomicBoolean iphEntranceCloseButtonClicked = new AtomicBoolean();
        iphEntranceCloseButtonClicked.set(false);
        mCloseIPHEntranceButton.performClick();
        Assert.assertFalse(iphEntranceCloseButtonClicked.get());

        mModel.set(TabGridIphItemProperties.IPH_ENTRANCE_CLOSE_BUTTON_LISTENER,
                (View view) -> iphEntranceCloseButtonClicked.set(true));

        mCloseIPHEntranceButton.performClick();
        Assert.assertTrue(iphEntranceCloseButtonClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetIphEntranceShowButtonListener() {
        AtomicBoolean iphEntranceShowButtonClicked = new AtomicBoolean();
        iphEntranceShowButtonClicked.set(false);
        mShowIPHDialogButton.performClick();
        Assert.assertFalse(iphEntranceShowButtonClicked.get());

        mModel.set(TabGridIphItemProperties.IPH_ENTRANCE_SHOW_BUTTON_LISTENER,
                (View view) -> iphEntranceShowButtonClicked.set(true));

        mShowIPHDialogButton.performClick();
        Assert.assertTrue(iphEntranceShowButtonClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetIphDialogCloseButtonListener() {
        AtomicBoolean iphDialogCloseButtonClicked = new AtomicBoolean();
        iphDialogCloseButtonClicked.set(false);
        mCloseIPHDialogButton.performClick();
        Assert.assertFalse(iphDialogCloseButtonClicked.get());

        mModel.set(TabGridIphItemProperties.IPH_DIALOG_CLOSE_BUTTON_LISTENER,
                (View view) -> iphDialogCloseButtonClicked.set(true));

        mCloseIPHDialogButton.performClick();
        Assert.assertTrue(iphDialogCloseButtonClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetIphDialogVisibility() {
        assertFalse(mIphWindow.isShowing());
        assertFalse(mIphAnimation.isRunning());

        mModel.set(TabGridIphItemProperties.IS_IPH_DIALOG_VISIBLE, true);

        assertTrue(mIphWindow.isShowing());
        if (areAnimatorsEnabled()) {
            assertTrue(mIphAnimation.isRunning());
        }

        mModel.set(TabGridIphItemProperties.IS_IPH_DIALOG_VISIBLE, false);

        assertFalse(mIphWindow.isShowing());
        if (areAnimatorsEnabled()) {
            assertFalse(mIphAnimation.isRunning());
        }
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetIphEntranceVisibility() {
        assertNull(mContentView.getRecyclerViewFooterForTesting());
        assertEquals(SCROLLBARS_INSIDE_OVERLAY, mContentView.getScrollBarStyle());

        mModel.set(TabGridIphItemProperties.IS_IPH_ENTRANCE_VISIBLE, true);

        View recyclerViewFooter = mContentView.getRecyclerViewFooterForTesting();
        assertNotNull(recyclerViewFooter);
        assertEquals(View.INVISIBLE, recyclerViewFooter.getVisibility());
        assertEquals(SCROLLBARS_OUTSIDE_OVERLAY, mContentView.getScrollBarStyle());

        mModel.set(TabGridIphItemProperties.IS_IPH_ENTRANCE_VISIBLE, false);

        assertNull(mContentView.getRecyclerViewFooterForTesting());
        assertEquals(SCROLLBARS_INSIDE_OVERLAY, mContentView.getScrollBarStyle());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetScrimViewObserver() {
        AtomicBoolean iphDialogScrimViewClicked = new AtomicBoolean();
        iphDialogScrimViewClicked.set(false);
        ScrimView.ScrimObserver observer = new ScrimView.ScrimObserver() {
            @Override
            public void onScrimClick() {
                iphDialogScrimViewClicked.set(true);
            }

            @Override
            public void onScrimVisibilityChanged(boolean visible) {}
        };

        mModel.set(TabGridIphItemProperties.IPH_SCRIM_VIEW_OBSERVER, observer);
        mModel.set(TabGridIphItemProperties.IS_IPH_DIALOG_VISIBLE, true);

        assertTrue(mDialogParentView.getChildAt(0) instanceof ScrimView);
        ScrimView scrimView = (ScrimView) mDialogParentView.getChildAt(0);
        scrimView.performClick();
        assertTrue(iphDialogScrimViewClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetIsIncognito() {
        Drawable normalIphEntranceBackground = null;
        ColorStateList normalIphEntranceBackgroundTint = null;
        mModel.set(TabGridIphItemProperties.IS_INCOGNITO, false);
        if (isKitKatAndBelow()) {
            normalIphEntranceBackground = mIphView.getBackground();
        } else {
            normalIphEntranceBackgroundTint = mIphView.getBackgroundTintList();
        }
        ColorStateList normalShowIphDialogButtonColor = mShowIPHDialogButton.getTextColors();
        ColorStateList normalIphEntranceIntroductionColor = mIphIntroduction.getTextColors();
        ColorStateList normalCloseIphEntranceButtonTint = isKitKatAndBelow()
                ? mCloseIPHEntranceButton.getSupportImageTintList()
                : mCloseIPHEntranceButton.getImageTintList();

        Drawable incognitoIphEntranceBackground = null;
        ColorStateList incognitoIphEntranceBackgroundTint = null;
        mModel.set(TabGridIphItemProperties.IS_INCOGNITO, true);
        if (isKitKatAndBelow()) {
            incognitoIphEntranceBackground = mIphView.getBackground();
        } else {
            incognitoIphEntranceBackgroundTint = mIphView.getBackgroundTintList();
        }
        ColorStateList incognitoShowIphDialogButtonColor = mShowIPHDialogButton.getTextColors();
        ColorStateList incognitoIphEntranceIntroductionColor = mIphIntroduction.getTextColors();
        ColorStateList incognitoCloseIphEntranceButtonTint = isKitKatAndBelow()
                ? mCloseIPHEntranceButton.getSupportImageTintList()
                : mCloseIPHEntranceButton.getImageTintList();

        if (isKitKatAndBelow()) {
            assertNotNull(normalIphEntranceBackground);
            assertNotNull(incognitoIphEntranceBackground);
            assertNotEquals(normalIphEntranceBackground, incognitoIphEntranceBackground);
        } else {
            assertNotNull(normalIphEntranceBackgroundTint);
            assertNotNull(incognitoIphEntranceBackgroundTint);
            assertNotEquals(normalIphEntranceBackgroundTint, incognitoIphEntranceBackgroundTint);
        }
        assertNotEquals(normalShowIphDialogButtonColor, incognitoShowIphDialogButtonColor);
        assertNotEquals(normalIphEntranceIntroductionColor, incognitoIphEntranceIntroductionColor);
        assertNotEquals(normalCloseIphEntranceButtonTint, incognitoCloseIphEntranceButtonTint);
    }
}
