// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.MatcherAssert.assertThat;

import static org.chromium.base.GarbageCollectionTestUtils.canBeGarbageCollected;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ButtonCompat;

import java.lang.ref.WeakReference;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Tests for the {@link android.support.v7.widget.RecyclerView.ViewHolder} classes for {@link
 * TabListCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabListViewHolderTest extends DummyUiActivityTestCase {
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;

    private ViewGroup mTabGridView;
    private PropertyModel mGridModel;
    private PropertyModelChangeProcessor mGridMCP;

    private ViewGroup mTabStripView;
    private PropertyModel mStripModel;
    private PropertyModelChangeProcessor mStripMCP;

    private ViewGroup mSelectableTabGridView;
    private PropertyModel mSelectableModel;
    private PropertyModelChangeProcessor mSelectableMCP;
    private SelectionDelegate<Integer> mSelectionDelegate;
    private int mSelectedTabBackgroundDrawableId = R.drawable.selected_tab_background;

    private TabListMediator.ThumbnailFetcher mMockThumbnailProvider =
            new TabListMediator.ThumbnailFetcher(new TabListMediator.ThumbnailProvider() {
                @Override
                public void getTabThumbnailWithCallback(Tab tab, Callback<Bitmap> callback,
                        boolean forceUpdate, boolean writeToCache) {
                    Bitmap bitmap = mShouldReturnBitmap
                            ? Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888)
                            : null;
                    callback.onResult(bitmap);
                    mThumbnailFetchedCount.incrementAndGet();
                }
            }, null, false, false);
    private AtomicInteger mThumbnailFetchedCount = new AtomicInteger();

    private TabListMediator.TabActionListener mMockCloseListener =
            new TabListMediator.TabActionListener() {
                @Override
                public void run(int tabId) {
                    mCloseClicked.set(true);
                    mCloseTabId.set(tabId);
                }
            };
    private AtomicBoolean mCloseClicked = new AtomicBoolean();
    private AtomicInteger mCloseTabId = new AtomicInteger();

    private TabListMediator.TabActionListener mMockSelectedListener =
            new TabListMediator.TabActionListener() {
                @Override
                public void run(int tabId) {
                    mSelectClicked.set(true);
                    mSelectTabId.set(tabId);
                }
            };
    private AtomicBoolean mSelectClicked = new AtomicBoolean();
    private AtomicInteger mSelectTabId = new AtomicInteger();

    private TabListMediator.TabActionListener mMockCreateGroupButtonListener =
            new TabListMediator.TabActionListener() {
                @Override
                public void run(int tabId) {
                    mCreateGroupButtonClicked.set(true);
                    mCreateGroupTabId.set(tabId);
                }
            };
    private AtomicBoolean mCreateGroupButtonClicked = new AtomicBoolean();
    private AtomicInteger mCreateGroupTabId = new AtomicInteger();
    private boolean mShouldReturnBitmap;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        ViewGroup view = new LinearLayout(getActivity());
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getActivity().setContentView(view, params);

            mTabGridView = (ViewGroup) getActivity().getLayoutInflater().inflate(
                    R.layout.closable_tab_grid_card_item, null);
            mTabStripView = (ViewGroup) getActivity().getLayoutInflater().inflate(
                    R.layout.tab_strip_item, null);
            mSelectableTabGridView = (ViewGroup) getActivity().getLayoutInflater().inflate(
                    R.layout.selectable_tab_grid_card_item, null);

            view.addView(mTabGridView);
            view.addView(mTabStripView);
            view.addView(mSelectableTabGridView);
        });

        mSelectionDelegate = new SelectionDelegate<>();

        mGridModel = new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                             .with(TabProperties.TAB_ID, TAB1_ID)
                             .with(TabProperties.TAB_SELECTED_LISTENER, mMockSelectedListener)
                             .with(TabProperties.TAB_CLOSED_LISTENER, mMockCloseListener)
                             .with(TabProperties.SELECTED_TAB_BACKGROUND_DRAWABLE_ID,
                                     mSelectedTabBackgroundDrawableId)
                             .build();
        mStripModel = new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_STRIP)
                              .with(TabProperties.TAB_SELECTED_LISTENER, mMockSelectedListener)
                              .with(TabProperties.TAB_CLOSED_LISTENER, mMockCloseListener)
                              .build();
        mSelectableModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.SELECTABLE_TAB_CLICKED_LISTENER, mMockSelectedListener)
                        .with(TabProperties.TAB_SELECTION_DELEGATE, mSelectionDelegate)
                        .with(TabProperties.SELECTED_TAB_BACKGROUND_DRAWABLE_ID,
                                mSelectedTabBackgroundDrawableId)
                        .build();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mGridMCP = PropertyModelChangeProcessor.create(
                    mGridModel, mTabGridView, TabGridViewBinder::bindClosableTab);
            mStripMCP = PropertyModelChangeProcessor.create(
                    mStripModel, mTabStripView, TabStripViewBinder::bind);
            mSelectableMCP = PropertyModelChangeProcessor.create(
                    mSelectableModel, mSelectableTabGridView, TabGridViewBinder::bindSelectableTab);
        });
    }

    private void testGridSelected(ViewGroup holder, PropertyModel model) {
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.LOLLIPOP_MR1) {
            model.set(TabProperties.IS_SELECTED, true);
            Assert.assertTrue(holder.getForeground() != null);
            model.set(TabProperties.IS_SELECTED, false);
            Assert.assertFalse(holder.getForeground() != null);
        } else {
            model.set(TabProperties.IS_SELECTED, true);
            View selectedView = holder.findViewById(R.id.selected_view_below_lollipop);
            Assert.assertTrue(selectedView.getVisibility() == View.VISIBLE);
            model.set(TabProperties.IS_SELECTED, false);
            Assert.assertTrue(selectedView.getVisibility() == View.GONE);
        }
        mStripModel.set(TabProperties.IS_SELECTED, true);
        Assert.assertTrue(((FrameLayout) mTabStripView).getForeground() != null);
        mStripModel.set(TabProperties.IS_SELECTED, false);
        Assert.assertFalse(((FrameLayout) mTabStripView).getForeground() != null);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSelected() {
        testGridSelected(mTabGridView, mGridModel);

        mStripModel.set(TabProperties.IS_SELECTED, true);
        Assert.assertTrue(((FrameLayout) mTabStripView).getForeground() != null);
        mStripModel.set(TabProperties.IS_SELECTED, false);
        Assert.assertFalse(((FrameLayout) mTabStripView).getForeground() != null);

        testGridSelected(mSelectableTabGridView, mSelectableModel);
        mSelectableModel.set(TabProperties.IS_SELECTED, true);
        ImageView actionButton = mSelectableTabGridView.findViewById(R.id.action_button);
        Assert.assertTrue(actionButton.getBackground().getLevel() == 1);
        Assert.assertTrue(actionButton.getDrawable() != null);
        Assert.assertEquals(255, actionButton.getDrawable().getAlpha());

        mSelectableModel.set(TabProperties.IS_SELECTED, false);
        Assert.assertTrue(actionButton.getBackground().getLevel() == 0);
        Assert.assertEquals(0, actionButton.getDrawable().getAlpha());
    }

    @Test
    @MediumTest
    public void testAnimationRestored() {
        View backgroundView = mTabGridView.findViewById(R.id.background_view);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mGridModel.set(TabProperties.IS_SELECTED, true);
            mGridModel.set(TabProperties.CARD_ANIMATION_STATUS,
                    ClosableTabGridView.AnimationStatus.CARD_RESTORE);
        });
        CriteriaHelper.pollUiThread(
                () -> !((ClosableTabGridView) mTabGridView).getIsAnimatingForTesting());

        Assert.assertTrue(backgroundView.getVisibility() == View.GONE);
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
            View selectedView = mTabGridView.findViewById(R.id.selected_view_below_lollipop);
            Assert.assertTrue(selectedView.getVisibility() == View.VISIBLE);
        } else {
            Drawable selectedDrawable = mTabGridView.getForeground();
            Assert.assertNotNull(selectedDrawable);
        }

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mGridModel.set(TabProperties.IS_SELECTED, false);
            mGridModel.set(TabProperties.CARD_ANIMATION_STATUS,
                    ClosableTabGridView.AnimationStatus.CARD_RESTORE);
        });
        CriteriaHelper.pollUiThread(
                () -> !((ClosableTabGridView) mTabGridView).getIsAnimatingForTesting());
        Assert.assertTrue(backgroundView.getVisibility() == View.GONE);

        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
            View selectedView = mTabGridView.findViewById(R.id.selected_view_below_lollipop);
            Assert.assertTrue(selectedView.getVisibility() == View.GONE);
        } else {
            Drawable selectedDrawable = mTabGridView.getForeground();
            Assert.assertNull(selectedDrawable);
        }
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testTitle() {
        final String title = "Surf the cool webz";
        mGridModel.set(TabProperties.TITLE, title);
        TextView textView = mTabGridView.findViewById(R.id.tab_title);
        Assert.assertEquals(textView.getText(), title);

        mSelectableModel.set(TabProperties.TITLE, title);
        textView = mSelectableTabGridView.findViewById(R.id.tab_title);
        Assert.assertEquals(textView.getText(), title);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testThumbnail() {
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        ImageView thumbnail = mTabGridView.findViewById(R.id.tab_thumbnail);
        Assert.assertNull(thumbnail.getDrawable());
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, null);

        mShouldReturnBitmap = true;
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        assertThat(thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
        Assert.assertEquals(2, mThumbnailFetchedCount.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testThumbnailGCAfterNullBitmap() {
        ImageView thumbnail = mTabGridView.findViewById(R.id.tab_thumbnail);
        mShouldReturnBitmap = true;
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        assertThat(thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
        Bitmap bitmap = ((BitmapDrawable) thumbnail.getDrawable()).getBitmap();
        WeakReference<Bitmap> ref = new WeakReference<>(bitmap);
        bitmap = null;

        Assert.assertFalse(canBeGarbageCollected(ref));

        mShouldReturnBitmap = false;
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        Assert.assertTrue(canBeGarbageCollected(ref));
        Assert.assertEquals(2, mThumbnailFetchedCount.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testThumbnailGCAfterNewBitmap() {
        ImageView thumbnail = mTabGridView.findViewById(R.id.tab_thumbnail);
        mShouldReturnBitmap = true;
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        assertThat(thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
        Bitmap bitmap = ((BitmapDrawable) thumbnail.getDrawable()).getBitmap();
        WeakReference<Bitmap> ref = new WeakReference<>(bitmap);
        bitmap = null;

        Assert.assertFalse(canBeGarbageCollected(ref));

        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        Assert.assertTrue(canBeGarbageCollected(ref));
        Assert.assertEquals(2, mThumbnailFetchedCount.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testResetThumbnailGC() {
        ImageView thumbnail = mTabGridView.findViewById(R.id.tab_thumbnail);
        mShouldReturnBitmap = true;
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        assertThat(thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
        Bitmap bitmap = ((BitmapDrawable) thumbnail.getDrawable()).getBitmap();
        WeakReference<Bitmap> ref = new WeakReference<>(bitmap);
        bitmap = null;

        Assert.assertFalse(canBeGarbageCollected(ref));

        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, null);
        Assert.assertTrue(canBeGarbageCollected(ref));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testHiddenGC() {
        ImageView thumbnail = mTabGridView.findViewById(R.id.tab_thumbnail);
        mShouldReturnBitmap = true;
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        assertThat(thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
        Bitmap bitmap = ((BitmapDrawable) thumbnail.getDrawable()).getBitmap();
        WeakReference<Bitmap> ref = new WeakReference<>(bitmap);
        bitmap = null;

        Assert.assertFalse(canBeGarbageCollected(ref));

        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, null);
        Assert.assertTrue(canBeGarbageCollected(ref));
        Assert.assertNull(thumbnail.getDrawable());
        Assert.assertEquals(1, mThumbnailFetchedCount.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testHiddenThenShow() {
        ImageView thumbnail = mTabGridView.findViewById(R.id.tab_thumbnail);
        mShouldReturnBitmap = true;
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        assertThat(thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
        Assert.assertEquals(1, mThumbnailFetchedCount.get());

        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, null);
        Assert.assertNull(thumbnail.getDrawable());
        Assert.assertEquals(1, mThumbnailFetchedCount.get());

        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        assertThat(thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
        Assert.assertEquals(2, mThumbnailFetchedCount.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testClickToSelect() {
        Assert.assertFalse(mSelectClicked.get());
        mTabGridView.performClick();
        Assert.assertTrue(mSelectClicked.get());
        mSelectClicked.set(false);
        int firstSelectId = mSelectTabId.get();
        Assert.assertEquals(TAB1_ID, firstSelectId);

        mGridModel.set(TabProperties.TAB_ID, TAB2_ID);
        mTabGridView.performClick();
        Assert.assertTrue(mSelectClicked.get());
        mSelectClicked.set(false);
        int secondSelectId = mSelectTabId.get();
        // When TAB_ID in PropertyModel is updated, binder should select tab with updated tab ID.
        Assert.assertEquals(TAB2_ID, secondSelectId);
        Assert.assertNotEquals(firstSelectId, secondSelectId);

        ImageButton button = mTabStripView.findViewById(R.id.tab_strip_item_button);
        mStripModel.set(TabProperties.IS_SELECTED, false);
        button.performClick();
        Assert.assertTrue(mSelectClicked.get());
        mSelectClicked.set(false);

        mStripModel.set(TabProperties.IS_SELECTED, true);
        button.performClick();
        Assert.assertFalse(mSelectClicked.get());
        mSelectClicked.set(false);

        mSelectableModel.set(TabProperties.IS_SELECTED, false);
        mSelectableTabGridView.performClick();
        Assert.assertTrue(mSelectClicked.get());
        mSelectClicked.set(false);

        mSelectableModel.set(TabProperties.IS_SELECTED, true);
        mSelectableTabGridView.performClick();
        Assert.assertTrue(mSelectClicked.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testClickToClose() {
        ImageView actionButton = mTabGridView.findViewById(R.id.action_button);
        ImageButton button = mTabStripView.findViewById(R.id.tab_strip_item_button);
        Assert.assertFalse(mCloseClicked.get());
        actionButton.performClick();
        Assert.assertTrue(mCloseClicked.get());
        mCloseClicked.set(false);
        int firstCloseId = mCloseTabId.get();
        Assert.assertEquals(TAB1_ID, firstCloseId);

        mGridModel.set(TabProperties.TAB_ID, TAB2_ID);
        actionButton.performClick();
        Assert.assertTrue(mCloseClicked.get());
        mCloseClicked.set(false);
        int secondClosed = mCloseTabId.get();
        // When TAB_ID in PropertyModel is updated, binder should close tab with updated tab ID.
        Assert.assertEquals(TAB2_ID, secondClosed);
        Assert.assertNotEquals(firstCloseId, secondClosed);

        mStripModel.set(TabProperties.IS_SELECTED, true);
        button.performClick();
        Assert.assertTrue(mCloseClicked.get());
        mCloseClicked.set(false);

        mStripModel.set(TabProperties.IS_SELECTED, false);
        button.performClick();
        Assert.assertFalse(mCloseClicked.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSetCreateGroupListener() {
        ButtonCompat actionButton = mTabGridView.findViewById(R.id.create_group_button);
        // By default, the create group button is invisible.
        Assert.assertEquals(View.GONE, actionButton.getVisibility());

        // When setup with actual listener, the button should be visible.
        mGridModel.set(TabProperties.CREATE_GROUP_LISTENER, mMockCreateGroupButtonListener);
        Assert.assertEquals(View.VISIBLE, actionButton.getVisibility());
        Assert.assertFalse(mCreateGroupButtonClicked.get());
        actionButton.performClick();
        Assert.assertTrue(mCreateGroupButtonClicked.get());
        mCreateGroupButtonClicked.set(false);
        int firstCreateGroupId = mCreateGroupTabId.get();
        Assert.assertEquals(TAB1_ID, firstCreateGroupId);

        mGridModel.set(TabProperties.TAB_ID, TAB2_ID);
        actionButton.performClick();
        Assert.assertTrue(mCreateGroupButtonClicked.get());
        mCreateGroupButtonClicked.set(false);
        int secondCreateGroupId = mCreateGroupTabId.get();
        // When TAB_ID in PropertyModel is updated, binder should create group with updated tab ID.
        Assert.assertEquals(TAB2_ID, secondCreateGroupId);
        Assert.assertNotEquals(firstCreateGroupId, secondCreateGroupId);

        mGridModel.set(TabProperties.CREATE_GROUP_LISTENER, null);
        actionButton.performClick();
        Assert.assertFalse(mCreateGroupButtonClicked.get());
        // When CREATE_GROUP_LISTENER is set to null, the button should be invisible.
        Assert.assertEquals(View.GONE, actionButton.getVisibility());
    }

    @Override
    public void tearDownTest() throws Exception {
        mStripMCP.destroy();
        mGridMCP.destroy();
        mSelectableMCP.destroy();
        super.tearDownTest();
    }
}
