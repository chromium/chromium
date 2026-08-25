// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_COLLAPSE;
import static android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_DISMISS;
import static android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_EXPAND;
import static android.view.accessibility.AccessibilityNodeInfo.EXPANDED_STATE_COLLAPSED;
import static android.view.accessibility.AccessibilityNodeInfo.EXPANDED_STATE_FULL;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;
import android.widget.ExpandableListView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link RecentTabsGroupView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class RecentTabsGroupViewUnitTest {
    private Context mContext;
    private RecentTabsGroupView mGroupView;

    private static class TestExpandableListView extends ExpandableListView {
        private final View mChild;
        private final int mPosition;
        private final long mPackedPos;
        private boolean mExpandCalled;
        private boolean mCollapseCalled;
        private int mLastGroupPos = -1;

        TestExpandableListView(Context context, View child, int position, long packedPos) {
            super(context);
            mChild = child;
            mPosition = position;
            mPackedPos = packedPos;
        }

        void attachChild() {
            addViewInLayout(mChild, 0, new ViewGroup.LayoutParams(100, 100));
        }

        @Override
        public int getPositionForView(View view) {
            if (view == mChild) {
                return mPosition;
            }
            return INVALID_POSITION;
        }

        @Override
        public long getExpandableListPosition(int flatListPosition) {
            if (flatListPosition == mPosition) {
                return mPackedPos;
            }
            return PACKED_POSITION_VALUE_NULL;
        }

        @Override
        public boolean expandGroup(int groupPos) {
            mExpandCalled = true;
            mLastGroupPos = groupPos;
            return true;
        }

        @Override
        public boolean collapseGroup(int groupPos) {
            mCollapseCalled = true;
            mLastGroupPos = groupPos;
            return true;
        }
    }

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mGroupView =
                (RecentTabsGroupView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.recent_tabs_group_item, /* root= */ null);
    }

    private ForeignSession createForeignSession() {
        List<ForeignSessionTab> tabs = new ArrayList<>();
        tabs.add(new ForeignSessionTab(new GURL("https://google.com"), "Google", 0L, 0L, 1));
        List<ForeignSessionWindow> windows = new ArrayList<>();
        windows.add(new ForeignSessionWindow(0L, 1, tabs));
        return new ForeignSession("session_tag", "Session Name", 0L, windows, 0);
    }

    @Test
    public void testOnInitializeAccessibilityNodeInfo_DefaultCollapsed() {
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();
        mGroupView.onInitializeAccessibilityNodeInfo(info);

        List<AccessibilityAction> actionList = info.getActionList();
        assertTrue(actionList.contains(ACTION_EXPAND));
        assertFalse(actionList.contains(ACTION_COLLAPSE));
    }

    @Test
    public void testOnInitializeAccessibilityNodeInfo_RecentlyClosedCollapsed() {
        mGroupView.configureForRecentlyClosedTabs(/* isExpanded= */ false);

        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();
        mGroupView.onInitializeAccessibilityNodeInfo(info);

        List<AccessibilityAction> actionList = info.getActionList();
        assertTrue(actionList.contains(ACTION_EXPAND));
        assertFalse(actionList.contains(ACTION_COLLAPSE));
    }

    @Test
    public void testOnInitializeAccessibilityNodeInfo_RecentlyClosedExpanded() {
        mGroupView.configureForRecentlyClosedTabs(/* isExpanded= */ true);

        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();
        mGroupView.onInitializeAccessibilityNodeInfo(info);

        List<AccessibilityAction> actionList = info.getActionList();
        assertTrue(actionList.contains(ACTION_COLLAPSE));
        assertFalse(actionList.contains(ACTION_EXPAND));
    }

    @Test
    public void testOnInitializeAccessibilityNodeInfo_ForeignSessionExpanded() {
        mGroupView.configureForForeignSession(createForeignSession(), /* isExpanded= */ true);

        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();
        mGroupView.onInitializeAccessibilityNodeInfo(info);

        List<AccessibilityAction> actionList = info.getActionList();
        assertTrue(actionList.contains(ACTION_COLLAPSE));
        assertFalse(actionList.contains(ACTION_EXPAND));
    }

    @Test
    public void testOnInitializeAccessibilityNodeInfo_PromoExpanded() {
        mGroupView.configureForPromo(/* isExpanded= */ true);

        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();
        mGroupView.onInitializeAccessibilityNodeInfo(info);

        List<AccessibilityAction> actionList = info.getActionList();
        assertTrue(actionList.contains(ACTION_COLLAPSE));
        assertFalse(actionList.contains(ACTION_EXPAND));
    }

    @Test
    @Config(sdk = 36)
    public void testOnInitializeAccessibilityNodeInfo_Sdk36() {
        mGroupView.configureForRecentlyClosedTabs(/* isExpanded= */ false);
        AccessibilityNodeInfo infoCollapsed = AccessibilityNodeInfo.obtain();
        mGroupView.onInitializeAccessibilityNodeInfo(infoCollapsed);
        assertEquals(EXPANDED_STATE_COLLAPSED, infoCollapsed.getExpandedState());

        mGroupView.configureForRecentlyClosedTabs(/* isExpanded= */ true);
        AccessibilityNodeInfo infoExpanded = AccessibilityNodeInfo.obtain();
        mGroupView.onInitializeAccessibilityNodeInfo(infoExpanded);
        assertEquals(EXPANDED_STATE_FULL, infoExpanded.getExpandedState());
    }

    @Test
    public void testPerformAccessibilityAction_WithOnClickListener() {
        AtomicBoolean clicked = new AtomicBoolean(false);
        mGroupView.setOnClickListener(v -> clicked.set(true));

        boolean handled =
                mGroupView.performAccessibilityAction(ACTION_EXPAND.getId(), /* arguments= */ null);
        assertTrue(handled);
        assertTrue(clicked.get());

        clicked.set(false);
        handled =
                mGroupView.performAccessibilityAction(
                        ACTION_COLLAPSE.getId(), /* arguments= */ null);
        assertTrue(handled);
        assertTrue(clicked.get());
    }

    @Test
    public void testPerformAccessibilityAction_ExpandableListView_Expand() {
        mGroupView.configureForRecentlyClosedTabs(/* isExpanded= */ false);

        int flatPosition = 2;
        int groupPosition = 1;
        long packedPosition = ExpandableListView.getPackedPositionForGroup(groupPosition);

        TestExpandableListView parent =
                new TestExpandableListView(mContext, mGroupView, flatPosition, packedPosition);
        parent.attachChild();

        boolean handled =
                mGroupView.performAccessibilityAction(ACTION_EXPAND.getId(), /* arguments= */ null);
        assertTrue(handled);
        assertTrue(parent.mExpandCalled);
        assertFalse(parent.mCollapseCalled);
        assertEquals(groupPosition, parent.mLastGroupPos);
    }

    @Test
    public void testPerformAccessibilityAction_ExpandableListView_ExpandWhenAlreadyExpanded() {
        mGroupView.configureForRecentlyClosedTabs(/* isExpanded= */ true);

        int flatPosition = 2;
        int groupPosition = 1;
        long packedPosition = ExpandableListView.getPackedPositionForGroup(groupPosition);

        TestExpandableListView parent =
                new TestExpandableListView(mContext, mGroupView, flatPosition, packedPosition);
        parent.attachChild();

        boolean handled =
                mGroupView.performAccessibilityAction(ACTION_EXPAND.getId(), /* arguments= */ null);
        assertTrue(handled);
        assertTrue(parent.mExpandCalled);
        assertFalse(parent.mCollapseCalled);
        assertEquals(groupPosition, parent.mLastGroupPos);
    }

    @Test
    public void testPerformAccessibilityAction_ExpandableListView_Collapse() {
        mGroupView.configureForRecentlyClosedTabs(/* isExpanded= */ true);

        int flatPosition = 2;
        int groupPosition = 1;
        long packedPosition = ExpandableListView.getPackedPositionForGroup(groupPosition);

        TestExpandableListView parent =
                new TestExpandableListView(mContext, mGroupView, flatPosition, packedPosition);
        parent.attachChild();

        boolean handled =
                mGroupView.performAccessibilityAction(
                        ACTION_COLLAPSE.getId(), /* arguments= */ null);
        assertTrue(handled);
        assertTrue(parent.mCollapseCalled);
        assertFalse(parent.mExpandCalled);
        assertEquals(groupPosition, parent.mLastGroupPos);
    }

    @Test
    public void testPerformAccessibilityAction_ExpandableListView_CollapseWhenAlreadyCollapsed() {
        mGroupView.configureForRecentlyClosedTabs(/* isExpanded= */ false);

        int flatPosition = 2;
        int groupPosition = 1;
        long packedPosition = ExpandableListView.getPackedPositionForGroup(groupPosition);

        TestExpandableListView parent =
                new TestExpandableListView(mContext, mGroupView, flatPosition, packedPosition);
        parent.attachChild();

        boolean handled =
                mGroupView.performAccessibilityAction(
                        ACTION_COLLAPSE.getId(), /* arguments= */ null);
        assertTrue(handled);
        assertTrue(parent.mCollapseCalled);
        assertFalse(parent.mExpandCalled);
        assertEquals(groupPosition, parent.mLastGroupPos);
    }

    @Test
    public void testPerformAccessibilityAction_ExpandableListView_PackedPosNullFallback() {
        mGroupView.configureForRecentlyClosedTabs(/* isExpanded= */ false);

        int flatPosition = 2;
        TestExpandableListView parent =
                new TestExpandableListView(
                        mContext,
                        mGroupView,
                        flatPosition,
                        ExpandableListView.PACKED_POSITION_VALUE_NULL);
        parent.attachChild();

        AtomicBoolean clicked = new AtomicBoolean(false);
        mGroupView.setOnClickListener(v -> clicked.set(true));

        boolean handled =
                mGroupView.performAccessibilityAction(ACTION_EXPAND.getId(), /* arguments= */ null);
        assertTrue(handled);
        assertTrue(clicked.get());
        assertFalse(parent.mExpandCalled);
        assertFalse(parent.mCollapseCalled);
        assertEquals(-1, parent.mLastGroupPos);
    }

    @Test
    public void testPerformAccessibilityAction_ExpandableListView_ChildPositionTypeFallback() {
        mGroupView.configureForRecentlyClosedTabs(/* isExpanded= */ false);

        int flatPosition = 2;
        long packedChildPosition =
                ExpandableListView.getPackedPositionForChild(
                        /* groupPosition= */ 1, /* childPosition= */ 0);

        TestExpandableListView parent =
                new TestExpandableListView(mContext, mGroupView, flatPosition, packedChildPosition);
        parent.attachChild();

        AtomicBoolean clicked = new AtomicBoolean(false);
        mGroupView.setOnClickListener(v -> clicked.set(true));

        boolean handled =
                mGroupView.performAccessibilityAction(
                        ACTION_COLLAPSE.getId(), /* arguments= */ null);
        assertTrue(handled);
        assertTrue(clicked.get());
        assertFalse(parent.mExpandCalled);
        assertFalse(parent.mCollapseCalled);
        assertEquals(-1, parent.mLastGroupPos);
    }

    @Test
    public void testPerformAccessibilityAction_ExpandableListView_InvalidPositionFallback() {
        mGroupView.configureForRecentlyClosedTabs(/* isExpanded= */ false);

        TestExpandableListView parent =
                new TestExpandableListView(
                        mContext,
                        mGroupView,
                        ExpandableListView.INVALID_POSITION,
                        ExpandableListView.PACKED_POSITION_VALUE_NULL);
        parent.attachChild();

        // When invalid position, falls through to performClick(), which returns false if no
        // listener.
        boolean handled =
                mGroupView.performAccessibilityAction(ACTION_EXPAND.getId(), /* arguments= */ null);
        assertFalse(handled);
        assertFalse(parent.mExpandCalled);
        assertFalse(parent.mCollapseCalled);
        assertEquals(-1, parent.mLastGroupPos);
    }

    @Test
    public void testPerformAccessibilityAction_UnhandledActionDelegatesToSuper() {
        boolean handled =
                mGroupView.performAccessibilityAction(
                        ACTION_DISMISS.getId(), /* arguments= */ null);
        assertFalse(handled);
    }
}
