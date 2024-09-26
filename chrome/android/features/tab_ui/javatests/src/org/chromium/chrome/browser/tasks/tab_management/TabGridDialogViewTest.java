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
import android.os.SystemClock;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_management.TabGridDialogView.VisibilityListener;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** BlankUiTestActivity Tests for the {@link TabGridDialogView}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DisableFeatures({ChromeFeatureList.DATA_SHARING})
@Batch(Batch.UNIT_TESTS)
public class TabGridDialogViewTest extends BlankUiTestActivityTestCase {
    private int mMinMargin;
    private int mMaxMargin;
    private FrameLayout mTestParent;
    private View mSourceView;
    private View mUngroupBar;
    private View mAnimationCardView;
    private View mBackgroundFrameView;
    private TextView mUngroupBarTextView;
    private RelativeLayout mTabGridDialogContainer;
    private FrameLayout.LayoutParams mContainerParams;
    private TabGridDialogView mTabGridDialogView;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        ThreadUtils.runOnUiThreadBlocking(
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
                    mContainerParams =
                            (FrameLayout.LayoutParams) mTabGridDialogContainer.getLayoutParams();
                    mAnimationCardView =
                            mTabGridDialogView.findViewById(R.id.dialog_animation_card_view);
                    mBackgroundFrameView = mTabGridDialogView.findViewById(R.id.dialog_frame);
                    ScrimCoordinator scrimCoordinator =
                            new ScrimCoordinator(getActivity(), null, mTestParent, Color.RED);
                    mTabGridDialogView.setupScrimCoordinator(scrimCoordinator);
                    mTabGridDialogView.setScrimClickRunnable(() -> {});

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
        int appHeaderHeight = 10;
        mTabGridDialogView.setAppHeaderHeight(appHeaderHeight);

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
        assertEquals(mContainerParams.topMargin, mMinMargin + appHeaderHeight);
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
        assertEquals(mContainerParams.topMargin, mMinMargin + appHeaderHeight);
        assertEquals(View.VISIBLE, mTabGridDialogView.getVisibility());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testResetDialog() {
        View toolbarView = new View(getActivity());
        View recyclerView = new View(getActivity());
        recyclerView.setVisibility(View.GONE);

        mTabGridDialogView.resetDialog(toolbarView, recyclerView);

        assertEquals(
                getActivity().findViewById(R.id.tab_grid_dialog_toolbar_container),
                toolbarView.getParent());
        assertEquals(
                getActivity().findViewById(R.id.tab_grid_dialog_recycler_view_container),
                recyclerView.getParent());
        assertEquals(View.VISIBLE, recyclerView.getVisibility());
    }

    @Test
    @MediumTest
    public void testUpdateUngroupBar() {
        AtomicReference<ColorStateList> showTextColorReference = new AtomicReference<>();
        AtomicReference<ColorStateList> hoverTextColorReference = new AtomicReference<>();
        // Initialize the dialog with stand-in views.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View toolbarView = new View(getActivity());
                    View recyclerView = new View(getActivity());
                    mTabGridDialogView.resetDialog(toolbarView, recyclerView);
                });

        // From hide to show.
        ThreadUtils.runOnUiThreadBlocking(
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
        ThreadUtils.runOnUiThreadBlocking(
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> assertEquals(View.INVISIBLE, mUngroupBar.getVisibility()));

        // From hide to hover.
        ThreadUtils.runOnUiThreadBlocking(
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
        ThreadUtils.runOnUiThreadBlocking(
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> assertEquals(View.INVISIBLE, mUngroupBar.getVisibility()));

        // From show to hover.
        // First, set the ungroup bar state to show.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTabGridDialogView.updateUngroupBar(TabGridDialogView.UngroupBarStatus.SHOW));
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mTabGridDialogView.getCurrentUngroupBarAnimatorForTesting(),
                                Matchers.nullValue()));

        ThreadUtils.runOnUiThreadBlocking(
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
        ThreadUtils.runOnUiThreadBlocking(
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSourceView = new View(getActivity());
                    mTestParent.addView(mSourceView, 0, new FrameLayout.LayoutParams(100, 100));
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.setupDialogAnimation(mSourceView);
                    parentViewReference.set((ViewGroup) mTabGridDialogContainer.getParent());
                    assertFalse(mTabGridDialogContainer.isFocused());
                });
        ViewGroup parent = parentViewReference.get();

        // Show the dialog with zoom-out animation.
        ThreadUtils.runOnUiThreadBlocking(
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> assertEquals(0f, mAnimationCardView.getAlpha(), 0.0));
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
                    assertTrue(mTabGridDialogContainer.isFocused());
                });

        // Hide the dialog with zoom-in animation.
        ThreadUtils.runOnUiThreadBlocking(
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
                        mAnimationCardView
                                        == ((ViewGroup)
                                                        parent.getChildAt(
                                                                parent.getChildCount() - 1))
                                                .getChildAt(0)
                                && mBackgroundFrameView
                                        == parent.getChildAt(parent.getChildCount() - 2));
        ThreadUtils.runOnUiThreadBlocking(
                () -> assertEquals(0f, mTabGridDialogContainer.getAlpha(), 0.0));
        // When the animation completes, the PopupWindow should be dismissed.
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));
        ThreadUtils.runOnUiThreadBlocking(
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSourceView = new View(getActivity());
                    mTestParent.addView(mSourceView, 0, new FrameLayout.LayoutParams(100, 100));
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.setupDialogAnimation(mSourceView);
                    assertFalse(mTabGridDialogContainer.isFocused());
                });
        // Show the dialog.
        ThreadUtils.runOnUiThreadBlocking(() -> mTabGridDialogView.showDialog());
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mTabGridDialogView.getCurrentDialogAnimatorForTesting(),
                                Matchers.nullValue()));
        // After the zoom in animation, alpha of animation related views should be 0.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
                    assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
                    assertTrue(mTabGridDialogContainer.isFocused());
                });

        // Hide the dialog with basic fade-out animation.
        ThreadUtils.runOnUiThreadBlocking(
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
        ThreadUtils.runOnUiThreadBlocking(
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.setupDialogAnimation(null);
                    // Initially alpha of animation related views should be 0.
                    assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
                    assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
                    assertFalse(mTabGridDialogContainer.isFocused());
                });

        // Show the dialog with basic fade-in animation.
        ThreadUtils.runOnUiThreadBlocking(
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(0f, mAnimationCardView.getAlpha(), 0.0);
                    assertEquals(0f, mBackgroundFrameView.getAlpha(), 0.0);
                    assertTrue(mTabGridDialogContainer.isFocused());
                });

        // Hide the dialog with basic fade-out animation.
        ThreadUtils.runOnUiThreadBlocking(
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
        ThreadUtils.runOnUiThreadBlocking(
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.setupDialogAnimation(null);
                });

        // Show the dialog.
        ThreadUtils.runOnUiThreadBlocking(
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.hideDialog();
                });
        visibilityCallback.waitForNext();
    }

    @Test
    @SmallTest
    public void testDispatchTouchEvent() {
        boolean[] isFocused = new boolean[] {false};
        boolean[] isFocusCleared = new boolean[] {false};
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    EditText textView =
                            new EditText(getActivity()) {
                                @Override
                                public boolean isFocused() {
                                    return isFocused[0];
                                }

                                @Override
                                public void clearFocus() {
                                    isFocusCleared[0] = true;
                                }
                            };
                    textView.setId(R.id.title);
                    mTabGridDialogView.addView(textView);
                });

        long time = SystemClock.uptimeMillis();
        MotionEvent event = MotionEvent.obtain(time, time, MotionEvent.ACTION_DOWN, 0.f, 0.f, 0);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.dispatchTouchEvent(event);
                });
        assertFalse(isFocusCleared[0]);

        isFocused[0] = true;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGridDialogView.dispatchTouchEvent(event);
                });
        assertTrue(isFocusCleared[0]);
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
