// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Canvas;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_groups.TabGroupsFeatureMap;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link VerticalTabGroupSpineDecoration}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(TabGroupsFeatureMap.UPDATE_TAB_GROUP_COLORS)
public class VerticalTabGroupSpineDecorationUnitTest {
    private static final int PARENT_WIDTH = 100;
    private static final int ITEM_HEIGHT = 10;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Canvas mCanvas;
    @Mock private Runnable mInvalidationTrigger;
    @Mock private RecyclerView.State mState;

    private RecyclerView mParent;
    private LinearLayoutManager mLayoutManager;
    private TabListModel mModel;
    private VerticalTabGroupSpineDecoration mSpineDecoration;
    private int mMarginBottom;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mMarginBottom =
                activity.getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_item_margin_bottom);
        mParent = new RecyclerView(activity);
        mLayoutManager = new LinearLayoutManager(activity);
        mParent.setLayoutManager(mLayoutManager);
        mParent.setAdapter(new ItemAdapter());

        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);

        SettableMonotonicObservableSupplier<TabModel> currentTabModelSupplier =
                ObservableSuppliers.createMonotonic();
        currentTabModelSupplier.set(mTabModel);
        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(currentTabModelSupplier);

        mModel = new TabListModel();
        mSpineDecoration =
                new VerticalTabGroupSpineDecoration(
                        activity, mInvalidationTrigger, mModel, mTabModelSelector);
    }

    @Test
    public void testDidChangeTabGroupColor_InvalidatesRecyclerView() {
        Token groupId = new Token(1L, 2L);

        mSpineDecoration.onDraw(mCanvas, mParent, mState);

        ArgumentCaptor<TabGroupObserver> captor = ArgumentCaptor.forClass(TabGroupObserver.class);
        verify(mTabModel).addTabGroupObserver(captor.capture());

        captor.getValue().didChangeTabGroupColor(groupId, 0);

        verify(mInvalidationTrigger).run();
    }

    @Test
    public void testOnDraw_HeaderScrolledOffScreen_StartsFromFirstVisibleChildTop() {
        Token groupId = new Token(1L, 2L);
        when(mTabModel.getTabGroupColorWithFallback(groupId)).thenReturn(0);

        // Model has Header (0), Tab 1 (1), Tab 2 (2)
        addGroupHeaderModel(groupId, /* isCollapsed= */ false);
        addTabModel(groupId);
        addTabModel(groupId);

        // Header is scrolled off, parent only has Tab 1 and Tab 2
        layOutItems(/* firstPosition= */ 1);
        assertEquals(1, mParent.getChildAdapterPosition(mParent.getChildAt(0)));
        // tab 1 (adapter position 1)
        int tab1Top = 50;
        int tab1Bottom = 150;
        float transY = 10f;
        setUpChild(tab1Top, tab1Bottom, transY, /* childIndex= */ 0);
        // tab 2 (adapter position 2)
        int tab2Bottom = 260;
        setUpChild(/* top= */ 160, tab2Bottom, transY, /* childIndex= */ 1);

        mSpineDecoration.onDraw(mCanvas, mParent, mState);

        ArgumentCaptor<android.graphics.RectF> rectCaptor =
                ArgumentCaptor.forClass(android.graphics.RectF.class);
        verify(mCanvas).drawRoundRect(rectCaptor.capture(), anyFloat(), anyFloat(), any());

        // Top should start directly at Tab 1's top edge (no margin, no header bottom)
        float expectedTop = tab1Top + transY;
        // Bottom should be Tab 2's bottom
        float expectedBottom = tab2Bottom + transY;

        assertEquals(expectedTop, rectCaptor.getValue().top, 0.01f);
        assertEquals(expectedBottom, rectCaptor.getValue().bottom, 0.01f);
    }

    @Test
    public void testOnDraw_ScrambledChildren_SortsAndDrawsCorrectly() {
        Token groupId = new Token(1L, 2L);
        when(mTabModel.getTabGroupColorWithFallback(groupId)).thenReturn(0);

        addGroupHeaderModel(groupId, /* isCollapsed= */ false);
        addTabModel(groupId);

        // DELIBERATELY SCRAMBLE the physical ViewGroup order:
        // parent.getChildAt(0) returns the child tab (Adapter position 1)
        // parent.getChildAt(1) returns the Header (Adapter position 0)
        layOutItems(/* firstPosition= */ 0);
        // Reorders children without a relayout (mParent is detached); adapter positions still come
        // from each child's ViewHolder.
        mParent.bringChildToFront(mParent.getChildAt(0));
        assertEquals(1, mParent.getChildAdapterPosition(mParent.getChildAt(0)));
        assertEquals(0, mParent.getChildAdapterPosition(mParent.getChildAt(1)));
        // tab view
        int tabBottom = 210;
        float transY = 20f;
        setUpChild(/* top= */ 110, tabBottom, transY, /* childIndex= */ 0);
        // header view
        int headerBottom = 100;
        setUpChild(/* top= */ 0, headerBottom, transY, /* childIndex= */ 1);

        mSpineDecoration.onDraw(mCanvas, mParent, mState);

        ArgumentCaptor<android.graphics.RectF> rectCaptor =
                ArgumentCaptor.forClass(android.graphics.RectF.class);
        verify(mCanvas).drawRoundRect(rectCaptor.capture(), anyFloat(), anyFloat(), any());

        float expectedTop = headerBottom + transY + mMarginBottom;
        float expectedBottom = tabBottom + transY;
        assertEquals(expectedTop, rectCaptor.getValue().top, 0.01f);
        assertEquals(expectedBottom, rectCaptor.getValue().bottom, 0.01f);
    }

    @Test
    public void testOnDraw_WithGroupExpanding_ConnectsToSibling() {
        testOnDraw_WithGroupExpandingOrCollapsing_ConnectsToSibling(/* isCollapsing= */ false);
    }

    @Test
    public void testOnDraw_WithGroupCollapsing_ConnectsToSibling() {
        testOnDraw_WithGroupExpandingOrCollapsing_ConnectsToSibling(/* isCollapsing= */ true);
    }

    private void testOnDraw_WithGroupExpandingOrCollapsing_ConnectsToSibling(boolean isCollapsing) {
        Token groupId = new Token(1L, 2L);
        when(mTabModel.getTabGroupColorWithFallback(groupId)).thenReturn(0);

        // Capture TabGroupObserver
        ArgumentCaptor<TabGroupObserver> captor = ArgumentCaptor.forClass(TabGroupObserver.class);
        verify(mTabModel).addTabGroupObserver(captor.capture());
        TabGroupObserver observer = captor.getValue();

        addGroupHeaderModel(groupId, /* isCollapsed= */ false);
        addTabModel(groupId);
        addTabModel(/* groupId= */ null);

        // Parent has Header, Tab, Sibling tab
        // header view
        int headerBottom = 100;
        float headerTransY = 10f;
        layOutItems(/* firstPosition= */ 0);
        setUpChild(/* top= */ 0, headerBottom, headerTransY, /* childIndex= */ 0);
        // tab view
        setUpChild(/* top= */ 110, /* bottom= */ 210, /* translationY= */ 0, /* childIndex= */ 1);
        // Sibling view
        int nextTop = 300;
        float nextTranslationY = 15f;
        setUpChild(nextTop, /* bottom= */ 400, nextTranslationY, /* childIndex= */ 2);

        // Trigger expand
        observer.didChangeTabGroupCollapsed(groupId, isCollapsing, true);

        mSpineDecoration.onDraw(mCanvas, mParent, mState);

        ArgumentCaptor<android.graphics.RectF> rectCaptor =
                ArgumentCaptor.forClass(android.graphics.RectF.class);
        verify(mCanvas).drawRoundRect(rectCaptor.capture(), anyFloat(), anyFloat(), any());

        float expectedTop = headerBottom + headerTransY + mMarginBottom;
        float expectedBottom = nextTop + nextTranslationY - mMarginBottom;
        assertEquals(expectedTop, rectCaptor.getValue().top, 0.01f);
        assertEquals(expectedBottom, rectCaptor.getValue().bottom, 0.01f);
    }

    @Test
    public void testOnDraw_CollapsedGroup_SkipsDrawing() {
        Token groupId = new Token(1L, 2L);
        when(mTabModel.getTabGroupColorWithFallback(groupId)).thenReturn(0);

        addGroupHeaderModel(groupId, /* isCollapsed= */ true);

        layOutItems(/* firstPosition= */ 0);
        setUpChild(/* top= */ 0, /* bottom= */ 100, /* translationY= */ 0, /* childIndex= */ 0);

        mSpineDecoration.onDraw(mCanvas, mParent, mState);

        verify(mCanvas, never()).drawRoundRect(any(), anyFloat(), anyFloat(), any());
    }

    @Test
    public void testOnDraw_GroupDragging_TranslatesSpine() {
        Token groupId = new Token(1L, 2L);
        when(mTabModel.getTabGroupColorWithFallback(groupId)).thenReturn(0);
        when(mTabModel.getTabCountForGroup(groupId)).thenReturn(2);

        PropertyModel headerModel = addGroupHeaderModel(groupId, /* isCollapsed= */ false);
        float draggingY = 50f;
        when(headerModel.get(TabProperties.DRAGGING_Y)).thenReturn(draggingY);
        addTabModel(groupId);

        // header view
        int headerBottom = 100;
        layOutItems(/* firstPosition= */ 0);
        setUpChild(/* top= */ 0, headerBottom, /* translationY= */ 20f, /* childIndex= */ 0);
        // tab view
        int tabBottom = 210;
        setUpChild(/* top= */ 110, tabBottom, /* translationY= */ 20f, /* childIndex= */ 1);

        mSpineDecoration.onDraw(mCanvas, mParent, mState);

        ArgumentCaptor<android.graphics.RectF> rectCaptor =
                ArgumentCaptor.forClass(android.graphics.RectF.class);
        verify(mCanvas).drawRoundRect(rectCaptor.capture(), anyFloat(), anyFloat(), any());

        float expectedTop = headerBottom + draggingY + mMarginBottom;
        float expectedBottom = tabBottom + draggingY;

        assertEquals(expectedTop, rectCaptor.getValue().top, 0.01f);
        assertEquals(expectedBottom, rectCaptor.getValue().bottom, 0.01f);
    }

    @Test
    public void testOnDraw_SingleChildDragging_PromotedToGroupDragging() {
        Token groupId = new Token(1L, 2L);
        when(mTabModel.getTabGroupColorWithFallback(groupId)).thenReturn(0);
        when(mTabModel.getTabCountForGroup(groupId)).thenReturn(1);

        addGroupHeaderModel(groupId, /* isCollapsed= */ false);
        PropertyModel tabModel = addTabModel(groupId);
        float draggingY = 50f;
        when(tabModel.get(TabProperties.DRAGGING_Y)).thenReturn(draggingY);

        // header view
        int headerBottom = 100;
        layOutItems(/* firstPosition= */ 0);
        setUpChild(/* top= */ 0, headerBottom, /* translationY= */ 20f, /* childIndex= */ 0);
        // tab view
        int tabBottom = 210;
        setUpChild(/* top= */ 110, tabBottom, /* translationY= */ 20f, /* childIndex= */ 1);

        mSpineDecoration.onDraw(mCanvas, mParent, mState);

        ArgumentCaptor<android.graphics.RectF> rectCaptor =
                ArgumentCaptor.forClass(android.graphics.RectF.class);
        verify(mCanvas).drawRoundRect(rectCaptor.capture(), anyFloat(), anyFloat(), any());

        float expectedTop = headerBottom + draggingY + mMarginBottom;
        float expectedBottom = tabBottom + draggingY;

        assertEquals(expectedTop, rectCaptor.getValue().top, 0.01f);
        assertEquals(expectedBottom, rectCaptor.getValue().bottom, 0.01f);
    }

    @Test
    public void testOnDraw_ChildDraggingInternally_LocksSpine() {
        Token groupId = new Token(1L, 2L);
        when(mTabModel.getTabGroupColorWithFallback(groupId)).thenReturn(0);
        when(mTabModel.getTabCountForGroup(groupId)).thenReturn(2);

        addGroupHeaderModel(groupId, /* isCollapsed= */ false);
        addTabModel(groupId);
        PropertyModel tabModel2 = addTabModel(groupId);
        when(tabModel2.get(TabProperties.DRAGGING_Y)).thenReturn(50f);

        // header view
        int headerBottom = 100;
        layOutItems(/* firstPosition= */ 0);
        setUpChild(/* top= */ 0, headerBottom, /* translationY= */ 20f, /* childIndex= */ 0);
        // tab view 1
        setUpChild(/* top= */ 110, /* bottom= */ 210, /* translationY= */ 20f, /* childIndex= */ 1);
        // tab view 2
        int tab2Bottom = 320;
        setUpChild(/* top= */ 220, tab2Bottom, /* translationY= */ 30f, /* childIndex= */ 2);

        mSpineDecoration.onDraw(mCanvas, mParent, mState);

        ArgumentCaptor<android.graphics.RectF> rectCaptor =
                ArgumentCaptor.forClass(android.graphics.RectF.class);
        verify(mCanvas).drawRoundRect(rectCaptor.capture(), anyFloat(), anyFloat(), any());

        assertEquals(headerBottom + mMarginBottom, rectCaptor.getValue().top, 0.01f);
        assertEquals(tab2Bottom, rectCaptor.getValue().bottom, 0.01f);
    }

    @Test
    public void testOnDraw_DragOtherTabs_DoesNotAnimate() {
        Token groupId = new Token(1L, 2L);
        when(mTabModel.getTabGroupColorWithFallback(groupId)).thenReturn(0);

        // Add a standalone tab at position 0, which is being dragged
        PropertyModel otherTabModel = addTabModel(/* groupId= */ null);
        when(otherTabModel.get(TabProperties.DRAGGING_Y)).thenReturn(50f);
        // Add the group after the standalone tab
        addGroupHeaderModel(groupId, /* isCollapsed= */ false);
        addTabModel(groupId);
        addTabModel(groupId);

        layOutItems(/* firstPosition= */ 0);
        // Parent has Other Tab (0), Header (1), Tab 1 (2), and Tab 2 (3)
        // Dragged view
        setUpChild(/* top= */ 0, /* bottom= */ 100, /* translationY= */ 20f, /* childIndex= */ 0);
        // header view
        int headerBottom = 210;
        float transY = 10f;
        setUpChild(/* top= */ 110, headerBottom, transY, /* childIndex= */ 1);
        // tab 1 (stable)
        View tabView1 = setUpChild(/* top= */ 220, /* bottom= */ 320, transY, /* childIndex= */ 2);
        tabView1.setAlpha(1f);
        // tab 2 (animating out, alpha = 0.5)
        int tab2Bottom = 430;
        View tabView2 = setUpChild(/* top= */ 330, tab2Bottom, transY, /* childIndex= */ 3);
        tabView2.setAlpha(0.5f);

        mSpineDecoration.onDraw(mCanvas, mParent, mState);

        ArgumentCaptor<android.graphics.RectF> rectCaptor =
                ArgumentCaptor.forClass(android.graphics.RectF.class);
        verify(mCanvas).drawRoundRect(rectCaptor.capture(), anyFloat(), anyFloat(), any());

        float expectedTop = headerBottom + transY + mMarginBottom;
        assertEquals(expectedTop, rectCaptor.getValue().top, 0.01f);
        // Since another tab is dragging (at pos 0), we should return targetBottom directly rather
        // than the alpha-interpolated value.
        float expectedBottom = tab2Bottom + transY;
        assertEquals(expectedBottom, rectCaptor.getValue().bottom, 0.01f);
    }

    @Test
    public void testOnDraw_LastGroupOnScreen_AnimatesWithAlpha() {
        Token groupId = new Token(1L, 2L);
        when(mTabModel.getTabGroupColorWithFallback(groupId)).thenReturn(0);

        addGroupHeaderModel(groupId, /* isCollapsed= */ false);
        addTabModel(groupId);
        addTabModel(groupId);

        // header view
        int headerBottom = 110;
        float transY = 10f;
        layOutItems(/* firstPosition= */ 0);
        setUpChild(/* top= */ 0, headerBottom, transY, /* childIndex= */ 0);
        // tab 1 (stable)
        int tab1Bottom = 220;
        View tabView1 = setUpChild(/* top= */ 110, tab1Bottom, transY, /* childIndex= */ 1);
        tabView1.setAlpha(1f);
        // tab 2 (animating out, alpha = 0.5)
        int tab2Bottom = 330;
        View tabView2 = setUpChild(/* top= */ 230, tab2Bottom, transY, /* childIndex= */ 2);
        tabView2.setAlpha(0.5f);

        mSpineDecoration.onDraw(mCanvas, mParent, mState);

        ArgumentCaptor<android.graphics.RectF> rectCaptor =
                ArgumentCaptor.forClass(android.graphics.RectF.class);
        verify(mCanvas).drawRoundRect(rectCaptor.capture(), anyFloat(), anyFloat(), any());

        float expectedTop = headerBottom + transY + mMarginBottom;
        assertEquals(expectedTop, rectCaptor.getValue().top, 0.01f);
        float expectedBottom = tab1Bottom + transY + (tab2Bottom - tab1Bottom) * 0.5f;
        assertEquals(expectedBottom, rectCaptor.getValue().bottom, 0.01f);
    }

    private PropertyModel addGroupHeaderModel(Token groupId, boolean isCollapsed) {
        PropertyModel headerModel = mock(PropertyModel.class);
        when(headerModel.get(TabProperties.TAB_GROUP_HEADER_ID)).thenReturn(groupId);
        when(headerModel.get(TabProperties.IS_COLLAPSED)).thenReturn(isCollapsed);
        mModel.add(new ListItem(0, headerModel));
        return headerModel;
    }

    private PropertyModel addTabModel(Token groupId) {
        PropertyModel tabModel = mock(PropertyModel.class);
        when(tabModel.get(TabProperties.TAB_GROUP_ID)).thenReturn(groupId);
        mModel.add(new ListItem(0, tabModel));
        return tabModel;
    }

    /**
     * Lays out {@link #mParent} so that its children are exactly the items from {@code
     * firstPosition} to the end of {@link #mModel}, in adapter order. Item height is irrelevant
     * (tests override child positions via {@link #setUpChild}); it only sizes the parent to fit.
     */
    private void layOutItems(int firstPosition) {
        int itemCount = mModel.size() - firstPosition;
        mParent.getAdapter().notifyDataSetChanged();
        mLayoutManager.scrollToPositionWithOffset(firstPosition, 0);
        mParent.measure(
                View.MeasureSpec.makeMeasureSpec(PARENT_WIDTH, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(
                        itemCount * ITEM_HEIGHT, View.MeasureSpec.EXACTLY));
        mParent.layout(0, 0, PARENT_WIDTH, itemCount * ITEM_HEIGHT);
        assertEquals(itemCount, mParent.getChildCount());
    }

    /** Overrides the position of the laid out child at {@code childIndex}. */
    private View setUpChild(int top, int bottom, float translationY, int childIndex) {
        View view = mParent.getChildAt(childIndex);
        view.layout(0, top, 0, bottom);
        view.setTranslationY(translationY);
        return view;
    }

    /** Creates a plain, fixed-height item view for each entry in {@link #mModel}. */
    private class ItemAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
        @Override
        public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            View view = new View(parent.getContext());
            view.setLayoutParams(
                    new RecyclerView.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT, ITEM_HEIGHT));
            return new RecyclerView.ViewHolder(view) {};
        }

        @Override
        public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {}

        @Override
        public int getItemCount() {
            return mModel.size();
        }
    }
}
