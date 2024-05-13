// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.greaterThanOrEqualTo;
import static org.hamcrest.Matchers.lessThanOrEqualTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.areAnimatorsEnabled;

import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_management.TabGridDialogView.VisibilityListener;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** BlankUiTestActivity Tests for the {@link TabGridDialogView}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class TabGridDialogViewTest extends BlankUiTestActivityTestCase {
    private int mToolbarHeight;
    private int mMinMargin;
    private int mMaxMargin;
    private FrameLayout mTestParent;
    private View mSourceView;
    private View mUngroupBar;
    private View mDataSharingBar;
    private View mAnimationCardView;
    private View mBackgroundFrameView;
    private TextView mUngroupBarTextView;
    private RelativeLayout mTabGridDialogContainer;
    private FrameLayout.LayoutParams mContainerParams;
    private TabGridDialogView mTabGridDialogView;

    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestParent = new FrameLayout(getActivity());
                    getActivity().setContentView(mTestParent);
                    LayoutInflater.from(getActivity())
                            .inflate(R.layout.tab_grid_dialog_layout, mTestParent, true);

                    mTabGridDialogView = mTestParent.findViewById(R.id.dialog_parent_view);
                    mTabGridDialogContainer =
                            mTabGridDialogView.findViewById(R.id.dialog_container_view);
                    mUngroupBar = mTabGridDialogContainer.findViewById(R.id.dialog_ungroup_bar);
                    mUngroupBarTextView = mUngroupBar.findViewById(R.id.dialog_ungroup_bar_text);
                    mDataSharingBar =
                            mTabGridDialogContainer.findViewById(
                                    R.id.dialog_data_sharing_group_bar);
                    mContainerParams =
                            (FrameLayout.LayoutParams) mTabGridDialogContainer.getLayoutParams();
                    mAnimationCardView =
                            mTabGridDialogView.findViewById(R.id.dialog_animation_card_view);
                    mBackgroundFrameView = mTabGridDialogView.findViewById(R.id.dialog_frame);
                    ScrimCoordinator scrimCoordinator =
                            new ScrimCoordinator(getActivity(), null, mTestParent, Color.RED);
                    mTabGridDialogView.setupScrimCoordinator(scrimCoordinator);
                    mTabGridDialogView.setScrimClickRunnable(() -> {});

                    mToolbarHeight =
                            (int)
                                    getActivity()
                                            .getResources()
                                            .getDimension(R.dimen.tab_group_toolbar_height);
                    mMinMargin =
                            getActivity()
                                    .getResources()
                                    .getDimensionPixelSize(R.dimen.tab_grid_dialog_min_margin);
                    mMaxMargin =
                            getActivity()
                                    .getResources()
                                    .getDimensionPixelSize(R.dimen.tab_grid_dialog_max_margin);
                });
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testUpdateDialogWithOrientation() {
        mockDialogStatus(false);

        mTabGridDialogView.updateDialogWithOrientation(Configuration.ORIENTATION_PORTRAIT);

        assertThat(
                mContainerParams.topMargin,
                allOf(greaterThanOrEqualTo(mMinMargin), lessThanOrEqualTo(mMaxMargin)));
        assertEquals(mContainerParams.leftMargin, mMinMargin);
        assertEquals(View.GONE, mTabGridDialogView.getVisibility());

        mockDialogStatus(false);

        mTabGridDialogView.updateDialogWithOrientation(Configuration.ORIENTATION_LANDSCAPE);

        assertThat(
                mContainerParams.leftMargin,
                allOf(greaterThanOrEqualTo(mMinMargin), lessThanOrEqualTo(mMaxMargin)));
        assertEquals(mContainerParams.topMargin, mMinMargin);
        assertEquals(View.GONE, mTabGridDialogView.getVisibility());

        mockDialogStatus(true);

        mTabGridDialogView.updateDialogWithOrientation(Configuration.ORIENTATION_PORTRAIT);

        assertThat(
                mContainerParams.topMargin,
                allOf(greaterThanOrEqualTo(mMinMargin), lessThanOrEqualTo(mMaxMargin)));
        assertEquals(mContainerParams.leftMargin, mMinMargin);
        assertEquals(View.VISIBLE, mTabGridDialogView.getVisibility());

        mockDialogStatus(true);

        mTabGridDialogView.updateDialogWithOrientation(Configuration.ORIENTATION_LANDSCAPE);

        assertThat(
                mContainerParams.leftMargin,
                allOf(greaterThanOrEqualTo(mMinMargin), lessThanOrEqualTo(mMaxMargin)));
        assertEquals(mContainerParams.topMargin, mMinMargin);
        assertEquals(View.VISIBLE, mTabGridDialogView.getVisibility());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures({ChromeFeatureList.DATA_SHARING_ANDROID})
    public void testResetDialog() {
        mTabGridDialogContainer.removeAllViews();
        View toolbarView = new View(getActivity());
        View recyclerView = new View(getActivity());
        recyclerView.setVisibility(View.GONE);

        mTabGridDialogView.resetDialog(toolbarView, recyclerView, null);

        // It should contain four child views: top tool bar, recyclerview, ungroup bar and undo bar
        // container.
        assertEquals(4, mTabGridDialogContainer.getChildCount());
        assertEquals(View.VISIBLE, recyclerView.getVisibility());
        RelativeLayout.LayoutParams params =
                (RelativeLayout.LayoutParams) recyclerView.getLayoutParams();
        assertEquals(mToolbarHeight, params.topMargin);
        assertEquals(0, params.leftMargin);
        assertEquals(0, params.rightMargin);
        assertEquals(0, params.bottomMargin);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures({ChromeFeatureList.DATA_SHARING_ANDROID})
    public void testResetDialogWithDataSharing() {
        mTabGridDialogContainer.removeAllViews();
        View toolbarView = new View(getActivity());
        View recyclerView = new View(getActivity());
        View shareBar =
                LayoutInflater.from(getActivity()).inflate(R.layout.data_sharing_group_bar, null);
        recyclerView.setVisibility(View.GONE);

        mTabGridDialogView.updateShouldShowShare(true);
        mTabGridDialogView.resetDialog(toolbarView, recyclerView, shareBar);

        // It should contain five child views: top tool bar, recyclerview, ungroup bar, data sharing
        // bar and undo bar container.
        assertEquals(5, mTabGridDialogContainer.getChildCount());
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.DATA_SHARING_ANDROID})
    public void testUpdateUngroupBar() {
        AtomicReference<ColorStateList> showTextColorReference = new AtomicReference<>();
        AtomicReference<ColorStateList> hoverTextColorReference = new AtomicReference<>();
        // Initialize the dialog with stand-in views.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogContainer.removeAllViews();
                    View toolbarView = new View(getActivity());
                    View recyclerView = new View(getActivity());
                    mTabGridDialogView.resetDialog(toolbarView, recyclerView, null);
                });

        // From hide to show.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ColorStateList colorStateList = mUngroupBarTextView.getTextColors();
                    mTabGridDialogView.updateUngroupBar(TabGridDialogView.UngroupBarStatus.SHOW);

                    assertNotNull(mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting());
                    // Verify that the ungroup bar textView is updated.
                    assertNotEquals(colorStateList, mUngroupBarTextView.getTextColors());
                    assertEquals(View.VISIBLE, mUngroupBar.getVisibility());
                    // Initialize text color when the ungroup bar is showing.
                    showTextColorReference.set(mUngroupBarTextView.getTextColors());
                });

        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting(),
                                Matchers.nullValue()));

        // From show to hide.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.updateUngroupBar(TabGridDialogView.UngroupBarStatus.HIDE);

                    assertNotNull(mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting());
                    // Verify that the ungroup bar textView is not updated.
                    assertEquals(showTextColorReference.get(), mUngroupBarTextView.getTextColors());
                    // Ungroup bar is still visible for the hiding animation.
                    assertEquals(View.VISIBLE, mUngroupBar.getVisibility());
                });

        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting(),
                                Matchers.nullValue()));
        // Ungroup bar is not visible after the hiding animation.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> assertEquals(View.INVISIBLE, mUngroupBar.getVisibility()));

        // From hide to hover.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ColorStateList colorStateList = mUngroupBarTextView.getTextColors();
                    mTabGridDialogView.updateUngroupBar(TabGridDialogView.UngroupBarStatus.HOVERED);

                    assertNotNull(mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting());
                    // Verify that the ungroup bar textView is updated.
                    assertNotEquals(colorStateList, mUngroupBarTextView.getTextColors());
                    assertEquals(View.VISIBLE, mUngroupBar.getVisibility());
                    // Initialize text color when the ungroup bar is being hovered on.
                    hoverTextColorReference.set(mUngroupBarTextView.getTextColors());
                });

        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting(),
                                Matchers.nullValue()));

        // From hover to hide.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.updateUngroupBar(TabGridDialogView.UngroupBarStatus.HIDE);

                    assertNotNull(mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting());
                    // Verify that the ungroup bar textView is not updated.
                    assertEquals(
                            hoverTextColorReference.get(), mUngroupBarTextView.getTextColors());
                    // Ungroup bar is still visible for the hiding animation.
                    assertEquals(View.VISIBLE, mUngroupBar.getVisibility());
                });

        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting(),
                                Matchers.nullValue()));
        // Ungroup bar is not visible after the hiding animation.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> assertEquals(View.INVISIBLE, mUngroupBar.getVisibility()));

        // From show to hover.
        // First, set the ungroup bar state to show.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTabGridDialogView.updateUngroupBar(TabGridDialogView.UngroupBarStatus.SHOW));
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting(),
                                Matchers.nullValue()));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(showTextColorReference.get(), mUngroupBarTextView.getTextColors());

                    mTabGridDialogView.updateUngroupBar(TabGridDialogView.UngroupBarStatus.HOVERED);

                    // There should be no animation going on.
                    assertNull(mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting());
                    // Verify that the ungroup bar textView is updated.
                    assertEquals(
                            hoverTextColorReference.get(), mUngroupBarTextView.getTextColors());
                    assertEquals(View.VISIBLE, mUngroupBar.getVisibility());
                });

        // From hover to show.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            hoverTextColorReference.get(), mUngroupBarTextView.getTextColors());

                    mTabGridDialogView.updateUngroupBar(TabGridDialogView.UngroupBarStatus.SHOW);

                    // There should be no animation going on.
                    assertNull(mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting());
                    // Verify that the ungroup bar textView is updated.
                    assertEquals(showTextColorReference.get(), mUngroupBarTextView.getTextColors());
                    assertEquals(View.VISIBLE, mUngroupBar.getVisibility());
                });
    }

    @Test
    @MediumTest
    public void testDialog_ZoomInZoomOut() {
        // TODO(crbug.com/40687819): figure out a stable way to separate different stages of the
        // animation so that we can verify the alpha and view hierarchy of the animation-related
        // views.
        AtomicReference<ViewGroup> parentViewReference = new AtomicReference<>();
        // Setup the animation with a stand-in animation source view.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSourceView = new View(getActivity());
                    mTestParent.addView(mSourceView, 0, new FrameLayout.LayoutParams(100, 100));
                });
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.setupDialogAnimation(mSourceView);
                    parentViewReference.set((ViewGroup) mTabGridDialogContainer.getParent());
                    assertFalse(mTabGridDialogContainer.isFocused());
                });
        ViewGroup parent = parentViewReference.get();

        // Show the dialog with zoom-out animation.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.showDialog();
                    assertNotNull(mTabGridDialogView.getCurrentDialogAnimatorForTesting());
                    assertEquals(View.VISIBLE, mTabGridDialogView.getVisibility());
                });
        // When the card fades out, the dialog should be brought to the top, and alpha of animation
        // related views should be set to 0.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            parent.getChildAt(parent.getChildCount() - 1),
                            Matchers.is(mTabGridDialogContainer));
                });
        TestThreadUtils.runOnUiThreadBlocking(
                () -> assertEquals(0f, mAnimationCardView.getAlpha(), 0.0));
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
                    assertTrue(mTabGridDialogContainer.isFocused());
                });

        // Hide the dialog with zoom-in animation.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.hideDialog();
                    assertNotNull(mTabGridDialogView.getCurrentDialogAnimatorForTesting());
                    // PopupWindow is still showing for the hide animation.
                    assertEquals(View.VISIBLE, mTabGridDialogView.getVisibility());
                });
        // When the dialog fades out, the animation card and the background frame should be brought
        // to the top.
        CriteriaHelper.pollUiThread(
                () ->
                        mAnimationCardView == parent.getChildAt(parent.getChildCount() - 1)
                                && mBackgroundFrameView
                                        == parent.getChildAt(parent.getChildCount() - 2));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> assertEquals(0f, mTabGridDialogContainer.getAlpha(), 0.0));
        // When the animation completes, the PopupWindow should be dismissed.
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(View.GONE, mTabGridDialogView.getVisibility());
                    assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
                    assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
                    assertEquals(0f, mTabGridDialogContainer.getTranslationX(), 0.0);
                    assertEquals(0f, mTabGridDialogContainer.getTranslationY(), 0.0);
                    assertEquals(1f, mTabGridDialogContainer.getScaleX(), 0.0);
                    assertEquals(1f, mTabGridDialogContainer.getScaleY(), 0.0);
                    assertFalse(mTabGridDialogContainer.isFocused());
                });
    }

    @Test
    @MediumTest
    public void testDialog_ZoomInFadeOut() {
        // Setup the animation with a stand-in animation source view.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSourceView = new View(getActivity());
                    mTestParent.addView(mSourceView, 0, new FrameLayout.LayoutParams(100, 100));
                });
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.setupDialogAnimation(mSourceView);
                    assertFalse(mTabGridDialogContainer.isFocused());
                });
        // Show the dialog.
        TestThreadUtils.runOnUiThreadBlocking(() -> mTabGridDialogView.showDialog());
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));
        // After the zoom in animation, alpha of animation related views should be 0.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
                    assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
                    assertTrue(mTabGridDialogContainer.isFocused());
                });

        // Hide the dialog with basic fade-out animation.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.setupDialogAnimation(null);
                    mTabGridDialogView.hideDialog();
                    // Alpha of animation related views should remain 0.
                    assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
                    assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
                    assertNotNull(mTabGridDialogView.getCurrentDialogAnimatorForTesting());
                    assertEquals(View.VISIBLE, mTabGridDialogView.getVisibility());
                });
        // When the animation completes, the PopupWindow should be dismissed. The alpha of animation
        // related views should remain 0.
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(View.GONE, mTabGridDialogView.getVisibility());
                    assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
                    assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
                    assertFalse(mTabGridDialogContainer.isFocused());
                });
    }

    @Test
    @MediumTest
    public void testDialog_FadeInFadeOut() {
        // Setup the the basic fade-in and fade-out animation.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.setupDialogAnimation(null);
                    // Initially alpha of animation related views should be 0.
                    assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
                    assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
                    assertFalse(mTabGridDialogContainer.isFocused());
                });

        // Show the dialog with basic fade-in animation.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.showDialog();
                    assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
                    assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
                    assertNotNull(mTabGridDialogView.getCurrentDialogAnimatorForTesting());
                    assertEquals(View.VISIBLE, mTabGridDialogView.getVisibility());
                });
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
                    assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
                    assertTrue(mTabGridDialogContainer.isFocused());
                });

        // Hide the dialog with basic fade-out animation.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.hideDialog();
                    if (areAnimatorsEnabled()) {
                        // At the very beginning of hiding animation, alpha of background frame and
                        // animation card should both be set to 0f.
                        assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
                        assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
                    }
                    assertNotNull(mTabGridDialogView.getCurrentDialogAnimatorForTesting());
                    assertEquals(View.VISIBLE, mTabGridDialogView.getVisibility());
                });
        // When the animation completes, the PopupWindow should be dismissed. The alpha of animation
        // related views should remain 0.
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(View.GONE, mTabGridDialogView.getVisibility());
                    assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
                    assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
                    assertFalse(mTabGridDialogContainer.isFocused());
                });
    }

    @Test
    @MediumTest
    public void testHideDialog_InvokeVisibilityListener() throws TimeoutException {
        CallbackHelper visibilityCallback = new CallbackHelper();
        mTabGridDialogView.setVisibilityListener(
                new VisibilityListener() {
                    @Override
                    public void finishedHidingDialogView() {
                        visibilityCallback.notifyCalled();
                    }
                });
        // Setup the the basic animation.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.setupDialogAnimation(null);
                });

        // Show the dialog.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.showDialog();
                });
        // Wait for show to finish.
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));

        // Hide the dialog.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.hideDialog();
                });
        visibilityCallback.waitForNext();
    }

    private void mockDialogStatus(boolean isShowing) {
        mContainerParams.setMargins(0, 0, 0, 0);
        if (isShowing) {
            mTabGridDialogView.setVisibility(View.VISIBLE);
        } else {
            mTabGridDialogView.setVisibility(View.GONE);
        }
    }
}
