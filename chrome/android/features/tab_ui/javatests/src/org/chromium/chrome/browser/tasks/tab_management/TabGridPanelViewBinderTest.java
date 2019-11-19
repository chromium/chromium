// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.areAnimatorsEnabled;

import android.content.res.ColorStateList;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.os.SystemClock;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.support.v4.content.ContextCompat;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.PopupWindow;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.browser.toolbar.ToolbarColors;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ChromeImageView;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for {@link TabGridPanelViewBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabGridPanelViewBinderTest extends DummyUiActivityTestCase {
    private static final int CONTENT_TOP_MARGIN = 56;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mMCP;
    private TabGroupUiToolbarView mToolbarView;
    private RecyclerView mContentView;
    private TabGridDialogParent mTabGridDialogParent;
    private ChromeImageView mRightButton;
    private ChromeImageView mLeftButton;
    private EditText mTitleTextView;
    private View mMainContent;
    private ViewGroup mTabGridDialogParentView;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        FrameLayout parentView = new FrameLayout(getActivity());
        mContentView = (TabListRecyclerView) LayoutInflater.from(getActivity())
                               .inflate(R.layout.tab_list_recycler_view_layout, parentView, false);
        mContentView.setLayoutManager(new GridLayoutManager(getActivity(), 2));
        mToolbarView = (TabGroupUiToolbarView) LayoutInflater.from(getActivity())
                               .inflate(R.layout.bottom_tab_grid_toolbar, mContentView, false);
        mTabGridDialogParent =
                new TabGridDialogParent(getActivity(), new FrameLayout(getActivity()));
        mTabGridDialogParentView = mTabGridDialogParent.getTabGridDialogParentViewForTesting();
        mLeftButton = mToolbarView.findViewById(R.id.toolbar_left_button);
        mRightButton = mToolbarView.findViewById(R.id.toolbar_right_button);
        mTitleTextView = mToolbarView.findViewById(R.id.title);
        mMainContent = mToolbarView.findViewById(R.id.main_content);

        mModel = new PropertyModel(TabGridPanelProperties.ALL_KEYS);

        mMCP = PropertyModelChangeProcessor.create(mModel,
                new TabGridPanelViewBinder.ViewHolder(
                        mToolbarView, mContentView, mTabGridDialogParent),
                TabGridPanelViewBinder::bind);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetCollapseClickListener() {
        AtomicBoolean leftButtonClicked = new AtomicBoolean();
        leftButtonClicked.set(false);
        mLeftButton.performClick();
        Assert.assertFalse(leftButtonClicked.get());

        mModel.set(TabGridPanelProperties.COLLAPSE_CLICK_LISTENER,
                (View view) -> leftButtonClicked.set(true));

        mLeftButton.performClick();
        Assert.assertTrue(leftButtonClicked.get());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetAddClickListener() {
        AtomicBoolean rightButtonClicked = new AtomicBoolean();
        rightButtonClicked.set(false);
        mRightButton.performClick();
        Assert.assertFalse(rightButtonClicked.get());

        mModel.set(TabGridPanelProperties.ADD_CLICK_LISTENER,
                (View view) -> rightButtonClicked.set(true));

        mRightButton.performClick();
        Assert.assertTrue(rightButtonClicked.get());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetHeaderTitle() {
        String title = "1024 tabs";
        Assert.assertNotEquals(title, mTitleTextView.getText());

        mModel.set(TabGridPanelProperties.HEADER_TITLE, title);

        Assert.assertEquals(title, mTitleTextView.getText().toString());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testContentTopMargin() {
        // Since setting content top margin is only used in sheet, we can assume that the parent is
        // a FrameLayout here.
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(0, 0);
        params.setMargins(0, 0, 0, 0);
        mContentView.setLayoutParams(new FrameLayout.LayoutParams(0, 0));
        Assert.assertEquals(
                0, ((ViewGroup.MarginLayoutParams) mContentView.getLayoutParams()).topMargin);

        mModel.set(TabGridPanelProperties.CONTENT_TOP_MARGIN, CONTENT_TOP_MARGIN);

        Assert.assertEquals(CONTENT_TOP_MARGIN,
                ((ViewGroup.MarginLayoutParams) mContentView.getLayoutParams()).topMargin);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetPrimaryColor() {
        int color = ContextCompat.getColor(getActivity(), R.color.modern_blue_300);
        Assert.assertNull(mMainContent.getBackground());
        Assert.assertNull(mContentView.getBackground());

        mModel.set(TabGridPanelProperties.PRIMARY_COLOR, color);

        Assert.assertEquals(color, ((ColorDrawable) mMainContent.getBackground()).getColor());
        Assert.assertEquals(color, ((ColorDrawable) mContentView.getBackground()).getColor());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetTint() {
        ColorStateList tint = ToolbarColors.getThemedToolbarIconTint(getActivity(), true);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Assert.assertNotEquals(tint, mLeftButton.getImageTintList());
            Assert.assertNotEquals(tint, mRightButton.getImageTintList());
        }
        Assert.assertNotEquals(tint, mTitleTextView.getTextColors());

        mModel.set(TabGridPanelProperties.TINT, tint);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Assert.assertEquals(tint, mLeftButton.getImageTintList());
            Assert.assertEquals(tint, mRightButton.getImageTintList());
        }
        Assert.assertEquals(tint, mTitleTextView.getTextColors());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetScrimViewObserver() {
        AtomicBoolean scrimViewClicked = new AtomicBoolean();
        scrimViewClicked.set(false);
        ScrimView.ScrimObserver scrimObserver = new ScrimView.ScrimObserver() {
            @Override
            public void onScrimClick() {
                scrimViewClicked.set(true);
            }

            @Override
            public void onScrimVisibilityChanged(boolean visible) {}
        };

        mModel.set(TabGridPanelProperties.SCRIMVIEW_OBSERVER, scrimObserver);
        // Open the dialog to show the ScrimView.
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);
        // ScrimView is inserted at the lowest level of the parent view hierarchy.
        View scrimView = mTabGridDialogParentView.getChildAt(0);
        Assert.assertTrue(scrimView instanceof ScrimView);
        Assert.assertEquals(View.VISIBLE, scrimView.getVisibility());
        scrimView.performClick();
        Assert.assertTrue(scrimViewClicked.get());
    }

    @Test
    @SmallTest
    public void testSetDialogVisibility() {
        Assert.assertFalse(mTabGridDialogParent.getPopupWindowForTesting().isShowing());
        Assert.assertNull(mTabGridDialogParent.getCurrentDialogAnimatorForTesting());

        // Setup basic dialog animation. The dialog show/hide animation is always initialized before
        // the visibility of dialog is set.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTabGridDialogParent.setupDialogAnimation(null));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true));

        if (areAnimatorsEnabled()) {
            Assert.assertNotNull(mTabGridDialogParent.getCurrentDialogAnimatorForTesting());
        }
        Assert.assertTrue(mTabGridDialogParent.getPopupWindowForTesting().isShowing());
        CriteriaHelper.pollUiThread(
                () -> mTabGridDialogParent.getCurrentDialogAnimatorForTesting() == null);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false));

        if (areAnimatorsEnabled()) {
            Assert.assertNotNull(mTabGridDialogParent.getCurrentDialogAnimatorForTesting());
            Assert.assertTrue(mTabGridDialogParent.getPopupWindowForTesting().isShowing());
        }
        CriteriaHelper.pollUiThread(
                () -> mTabGridDialogParent.getCurrentDialogAnimatorForTesting() == null);
        Assert.assertFalse(mTabGridDialogParent.getPopupWindowForTesting().isShowing());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetAnimationSourceView() {
        // Initially, the show animation set is empty.
        Assert.assertEquals(0,
                mTabGridDialogParent.getShowDialogAnimationForTesting()
                        .getChildAnimations()
                        .size());

        // When set animation source view as null, the show animation is set to be basic fade-in
        // which contains only one animation in animation set.
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, null);
        Assert.assertEquals(1,
                mTabGridDialogParent.getShowDialogAnimationForTesting()
                        .getChildAnimations()
                        .size());

        // Create a dummy source view to setup the dialog animation.
        View sourceView = new View(getActivity());

        // When set with a specific animation source view, the show animation contains 6 child
        // animations.
        mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, sourceView);
        Assert.assertEquals(6,
                mTabGridDialogParent.getShowDialogAnimationForTesting()
                        .getChildAnimations()
                        .size());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetUngroupbarStatus() {
        // Default status for ungroup bar is hidden.
        Assert.assertEquals(TabGridDialogParent.UngroupBarStatus.HIDE,
                mTabGridDialogParent.getUngroupBarStatusForTesting());

        mModel.set(TabGridPanelProperties.UNGROUP_BAR_STATUS,
                TabGridDialogParent.UngroupBarStatus.SHOW);
        Assert.assertEquals(TabGridDialogParent.UngroupBarStatus.SHOW,
                mTabGridDialogParent.getUngroupBarStatusForTesting());

        mModel.set(TabGridPanelProperties.UNGROUP_BAR_STATUS,
                TabGridDialogParent.UngroupBarStatus.HOVERED);
        Assert.assertEquals(TabGridDialogParent.UngroupBarStatus.HOVERED,
                mTabGridDialogParent.getUngroupBarStatusForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetDialogBackgroundResource() {
        int normalResourceId = R.drawable.tab_grid_dialog_background;
        int incognitoResourceId = R.drawable.tab_grid_dialog_background_incognito;
        // Default setup is in normal mode.
        Assert.assertEquals(
                normalResourceId, mTabGridDialogParent.getBackgroundDrawableResourceIdForTesting());

        mModel.set(TabGridPanelProperties.DIALOG_BACKGROUND_RESOUCE_ID, incognitoResourceId);

        Assert.assertEquals(incognitoResourceId,
                mTabGridDialogParent.getBackgroundDrawableResourceIdForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetUngroupbarBackgroundColor() {
        int normalColorId = R.color.tab_grid_dialog_background_color;
        int incognitoColorId = R.color.tab_grid_dialog_background_color_incognito;
        // Default setup is in normal mode.
        Assert.assertEquals(normalColorId,
                mTabGridDialogParent.getUngroupBarBackgroundColorResourceIdForTesting());

        mModel.set(TabGridPanelProperties.DIALOG_UNGROUP_BAR_BACKGROUND_COLOR_ID, incognitoColorId);

        Assert.assertEquals(incognitoColorId,
                mTabGridDialogParent.getUngroupBarBackgroundColorResourceIdForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetUngroupbarHoveredBackgroundColor() {
        int normalColorId = R.color.tab_grid_card_selected_color;
        int incognitoColorId = R.color.tab_grid_card_selected_color_incognito;
        // Default setup is in normal mode.
        Assert.assertEquals(normalColorId,
                mTabGridDialogParent.getUngroupBarHoveredBackgroundColorResourceIdForTesting());

        mModel.set(TabGridPanelProperties.DIALOG_UNGROUP_BAR_HOVERED_BACKGROUND_COLOR_ID,
                incognitoColorId);

        Assert.assertEquals(incognitoColorId,
                mTabGridDialogParent.getUngroupBarHoveredBackgroundColorResourceIdForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetUngroupbarTextAppearance() {
        int normalStyleId = R.style.TextAppearance_BlueTitle2;
        int incognitoStyleId = R.style.TextAppearance_BlueTitle2Incognito;
        // Default setup is in normal mode.
        Assert.assertEquals(
                normalStyleId, mTabGridDialogParent.getUngroupBarTextAppearanceForTesting());

        mModel.set(TabGridPanelProperties.DIALOG_UNGROUP_BAR_TEXT_APPEARANCE, incognitoStyleId);

        Assert.assertEquals(
                incognitoStyleId, mTabGridDialogParent.getUngroupBarTextAppearanceForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetMainContentVisibility() {
        mContentView.setVisibility(View.INVISIBLE);
        Assert.assertEquals(View.INVISIBLE, mContentView.getVisibility());

        mModel.set(TabGridPanelProperties.IS_MAIN_CONTENT_VISIBLE, true);

        Assert.assertEquals(View.VISIBLE, mContentView.getVisibility());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetTitleTextWatcher() {
        String title = "cool tabs";
        AtomicBoolean titleTextUpdated = new AtomicBoolean();
        titleTextUpdated.set(false);

        TextWatcher textWatcher = new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {}

            @Override
            public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {}

            @Override
            public void afterTextChanged(Editable editable) {
                titleTextUpdated.set(true);
            }
        };
        mModel.set(TabGridPanelProperties.TITLE_TEXT_WATCHER, textWatcher);

        mTitleTextView.setText(title);
        Assert.assertTrue(titleTextUpdated.get());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetTitleTextOnFocusListener() {
        AtomicBoolean textFocusChanged = new AtomicBoolean();
        textFocusChanged.set(false);
        Assert.assertFalse(mTitleTextView.isFocused());

        View.OnFocusChangeListener listener = (view, b) -> textFocusChanged.set(true);
        mModel.set(TabGridPanelProperties.TITLE_TEXT_ON_FOCUS_LISTENER, listener);
        mTitleTextView.requestFocus();

        Assert.assertTrue(mTitleTextView.isFocused());
        Assert.assertTrue(textFocusChanged.get());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetCursorVisibility() {
        mTitleTextView.setCursorVisible(false);

        mModel.set(TabGridPanelProperties.TITLE_CURSOR_VISIBILITY, true);

        Assert.assertTrue(mTitleTextView.isCursorVisible());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetIsTitleTextFocused() {
        Assert.assertFalse(mTitleTextView.isFocused());

        mModel.set(TabGridPanelProperties.IS_TITLE_TEXT_FOCUSED, true);

        // Binder should ignore set focus signal to avoid duplicate setting.
        Assert.assertFalse(mTitleTextView.isFocused());

        mTitleTextView.requestFocus();
        Assert.assertTrue(mTitleTextView.isFocused());

        mModel.set(TabGridPanelProperties.IS_TITLE_TEXT_FOCUSED, false);

        Assert.assertFalse(mTitleTextView.isFocused());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetPopupWindowFocusable() {
        PopupWindow popupWindow = mTabGridDialogParent.getPopupWindowForTesting();
        Assert.assertFalse(popupWindow.isFocusable());

        mModel.set(TabGridPanelProperties.IS_POPUP_WINDOW_FOCUSABLE, true);
        Assert.assertTrue(popupWindow.isFocusable());

        mModel.set(TabGridPanelProperties.IS_POPUP_WINDOW_FOCUSABLE, false);
        Assert.assertFalse(popupWindow.isFocusable());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetTitleTextOnTouchListener() {
        AtomicBoolean titleTextTouched = new AtomicBoolean();
        titleTextTouched.set(false);

        View.OnTouchListener listener = (view, event) -> {
            titleTextTouched.set(true);
            return false;
        };
        mModel.set(TabGridPanelProperties.TITLE_TEXT_ON_TOUCH_LISTENER, listener);
        // Create a dummy MotionEvent.
        MotionEvent e = MotionEvent.obtain(SystemClock.uptimeMillis(), SystemClock.uptimeMillis(),
                MotionEvent.ACTION_DOWN, 0, 0, 0);
        mTitleTextView.dispatchTouchEvent(e);

        Assert.assertTrue(titleTextTouched.get());
    }

    @Override
    public void tearDownTest() throws Exception {
        mMCP.destroy();
        mTabGridDialogParent.destroy();
        super.tearDownTest();
    }
}
