// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;

import static org.chromium.base.GarbageCollectionTestUtils.canBeGarbageCollected;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.LevelDBPersistedDataStorage;
import org.chromium.chrome.browser.tab.state.LevelDBPersistedDataStorageJni;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChipView;
import org.chromium.ui.widget.ChromeImageView;

import java.lang.ref.WeakReference;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Tests for the {@link androidx.recyclerview.widget.RecyclerView.ViewHolder} classes for {@link
 * TabListCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabListViewHolderTest extends DummyUiActivityTestCase {
    @Rule
    public JniMocker mMocker = new JniMocker();

    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final String EXPECTED_PRICE_STRING = "$287";
    private static final String EXPECTED_PREVIOUS_PRICE_STRING = "$314";

    private ViewGroup mTabGridView;
    private PropertyModel mGridModel;
    private PropertyModelChangeProcessor mGridMCP;

    private ViewGroup mTabStripView;
    private PropertyModel mStripModel;
    private PropertyModelChangeProcessor mStripMCP;

    private ViewGroup mSelectableTabGridView;
    private PropertyModel mSelectableModel;
    private PropertyModelChangeProcessor mSelectableMCP;

    private ViewGroup mTabListView;
    private ViewGroup mSelectableTabListView;

    @Mock
    private Profile mProfile;

    @Mock
    private LevelDBPersistedDataStorage.Natives mLevelDBPersistedTabDataStorage;

    private TabListMediator.ThumbnailFetcher mMockThumbnailProvider =
            new TabListMediator.ThumbnailFetcher(new TabListMediator.ThumbnailProvider() {
                @Override
                public void getTabThumbnailWithCallback(int tabId, Callback<Bitmap> callback,
                        boolean forceUpdate, boolean writeToCache) {
                    Bitmap bitmap = mShouldReturnBitmap
                            ? Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888)
                            : null;
                    callback.onResult(bitmap);
                    mThumbnailFetchedCount.incrementAndGet();
                }
            }, Tab.INVALID_TAB_ID, false, false);
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
        MockitoAnnotations.initMocks(this);
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
            mSelectableTabListView = (ViewGroup) getActivity().getLayoutInflater().inflate(
                    R.layout.selectable_tab_list_card_item, null);
            mTabListView = (ViewGroup) getActivity().getLayoutInflater().inflate(
                    R.layout.closable_tab_list_card_item, null);

            view.addView(mTabGridView);
            view.addView(mTabStripView);
            view.addView(mSelectableTabGridView);
            view.addView(mSelectableTabListView);
            view.addView(mTabListView);
        });

        SelectionDelegate<Integer> mSelectionDelegate = new SelectionDelegate<>();

        int mSelectedTabBackgroundDrawableId = R.drawable.selected_tab_background;
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
            PropertyModelChangeProcessor.create(mSelectableModel, mSelectableTabListView,
                    TabListViewBinder::bindSelectableListTab);
            PropertyModelChangeProcessor.create(
                    mGridModel, mTabListView, TabListViewBinder::bindListTab);
        });
        mMocker.mock(LevelDBPersistedDataStorageJni.TEST_HOOKS, mLevelDBPersistedTabDataStorage);
        doNothing()
                .when(mLevelDBPersistedTabDataStorage)
                .init(any(LevelDBPersistedDataStorage.class), any(BrowserContextHandle.class));
        doReturn(false).when(mProfile).isOffTheRecord();
        LevelDBPersistedDataStorage.setSkipNativeAssertionsForTesting(true);
        Profile.setLastUsedProfileForTesting(mProfile);
    }

    private void testGridSelected(ViewGroup holder, PropertyModel model) {
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.LOLLIPOP_MR1) {
            model.set(TabProperties.IS_SELECTED, true);
            Assert.assertNotNull(holder.getForeground());
            model.set(TabProperties.IS_SELECTED, false);
            Assert.assertNull(holder.getForeground());
        } else {
            model.set(TabProperties.IS_SELECTED, true);
            View selectedView = holder.findViewById(R.id.selected_view_below_lollipop);
            Assert.assertEquals(View.VISIBLE, selectedView.getVisibility());
            model.set(TabProperties.IS_SELECTED, false);
            Assert.assertEquals(View.GONE, selectedView.getVisibility());
        }
    }

    private void testSelectableTabClickToSelect(
            ViewGroup view, PropertyModel model, boolean isLongClick) {
        Runnable clickTask = () -> {
            if (isLongClick) {
                view.performLongClick();
            } else {
                view.performClick();
            }
        };

        model.set(TabProperties.IS_SELECTED, false);
        clickTask.run();
        Assert.assertTrue(mSelectClicked.get());
        mSelectClicked.set(false);

        model.set(TabProperties.IS_SELECTED, true);
        clickTask.run();
        Assert.assertTrue(mSelectClicked.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSelected() {
        testGridSelected(mTabGridView, mGridModel);

        mStripModel.set(TabProperties.IS_SELECTED, true);
        Assert.assertNotNull(((FrameLayout) mTabStripView).getForeground());
        mStripModel.set(TabProperties.IS_SELECTED, false);
        Assert.assertNull(((FrameLayout) mTabStripView).getForeground());

        testGridSelected(mSelectableTabGridView, mSelectableModel);
        mSelectableModel.set(TabProperties.IS_SELECTED, true);
        ImageView actionButton = mSelectableTabGridView.findViewById(R.id.action_button);
        Assert.assertEquals(1, actionButton.getBackground().getLevel());
        Assert.assertNotNull(actionButton.getDrawable());
        Assert.assertEquals(255, actionButton.getDrawable().getAlpha());

        mSelectableModel.set(TabProperties.IS_SELECTED, false);
        Assert.assertEquals(0, actionButton.getBackground().getLevel());
        Assert.assertEquals(0, actionButton.getDrawable().getAlpha());

        testGridSelected(mSelectableTabListView, mSelectableModel);
        mSelectableModel.set(TabProperties.IS_SELECTED, true);
        ImageView endButton = mSelectableTabListView.findViewById(R.id.end_button);
        Assert.assertEquals(1, endButton.getBackground().getLevel());
        Assert.assertNotNull(endButton.getDrawable());
        Assert.assertEquals(255, actionButton.getDrawable().getAlpha());

        mSelectableModel.set(TabProperties.IS_SELECTED, false);
        Assert.assertEquals(0, endButton.getBackground().getLevel());
        Assert.assertEquals(0, endButton.getDrawable().getAlpha());
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

        Assert.assertEquals(View.GONE, backgroundView.getVisibility());
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
            View selectedView = mTabGridView.findViewById(R.id.selected_view_below_lollipop);
            Assert.assertEquals(View.VISIBLE, selectedView.getVisibility());
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
        Assert.assertEquals(View.GONE, backgroundView.getVisibility());

        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
            View selectedView = mTabGridView.findViewById(R.id.selected_view_below_lollipop);
            Assert.assertEquals(View.GONE, selectedView.getVisibility());
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

        textView = mSelectableTabListView.findViewById(R.id.title);
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

    @SuppressWarnings("UnusedAssignment") // Intentionally set to null for garbage collection.
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

    @SuppressWarnings("UnusedAssignment") // Intentionally set to null for garbage collection.
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

    @SuppressWarnings("UnusedAssignment") // Intentionally set to null for garbage collection.
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

    @SuppressWarnings("UnusedAssignment") // Intentionally set to null for garbage collection.
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
        int firstSelectId = mSelectTabId.get();
        Assert.assertEquals(TAB1_ID, firstSelectId);
        mSelectClicked.set(false);
        mSelectTabId.set(Tab.INVALID_TAB_ID);

        Assert.assertFalse(mSelectClicked.get());
        mTabListView.performClick();
        Assert.assertTrue(mSelectClicked.get());
        firstSelectId = mSelectTabId.get();
        Assert.assertEquals(TAB1_ID, firstSelectId);
        mSelectClicked.set(false);

        mGridModel.set(TabProperties.TAB_ID, TAB2_ID);
        mTabGridView.performClick();
        Assert.assertTrue(mSelectClicked.get());
        int secondSelectId = mSelectTabId.get();
        // When TAB_ID in PropertyModel is updated, binder should select tab with updated tab ID.
        Assert.assertEquals(TAB2_ID, secondSelectId);
        Assert.assertNotEquals(firstSelectId, secondSelectId);
        mSelectClicked.set(false);
        mSelectTabId.set(Tab.INVALID_TAB_ID);

        Assert.assertFalse(mSelectClicked.get());
        mTabListView.performClick();
        Assert.assertTrue(mSelectClicked.get());
        secondSelectId = mSelectTabId.get();
        Assert.assertEquals(TAB2_ID, secondSelectId);
        Assert.assertNotEquals(firstSelectId, secondSelectId);
        mSelectClicked.set(false);

        mGridModel.set(TabProperties.TAB_SELECTED_LISTENER, null);
        mTabGridView.performClick();
        Assert.assertFalse(mSelectClicked.get());
        mTabListView.performClick();
        Assert.assertFalse(mSelectClicked.get());
        mSelectClicked.set(false);

        ImageButton button = mTabStripView.findViewById(R.id.tab_strip_item_button);
        mStripModel.set(TabProperties.IS_SELECTED, false);
        button.performClick();
        Assert.assertTrue(mSelectClicked.get());
        mSelectClicked.set(false);

        mStripModel.set(TabProperties.IS_SELECTED, true);
        button.performClick();
        Assert.assertFalse(mSelectClicked.get());
        mSelectClicked.set(false);

        testSelectableTabClickToSelect(mSelectableTabGridView, mSelectableModel, false);
        testSelectableTabClickToSelect(mSelectableTabGridView, mSelectableModel, true);

        // For the List version, we need to trigger the click from the view that has id
        // content_view, because of the xml hierarchy.
        ViewGroup selectableTabListContent = mSelectableTabListView.findViewById(R.id.content_view);
        testSelectableTabClickToSelect(selectableTabListContent, mSelectableModel, false);
        testSelectableTabClickToSelect(selectableTabListContent, mSelectableModel, true);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testClickToClose() {
        ImageView gridActionButton = mTabGridView.findViewById(R.id.action_button);
        ImageView listActionButton = mTabListView.findViewById(R.id.end_button);
        ImageButton button = mTabStripView.findViewById(R.id.tab_strip_item_button);

        Assert.assertFalse(mCloseClicked.get());
        gridActionButton.performClick();
        Assert.assertTrue(mCloseClicked.get());
        int firstCloseId = mCloseTabId.get();
        Assert.assertEquals(TAB1_ID, firstCloseId);

        mCloseClicked.set(false);
        mCloseTabId.set(Tab.INVALID_TAB_ID);

        Assert.assertFalse(mCloseClicked.get());
        listActionButton.performClick();
        Assert.assertTrue(mCloseClicked.get());
        firstCloseId = mCloseTabId.get();
        Assert.assertEquals(TAB1_ID, firstCloseId);

        mCloseClicked.set(false);

        mGridModel.set(TabProperties.TAB_ID, TAB2_ID);
        gridActionButton.performClick();
        Assert.assertTrue(mCloseClicked.get());
        mCloseClicked.set(false);
        int secondClosed = mCloseTabId.get();

        mCloseClicked.set(false);
        mCloseTabId.set(Tab.INVALID_TAB_ID);

        Assert.assertFalse(mCloseClicked.get());
        listActionButton.performClick();
        Assert.assertTrue(mCloseClicked.get());
        secondClosed = mCloseTabId.get();
        // When TAB_ID in PropertyModel is updated, binder should close tab with updated tab ID.
        Assert.assertEquals(TAB2_ID, secondClosed);
        Assert.assertNotEquals(firstCloseId, secondClosed);

        mCloseClicked.set(false);

        mGridModel.set(TabProperties.TAB_CLOSED_LISTENER, null);
        gridActionButton.performClick();
        Assert.assertFalse(mCloseClicked.get());
        listActionButton.performClick();
        Assert.assertFalse(mCloseClicked.get());

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

    @Test
    @MediumTest
    @UiThreadTest
    public void testSearchTermChip() {
        String searchTerm = "hello world";

        testGridSelected(mTabGridView, mGridModel);
        ChipView pageInfoButton = mTabGridView.findViewById(R.id.page_info_button);

        mGridModel.set(TabProperties.SEARCH_QUERY, searchTerm);
        Assert.assertEquals(View.VISIBLE, pageInfoButton.getVisibility());
        Assert.assertEquals(searchTerm, pageInfoButton.getPrimaryTextView().getText());

        mGridModel.set(TabProperties.SEARCH_QUERY, null);
        Assert.assertEquals(View.GONE, pageInfoButton.getVisibility());

        mGridModel.set(TabProperties.SEARCH_QUERY, searchTerm);
        Assert.assertEquals(View.VISIBLE, pageInfoButton.getVisibility());

        mGridModel.set(TabProperties.SEARCH_QUERY, null);
        Assert.assertEquals(View.GONE, pageInfoButton.getVisibility());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testPriceStringPriceDrop() {
        Tab tab = MockTab.createAndInitialize(1, false);
        MockShoppingPersistedTabDataFetcher fetcher = new MockShoppingPersistedTabDataFetcher(tab);
        fetcher.setPriceStrings(EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
        testPriceString(
                tab, fetcher, View.VISIBLE, EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testPriceStringNullPriceDrop() {
        Tab tab = MockTab.createAndInitialize(1, false);
        MockShoppingPersistedTabDataFetcher fetcher = new MockShoppingPersistedTabDataFetcher(tab);
        fetcher.setNullPriceDrop();
        testPriceString(
                tab, fetcher, View.GONE, EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testPriceStringPriceDropThenNull() {
        Tab tab = MockTab.createAndInitialize(1, false);
        MockShoppingPersistedTabDataFetcher fetcher = new MockShoppingPersistedTabDataFetcher(tab);
        fetcher.setPriceStrings(EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
        testPriceString(
                tab, fetcher, View.VISIBLE, EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
        fetcher.setNullPriceDrop();
        testPriceString(
                tab, fetcher, View.GONE, EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testPriceStringTurnFeatureOff() {
        Tab tab = MockTab.createAndInitialize(1, false);
        MockShoppingPersistedTabDataFetcher fetcher = new MockShoppingPersistedTabDataFetcher(tab);
        fetcher.setPriceStrings(EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
        testPriceString(
                tab, fetcher, View.VISIBLE, EXPECTED_PRICE_STRING, EXPECTED_PREVIOUS_PRICE_STRING);
        mGridModel.set(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER, null);
        PriceCardView priceCardView = mTabGridView.findViewById(R.id.price_info_box_outer);
        Assert.assertEquals(View.GONE, priceCardView.getVisibility());
    }

    private void testPriceString(Tab tab, MockShoppingPersistedTabDataFetcher fetcher,
            int expectedVisibility, String expectedCurrentPrice, String expectedPreviousPrice) {
        TabUiFeatureUtilities.ENABLE_PRICE_TRACKING.setForTesting(true);
        testGridSelected(mTabGridView, mGridModel);
        PriceCardView priceCardView = mTabGridView.findViewById(R.id.price_info_box_outer);
        TextView currentPrice = mTabGridView.findViewById(R.id.current_price);
        TextView previousPrice = mTabGridView.findViewById(R.id.previous_price);

        mGridModel.set(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER, fetcher);
        Assert.assertEquals(expectedVisibility, priceCardView.getVisibility());
        if (expectedVisibility == View.VISIBLE) {
            Assert.assertEquals(expectedCurrentPrice, currentPrice.getText());
            Assert.assertEquals(expectedPreviousPrice, previousPrice.getText());
        }
    }

    static class MockShoppingPersistedTabData extends ShoppingPersistedTabData {
        private PriceDrop mPriceDrop;

        MockShoppingPersistedTabData(Tab tab) {
            super(tab);
        }

        public void setPriceStrings(String priceString, String previousPriceString) {
            mPriceDrop = new PriceDrop(priceString, previousPriceString);
        }

        @Override
        public PriceDrop getPriceDrop() {
            return mPriceDrop;
        }
    }

    /**
     * Mock {@link TabListMediator.ShoppingPersistedTabDataFetcher} for testing purposes
     */
    static class MockShoppingPersistedTabDataFetcher
            extends TabListMediator.ShoppingPersistedTabDataFetcher {
        private ShoppingPersistedTabData mShoppingPersistedTabData;
        MockShoppingPersistedTabDataFetcher(Tab tab) {
            super(tab, null, null);
        }

        public void setPriceStrings(String priceString, String previousPriceString) {
            mShoppingPersistedTabData = new MockShoppingPersistedTabData(mTab);
            ((MockShoppingPersistedTabData) mShoppingPersistedTabData)
                    .setPriceStrings(priceString, previousPriceString);
        }

        public void setNullPriceDrop() {
            mShoppingPersistedTabData = new MockShoppingPersistedTabData(mTab);
        }

        @Override
        public void fetch(Callback<ShoppingPersistedTabData> callback) {
            callback.onResult(mShoppingPersistedTabData);
        }
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSearchListener() {
        ChipView pageInfoButton = mTabGridView.findViewById(R.id.page_info_button);

        AtomicInteger clickedTabId = new AtomicInteger(Tab.INVALID_TAB_ID);
        TabListMediator.TabActionListener searchListener = clickedTabId::set;
        mGridModel.set(TabProperties.PAGE_INFO_LISTENER, searchListener);

        pageInfoButton.performClick();
        Assert.assertEquals(TAB1_ID, clickedTabId.get());

        clickedTabId.set(Tab.INVALID_TAB_ID);
        mGridModel.set(TabProperties.PAGE_INFO_LISTENER, null);
        pageInfoButton.performClick();
        Assert.assertEquals(Tab.INVALID_TAB_ID, clickedTabId.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSearchChipIcon() {
        ChipView pageInfoButton = mTabGridView.findViewById(R.id.page_info_button);
        View iconView = pageInfoButton.getChildAt(0);
        Assert.assertTrue(iconView instanceof ChromeImageView);
        ChromeImageView iconImageView = (ChromeImageView) iconView;

        mGridModel.set(TabProperties.PAGE_INFO_ICON_DRAWABLE_ID, R.drawable.ic_logo_googleg_24dp);
        Drawable googleDrawable = iconImageView.getDrawable();

        mGridModel.set(TabProperties.PAGE_INFO_ICON_DRAWABLE_ID, R.drawable.ic_search);
        Drawable magnifierDrawable = iconImageView.getDrawable();

        Assert.assertNotEquals(magnifierDrawable, googleDrawable);
    }

    @Override
    public void tearDownTest() throws Exception {
        mStripMCP.destroy();
        mGridMCP.destroy();
        mSelectableMCP.destroy();
        super.tearDownTest();
    }
}
