// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.USE_SHRINK_CLOSE_ANIMATION;

import android.util.Pair;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter.ViewHolder;

/** Unit tests for {@link TabListItemAnimator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabListItemAnimatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SimpleRecyclerViewAdapter mAdapter;
    private TabListItemAnimator mItemAnimator;

    @Before
    public void setUp() {
        mItemAnimator = spy(new TabListItemAnimator());
    }

    private static void emptyBind(PropertyModel model, View view, PropertyKey key) {}

    private ViewHolder buildViewHolder(@ModelType int modelType, boolean useShrinkCloseAnimation) {
        View itemView = mock(View.class);
        when(itemView.getAlpha()).thenReturn(1f);
        when(itemView.getTranslationX()).thenReturn(0f);
        when(itemView.getTranslationY()).thenReturn(0f);
        when(itemView.getVisibility()).thenReturn(View.VISIBLE);
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
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }

    private void animateAddWithCompletionTrigger(Callback<ViewHolder> completionTrigger) {
        var holder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);

        assertTrue(mItemAnimator.animateAdd(holder));
        verify(holder.itemView).setAlpha(0f);

        assertTrue(mItemAnimator.isRunning());

        InOrder inOrder = Mockito.inOrder(mItemAnimator);

        completionTrigger.onResult(holder);

        // No guarantee this happens only once due to the animation loop.
        verify(holder.itemView, atLeastOnce()).setAlpha(1f);
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
        verify(holder.itemView).setTranslationX(-50);
        verify(holder.itemView).setTranslationY(-100);

        assertTrue(mItemAnimator.isRunning());

        InOrder inOrder = Mockito.inOrder(mItemAnimator);

        runAnimationToCompletion();

        // Cannot be accurate regarding animation interaction count.
        verify(holder.itemView, atLeastOnce()).setTranslationX(anyFloat());
        verify(holder.itemView, atLeastOnce()).setTranslationY(anyFloat());
        inOrder.verify(mItemAnimator).dispatchMoveStarting(holder);
        inOrder.verify(mItemAnimator).dispatchMoveFinished(holder);
        inOrder.verify(mItemAnimator).dispatchFinishedWhenAllAnimationsDone();
        verify(holder.itemView, atLeastOnce()).setTranslationX(0f);
        verify(holder.itemView, atLeastOnce()).setTranslationY(0f);

        assertFalse(mItemAnimator.isRunning());
    }

    @Test
    public void animateChange_SingleHolder_RunToCompletion() {
        var holder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);

        float x = 40f;
        float y = 30f;
        float alpha = 0.3f;
        when(holder.itemView.getTranslationX()).thenReturn(x);
        when(holder.itemView.getTranslationY()).thenReturn(y);
        when(holder.itemView.getAlpha()).thenReturn(alpha);
        assertTrue(mItemAnimator.animateChange(holder, null, 0, 100, 50, 200));
        verify(holder.itemView).setTranslationX(x);
        verify(holder.itemView).setTranslationY(y);
        verify(holder.itemView).setAlpha(alpha);

        assertTrue(mItemAnimator.isRunning());

        InOrder inOrder = Mockito.inOrder(mItemAnimator);

        runAnimationToCompletion();

        // Cannot be accurate regarding animation interaction count.
        verify(holder.itemView, atLeastOnce()).setAlpha(anyFloat());
        verify(holder.itemView, atLeastOnce()).setTranslationX(anyFloat());
        verify(holder.itemView, atLeastOnce()).setTranslationY(anyFloat());
        inOrder.verify(mItemAnimator).dispatchChangeStarting(holder, true);
        inOrder.verify(mItemAnimator).dispatchChangeFinished(holder, true);
        inOrder.verify(mItemAnimator).dispatchFinishedWhenAllAnimationsDone();
        verify(holder.itemView, atLeastOnce()).setAlpha(0f);
        verify(holder.itemView, atLeastOnce()).setTranslationX(50);
        verify(holder.itemView, atLeastOnce()).setTranslationY(100);

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
        when(oldHolder.itemView.getTranslationX()).thenReturn(x);
        when(oldHolder.itemView.getTranslationY()).thenReturn(y);
        when(oldHolder.itemView.getAlpha()).thenReturn(alpha);

        assertTrue(mItemAnimator.animateChange(oldHolder, newHolder, 0, 100, 50, 200));

        verify(oldHolder.itemView).setTranslationX(x);
        verify(oldHolder.itemView).setTranslationY(y);
        verify(oldHolder.itemView).setAlpha(alpha);
        verify(newHolder.itemView).setTranslationX(-10);
        verify(newHolder.itemView).setTranslationY(-70);
        verify(newHolder.itemView).setAlpha(alpha);

        assertTrue(mItemAnimator.isRunning());

        completionTrigger.onResult(Pair.create(oldHolder, newHolder));

        // Cannot be accurate regarding animation interaction count.
        verify(oldHolder.itemView, atLeastOnce()).setAlpha(anyFloat());
        verify(oldHolder.itemView, atLeastOnce()).setTranslationX(anyFloat());
        verify(oldHolder.itemView, atLeastOnce()).setTranslationY(anyFloat());
        verify(newHolder.itemView, atLeastOnce()).setAlpha(anyFloat());
        verify(newHolder.itemView, atLeastOnce()).setTranslationX(anyFloat());
        verify(newHolder.itemView, atLeastOnce()).setTranslationY(anyFloat());

        // Order cannot be verified due to HashMap usage.
        verify(mItemAnimator).dispatchChangeStarting(oldHolder, true);
        verify(mItemAnimator).dispatchChangeFinished(oldHolder, true);
        verify(mItemAnimator).dispatchChangeStarting(newHolder, false);
        verify(mItemAnimator).dispatchChangeFinished(newHolder, false);

        verify(oldHolder.itemView, atLeastOnce()).setAlpha(0f);
        verify(oldHolder.itemView, atLeastOnce()).setTranslationX(50);
        verify(oldHolder.itemView, atLeastOnce()).setTranslationY(100);
        verify(newHolder.itemView, atLeastOnce()).setAlpha(1f);
        verify(newHolder.itemView, atLeastOnce()).setTranslationX(0);
        verify(newHolder.itemView, atLeastOnce()).setTranslationY(0);

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
        verify(holder.itemView).setTranslationX(350);
        verify(holder.itemView).setTranslationY(100);

        assertTrue(mItemAnimator.isRunning());

        InOrder inOrder = Mockito.inOrder(mItemAnimator);

        completionTrigger.onResult(holder);

        // Cannot be accurate regarding animation interaction count.
        verify(holder.itemView, atLeastOnce()).setTranslationX(anyFloat());
        verify(holder.itemView, atLeastOnce()).setTranslationY(anyFloat());
        inOrder.verify(mItemAnimator).dispatchMoveStarting(holder);
        inOrder.verify(mItemAnimator).dispatchMoveFinished(holder);
        inOrder.verify(mItemAnimator).dispatchFinishedWhenAllAnimationsDone();
        verify(holder.itemView, atLeastOnce()).setTranslationX(0f);
        verify(holder.itemView, atLeastOnce()).setTranslationY(0f);

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
        when(holder.itemView.getAlpha()).thenReturn(0f);

        assertFalse(mItemAnimator.animateRemove(holder));
        verify(mItemAnimator).dispatchRemoveFinished(holder);
    }

    @Test
    public void animateRemove_NotVisible() {
        var holder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);
        when(holder.itemView.getVisibility()).thenReturn(View.INVISIBLE);

        assertFalse(mItemAnimator.animateRemove(holder));
        verify(mItemAnimator).dispatchRemoveFinished(holder);
    }

    private void animateTabRemoveWithCompletionTrigger(Callback<ViewHolder> completionTrigger) {
        var holder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ true);

        assertTrue(mItemAnimator.animateRemove(holder));

        assertTrue(mItemAnimator.isRunning());

        InOrder inOrder = Mockito.inOrder(mItemAnimator);

        completionTrigger.onResult(holder);

        // Cannot be accurate regarding animation.
        verify(holder.itemView, atLeastOnce()).setScaleX(anyFloat());
        verify(holder.itemView, atLeastOnce()).setScaleY(anyFloat());
        inOrder.verify(mItemAnimator).dispatchRemoveStarting(holder);
        inOrder.verify(mItemAnimator).dispatchRemoveFinished(holder);
        inOrder.verify(mItemAnimator).dispatchFinishedWhenAllAnimationsDone();
        verify(holder.itemView, atLeastOnce()).setAlpha(1f);
        verify(holder.itemView, atLeastOnce()).setScaleX(1f);
        verify(holder.itemView, atLeastOnce()).setScaleY(1f);

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
        assertTrue(mItemAnimator.animateRemove(holder));

        assertTrue(mItemAnimator.isRunning());

        InOrder inOrder = Mockito.inOrder(mItemAnimator);

        completionTrigger.onResult(holder);

        verify(holder.itemView, atLeastOnce()).setAlpha(0f);
        inOrder.verify(mItemAnimator).dispatchRemoveStarting(holder);
        inOrder.verify(mItemAnimator).dispatchRemoveFinished(holder);
        inOrder.verify(mItemAnimator).dispatchFinishedWhenAllAnimationsDone();
        verify(holder.itemView, atLeastOnce()).setAlpha(1f);

        assertFalse(mItemAnimator.isRunning());
    }

    @Test
    public void animateRemove_NonTabCard_RunToCompletion() {
        var holder = buildViewHolder(MESSAGE, /* useShrinkCloseAnimation= */ false);
        animateNonTabRemoveWithCompletionTrigger(holder, unused -> runAnimationToCompletion());
    }

    @Test
    public void animateRemove_NonTabCard_EndAnimation() {
        var holder = buildViewHolder(MESSAGE, /* useShrinkCloseAnimation= */ false);
        animateNonTabRemoveWithCompletionTrigger(holder, mItemAnimator::endAnimation);
    }

    @Test
    public void animateRemove_NonTabCard_EndAnimations() {
        var holder = buildViewHolder(MESSAGE, /* useShrinkCloseAnimation= */ false);
        animateNonTabRemoveWithCompletionTrigger(holder, unused -> mItemAnimator.endAnimations());
    }

    @Test
    public void animateRemove_TabCardNoShrink() {
        var holder = buildViewHolder(TAB, /* useShrinkCloseAnimation= */ false);
        animateNonTabRemoveWithCompletionTrigger(holder, unused -> mItemAnimator.endAnimations());
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
}
