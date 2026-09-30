// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.USE_SHRINK_CLOSE_ANIMATION;

import android.util.Pair;
import android.view.View;
import android.view.ViewOutlineProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter.ViewHolder;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TabListItemAnimator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabListItemAnimatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final SettableNonNullObservableSupplier<Boolean> mIsAnimatorRunningSupplier =
            ObservableSuppliers.createNonNull(false);
    private final List<Boolean> mIsAnimatorRunningValues = new ArrayList<>();

    private TabListItemAnimator mItemAnimator;

    @Before
    public void setUp() {
        mIsAnimatorRunningSupplier.addSyncObserver(mIsAnimatorRunningValues::add);
        mItemAnimator =
                spy(
                        new TabListItemAnimator(
                                mIsAnimatorRunningSupplier, /* useClipAnimations= */ false));
    }

    private static void emptyBind(PropertyModel model, View view, PropertyKey key) {}

    private static void assertAlpha(ViewHolder holder, float alpha) {
        assertEquals(alpha, holder.itemView.getAlpha(), 0f);
    }

    private static void assertTranslation(ViewHolder holder, float x, float y) {
        assertEquals(x, holder.itemView.getTranslationX(), 0f);
        assertEquals(y, holder.itemView.getTranslationY(), 0f);
    }

    private static void assertScale(ViewHolder holder, float scale) {
        assertEquals(scale, holder.itemView.getScaleX(), 0f);
        assertEquals(scale, holder.itemView.getScaleY(), 0f);
    }

    private ViewHolder buildViewHolder(@ModelType int modelType, boolean useShrinkCloseAnimation) {
        View itemView = new View(ContextUtils.getApplicationContext());
        var viewHolder = new ViewHolder(itemView, TabListItemAnimatorUnitTest::emptyBind);
        PropertyModel model =
                new PropertyModel.Builder(new PropertyKey[] {CARD_TYPE, USE_SHRINK_CLOSE_ANIMATION})
                        .with(CARD_TYPE, modelType)
                        .with(USE_SHRINK_CLOSE_ANIMATION, useShrinkCloseAnimation)
                        .build();
        viewHolder.model = model;
        return viewHolder;
    }

    private void runAnimationToCompletion() {
        mItemAnimator.runPendingAnimations();
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
    }

    private void animateAddWithCompletionTrigger(Callback<ViewHolder> completionTrigger) {
        var holder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);

        assertTrue(mItemAnimator.animateAdd(holder));
        assertAlpha(holder, 0f);

        assertTrue(mItemAnimator.isRunning());

        InOrder inOrder = Mockito.inOrder(mItemAnimator);

        completionTrigger.onResult(holder);

        assertAlpha(holder, 1f);
        inOrder.verify(mItemAnimator).dispatchAddStarting(holder);
        inOrder.verify(mItemAnimator).dispatchAddFinished(holder);
        inOrder.verify(mItemAnimator).dispatchFinishedWhenAllAnimationsDone();

        assertFalse(mItemAnimator.isRunning());
    }

    @Test
    public void animateAdd_RunToCompletion() {
        animateAddWithCompletionTrigger(holder -> runAnimationToCompletion());
    }

    @Test
    public void animateAdd_EndAnimation() {
        animateAddWithCompletionTrigger(mItemAnimator::endAnimation);
    }

    @Test
    public void animateAdd_EndAnimations() {
        animateAddWithCompletionTrigger(holder -> mItemAnimator.endAnimations());
    }

    @Test
    public void animateChange_SameViewHolder_NoDelta() {
        var holder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);

        assertFalse(mItemAnimator.animateChange(holder, holder, 0, 0, 0, 0));
        verify(mItemAnimator).dispatchMoveFinished(holder);
    }

    @Test
    public void animateChange_SameViewHolder_WithDelta() {
        var holder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);

        assertTrue(mItemAnimator.animateChange(holder, holder, 0, 100, 50, 200));
        assertTranslation(holder, -50, -100);

        assertTrue(mItemAnimator.isRunning());

        InOrder inOrder = Mockito.inOrder(mItemAnimator);

        runAnimationToCompletion();

        inOrder.verify(mItemAnimator).dispatchMoveStarting(holder);
        inOrder.verify(mItemAnimator).dispatchMoveFinished(holder);
        inOrder.verify(mItemAnimator).dispatchFinishedWhenAllAnimationsDone();
        assertTranslation(holder, 0, 0);

        assertFalse(mItemAnimator.isRunning());
    }

    @Test
    public void animateChange_SingleHolder_RunToCompletion() {
        var holder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);

        float x = 40f;
        float y = 30f;
        float alpha = 0.3f;
        holder.itemView.setTranslationX(x);
        holder.itemView.setTranslationY(y);
        holder.itemView.setAlpha(alpha);
        assertTrue(mItemAnimator.animateChange(holder, null, 0, 100, 50, 200));
        assertTranslation(holder, x, y);
        assertAlpha(holder, alpha);

        assertTrue(mItemAnimator.isRunning());

        InOrder inOrder = Mockito.inOrder(mItemAnimator);

        runAnimationToCompletion();

        inOrder.verify(mItemAnimator).dispatchChangeStarting(holder, true);
        inOrder.verify(mItemAnimator).dispatchChangeFinished(holder, true);
        inOrder.verify(mItemAnimator).dispatchFinishedWhenAllAnimationsDone();
        // The old view is reset once its animation ends.
        assertAlpha(holder, 1f);
        assertTranslation(holder, 0, 0);

        assertFalse(mItemAnimator.isRunning());

        verify(mItemAnimator, never()).dispatchChangeStarting(any(), eq(false));
        verify(mItemAnimator, never()).dispatchChangeFinished(any(), eq(false));
    }

    private void animateChangeWithCompletionTrigger(
            Callback<Pair<ViewHolder, ViewHolder>> completionTrigger) {
        var oldHolder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);
        var newHolder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);

        float x = 40f;
        float y = 30f;
        float alpha = 0.3f;
        oldHolder.itemView.setTranslationX(x);
        oldHolder.itemView.setTranslationY(y);
        oldHolder.itemView.setAlpha(alpha);

        assertTrue(mItemAnimator.animateChange(oldHolder, newHolder, 0, 100, 50, 200));

        assertTranslation(oldHolder, x, y);
        assertAlpha(oldHolder, alpha);
        assertTranslation(newHolder, -10, -70);
        assertAlpha(newHolder, alpha);

        assertTrue(mItemAnimator.isRunning());

        completionTrigger.onResult(Pair.create(oldHolder, newHolder));

        // Order cannot be verified due to HashMap usage.
        verify(mItemAnimator).dispatchChangeStarting(oldHolder, true);
        verify(mItemAnimator).dispatchChangeFinished(oldHolder, true);
        verify(mItemAnimator).dispatchChangeStarting(newHolder, false);
        verify(mItemAnimator).dispatchChangeFinished(newHolder, false);

        // Both views are reset once their animations end.
        assertAlpha(oldHolder, 1f);
        assertTranslation(oldHolder, 0, 0);
        assertAlpha(newHolder, 1f);
        assertTranslation(newHolder, 0, 0);

        verify(mItemAnimator, times(2)).dispatchFinishedWhenAllAnimationsDone();

        assertFalse(mItemAnimator.isRunning());
    }

    @Test
    public void animateChange_TwoHolders_RunToCompletion() {
        animateChangeWithCompletionTrigger(holders -> runAnimationToCompletion());
    }

    @Test
    public void animateChange_TwoHolders_EndAnimation() {
        animateChangeWithCompletionTrigger(
                holders -> {
                    mItemAnimator.endAnimation(holders.first);
                    mItemAnimator.endAnimation(holders.second);
                });
    }

    @Test
    public void animateChange_TwoHolders_EndAnimations() {
        animateChangeWithCompletionTrigger(holders -> mItemAnimator.endAnimations());
    }

    @Test
    public void animateMove_NoDelta() {
        var holder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);

        assertFalse(mItemAnimator.animateMove(holder, 0, 0, 0, 0));
        verify(mItemAnimator).dispatchMoveFinished(holder);
    }

    private void animateMoveWithCompletionTrigger(Callback<ViewHolder> completionTrigger) {
        var holder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);

        assertTrue(mItemAnimator.animateMove(holder, 400, 200, 50, 100));
        assertTranslation(holder, 350, 100);

        assertTrue(mItemAnimator.isRunning());

        InOrder inOrder = Mockito.inOrder(mItemAnimator);

        completionTrigger.onResult(holder);

        inOrder.verify(mItemAnimator).dispatchMoveStarting(holder);
        inOrder.verify(mItemAnimator).dispatchMoveFinished(holder);
        inOrder.verify(mItemAnimator).dispatchFinishedWhenAllAnimationsDone();
        assertTranslation(holder, 0, 0);

        assertFalse(mItemAnimator.isRunning());
    }

    @Test
    public void animateMove_RunToCompletion() {
        animateMoveWithCompletionTrigger(holder -> runAnimationToCompletion());
    }

    @Test
    public void animateMove_EndAnimation() {
        animateMoveWithCompletionTrigger(mItemAnimator::endAnimation);
    }

    @Test
    public void animateMove_EndAnimations() {
        animateMoveWithCompletionTrigger(holder -> mItemAnimator.endAnimations());
    }

    @Test
    public void animateRemove_Alpha0() {
        var holder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);
        holder.itemView.setAlpha(0f);

        assertFalse(mItemAnimator.animateRemove(holder));
        verify(mItemAnimator).dispatchRemoveFinished(holder);
    }

    @Test
    public void animateRemove_NotVisible() {
        var holder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);
        holder.itemView.setVisibility(View.INVISIBLE);

        assertFalse(mItemAnimator.animateRemove(holder));
        verify(mItemAnimator).dispatchRemoveFinished(holder);
    }

    private void animateTabRemoveWithCompletionTrigger(Callback<ViewHolder> completionTrigger) {
        var holder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ true);
        // Start from a non-default state to verify that the view is reset afterwards.
        holder.itemView.setAlpha(0.5f);
        holder.itemView.setScaleX(0.5f);
        holder.itemView.setScaleY(0.5f);

        assertTrue(mItemAnimator.animateRemove(holder));

        assertTrue(mItemAnimator.isRunning());

        InOrder inOrder = Mockito.inOrder(mItemAnimator);

        completionTrigger.onResult(holder);

        inOrder.verify(mItemAnimator).dispatchRemoveStarting(holder);
        inOrder.verify(mItemAnimator).dispatchRemoveFinished(holder);
        inOrder.verify(mItemAnimator).dispatchFinishedWhenAllAnimationsDone();
        // The view is reset once its animation ends.
        assertAlpha(holder, 1f);
        assertScale(holder, 1f);

        assertFalse(mItemAnimator.isRunning());
    }

    @Test
    public void animateRemove_TabCard_RunToCompletion() {
        animateTabRemoveWithCompletionTrigger(holder -> runAnimationToCompletion());
    }

    @Test
    public void animateRemove_TabCard_EndAnimation() {
        animateTabRemoveWithCompletionTrigger(mItemAnimator::endAnimation);
    }

    @Test
    public void animateRemove_TabCard_EndAnimations() {
        animateTabRemoveWithCompletionTrigger(holder -> mItemAnimator.endAnimations());
    }

    private void animateNonTabRemoveWithCompletionTrigger(
            ViewHolder holder, Callback<ViewHolder> completionTrigger) {
        // Start from a non-default alpha to verify that the view is reset afterwards.
        holder.itemView.setAlpha(0.5f);
        assertTrue(mItemAnimator.animateRemove(holder));
        assertAlpha(holder, 0.5f);

        assertTrue(mItemAnimator.isRunning());

        InOrder inOrder = Mockito.inOrder(mItemAnimator);

        completionTrigger.onResult(holder);

        inOrder.verify(mItemAnimator).dispatchRemoveStarting(holder);
        inOrder.verify(mItemAnimator).dispatchRemoveFinished(holder);
        inOrder.verify(mItemAnimator).dispatchFinishedWhenAllAnimationsDone();
        // The view is reset once its animation ends.
        assertAlpha(holder, 1f);

        assertFalse(mItemAnimator.isRunning());
    }

    @Test
    public void animateRemove_NonTabCard_RunToCompletion() {
        var holder = buildViewHolder(MESSAGE, /* useShrinkCloseAnimation= */ false);
        animateNonTabRemoveWithCompletionTrigger(holder, _ -> runAnimationToCompletion());
    }

    @Test
    public void animateRemove_NonTabCard_EndAnimation() {
        var holder = buildViewHolder(MESSAGE, /* useShrinkCloseAnimation= */ false);
        animateNonTabRemoveWithCompletionTrigger(holder, mItemAnimator::endAnimation);
    }

    @Test
    public void animateRemove_NonTabCard_EndAnimations() {
        var holder = buildViewHolder(MESSAGE, /* useShrinkCloseAnimation= */ false);
        animateNonTabRemoveWithCompletionTrigger(holder, _ -> mItemAnimator.endAnimations());
    }

    @Test
    public void animateRemove_TabCardNoShrink() {
        var holder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);
        animateNonTabRemoveWithCompletionTrigger(holder, _ -> mItemAnimator.endAnimations());
    }

    @Test
    public void multipleAnimationSequencing() {
        var removedHolder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);
        var movedHolder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);
        var changedHolder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);
        var addedHolder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);

        mItemAnimator.animateRemove(removedHolder);
        mItemAnimator.animateMove(movedHolder, 1, 2, 3, 4);
        mItemAnimator.animateChange(changedHolder, null, 1, 2, 3, 4);
        mItemAnimator.animateAdd(addedHolder);

        assertTrue(mItemAnimator.isRunning());

        InOrder inOrder = Mockito.inOrder(mItemAnimator);

        runAnimationToCompletion();

        inOrder.verify(mItemAnimator).dispatchRemoveStarting(removedHolder);

        inOrder.verify(mItemAnimator).dispatchMoveStarting(movedHolder);
        inOrder.verify(mItemAnimator).dispatchChangeStarting(changedHolder, true);

        // The timing of this and the move/change starting is deterministic, but due to animator
        // sequencing this falls ever so slightly after the other events.
        inOrder.verify(mItemAnimator).dispatchRemoveFinished(removedHolder);

        inOrder.verify(mItemAnimator).dispatchMoveFinished(movedHolder);
        inOrder.verify(mItemAnimator).dispatchChangeFinished(changedHolder, true);

        inOrder.verify(mItemAnimator).dispatchAddStarting(addedHolder);
        inOrder.verify(mItemAnimator).dispatchAddFinished(addedHolder);

        verify(mItemAnimator, times(4)).dispatchFinishedWhenAllAnimationsDone();
        assertFalse(mItemAnimator.isRunning());
    }

    @Test
    public void animatorRunningSupplier_RunAnimations() {
        var removedHolder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ true);
        mItemAnimator.animateAdd(removedHolder);

        mItemAnimator.runPendingAnimations();
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(List.of(true, false), mIsAnimatorRunningValues);
    }

    @Test
    public void animatorRunningSupplier_EndAnimations() {
        var removedHolder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ true);
        mItemAnimator.animateAdd(removedHolder);

        mItemAnimator.endAnimations();
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(List.of(true, false), mIsAnimatorRunningValues);
    }

    @Test
    public void animateAdd_WithClipAnimations_RunToCompletion() {
        mItemAnimator =
                spy(
                        new TabListItemAnimator(
                                mIsAnimatorRunningSupplier, /* useClipAnimations= */ true));
        assertEquals(TabListItemAnimator.DEFAULT_REMOVE_DURATION, mItemAnimator.getMoveDuration());
        assertEquals(TabListItemAnimator.DEFAULT_REMOVE_DURATION, mItemAnimator.getAddDuration());
        assertEquals(
                TabListItemAnimator.DEFAULT_REMOVE_DURATION, mItemAnimator.getChangeDuration());

        var holder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);

        assertTrue(mItemAnimator.animateAdd(holder));
        assertAlpha(holder, 0f);
        assertTrue(holder.itemView.getClipToOutline());
        assertNotNull(holder.itemView.getOutlineProvider());
        assertNotEquals(ViewOutlineProvider.BACKGROUND, holder.itemView.getOutlineProvider());

        assertTrue(mItemAnimator.isRunning());

        runAnimationToCompletion();

        assertAlpha(holder, 1f);
        assertEquals(ViewOutlineProvider.BACKGROUND, holder.itemView.getOutlineProvider());
        assertFalse(holder.itemView.getClipToOutline());
        verify(mItemAnimator).dispatchAddFinished(holder);
        assertFalse(mItemAnimator.isRunning());
    }

    @Test
    public void animateRemove_WithClipAnimations_RunToCompletion() {
        mItemAnimator =
                spy(
                        new TabListItemAnimator(
                                mIsAnimatorRunningSupplier, /* useClipAnimations= */ true));

        var holder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);

        assertTrue(mItemAnimator.animateRemove(holder));
        assertTrue(holder.itemView.getClipToOutline());
        assertNotNull(holder.itemView.getOutlineProvider());
        assertNotEquals(ViewOutlineProvider.BACKGROUND, holder.itemView.getOutlineProvider());

        assertTrue(mItemAnimator.isRunning());

        runAnimationToCompletion();

        assertAlpha(holder, 1f);
        assertEquals(ViewOutlineProvider.BACKGROUND, holder.itemView.getOutlineProvider());
        assertFalse(holder.itemView.getClipToOutline());
        verify(mItemAnimator).dispatchRemoveFinished(holder);
        assertFalse(mItemAnimator.isRunning());
    }

    @Test
    public void animateRemove_WithClipAnimations_ClipFromTop_RunToCompletion() {
        mItemAnimator =
                spy(
                        new TabListItemAnimator(
                                mIsAnimatorRunningSupplier, /* useClipAnimations= */ true));

        var holder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);
        holder.itemView.setTag(R.id.tab_clip_from_top, true);

        assertTrue(mItemAnimator.animateRemove(holder));
        assertTrue(holder.itemView.getClipToOutline());
        assertNotNull(holder.itemView.getOutlineProvider());
        assertNotEquals(ViewOutlineProvider.BACKGROUND, holder.itemView.getOutlineProvider());

        assertTrue(mItemAnimator.isRunning());

        runAnimationToCompletion();

        assertAlpha(holder, 1f);
        assertEquals(ViewOutlineProvider.BACKGROUND, holder.itemView.getOutlineProvider());
        assertFalse(holder.itemView.getClipToOutline());
        verify(mItemAnimator).dispatchRemoveFinished(holder);
        assertFalse(mItemAnimator.isRunning());
    }

    @Test
    public void multipleAnimationSequencing_WithClipAnimations() {
        mItemAnimator =
                spy(
                        new TabListItemAnimator(
                                mIsAnimatorRunningSupplier, /* useClipAnimations= */ true));

        var removedHolder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);
        var movedHolder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);
        var changedHolder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);
        var addedHolder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);

        mItemAnimator.animateRemove(removedHolder);
        mItemAnimator.animateMove(movedHolder, 1, 2, 3, 4);
        mItemAnimator.animateChange(changedHolder, null, 1, 2, 3, 4);
        mItemAnimator.animateAdd(addedHolder);

        assertTrue(mItemAnimator.isRunning());

        InOrder inOrder = Mockito.inOrder(mItemAnimator);

        runAnimationToCompletion();

        // In concurrent clip mode, all animations start immediately together.
        inOrder.verify(mItemAnimator).dispatchRemoveStarting(removedHolder);
        inOrder.verify(mItemAnimator).dispatchMoveStarting(movedHolder);
        inOrder.verify(mItemAnimator).dispatchChangeStarting(changedHolder, true);
        inOrder.verify(mItemAnimator).dispatchAddStarting(addedHolder);

        inOrder.verify(mItemAnimator).dispatchRemoveFinished(removedHolder);
        inOrder.verify(mItemAnimator).dispatchMoveFinished(movedHolder);
        inOrder.verify(mItemAnimator).dispatchChangeFinished(changedHolder, true);
        inOrder.verify(mItemAnimator).dispatchAddFinished(addedHolder);

        verify(mItemAnimator, times(4)).dispatchFinishedWhenAllAnimationsDone();
        assertFalse(mItemAnimator.isRunning());
    }
}
