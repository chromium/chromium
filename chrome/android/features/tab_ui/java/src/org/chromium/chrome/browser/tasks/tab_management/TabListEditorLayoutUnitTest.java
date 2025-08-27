// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link TabListEditorLayout}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabListEditorLayoutUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ViewGroup mRootView;
    @Mock private TabListRecyclerView mRecyclerView;
    @Mock private RecyclerView.Adapter mAdapter;
    @Mock private SelectionDelegate<TabListEditorItemSelectionId> mSelectionDelegate;
    @Mock private View mChildView;
    @Mock private ViewGroup mChildViewGroup;

    private Context mActivity;
    private TabListEditorLayout mTabListEditorLayout;
    private ViewGroup mParentView;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        mParentView = spy(new FrameLayout(mActivity, null));
        mTabListEditorLayout =
                spy(
                        (TabListEditorLayout)
                                LayoutInflater.from(mActivity)
                                        .inflate(
                                                R.layout.tab_list_editor_layout,
                                                mParentView,
                                                false));
    }

    private void initializeLayout() {
        mTabListEditorLayout.initialize(
                mRootView, mParentView, mRecyclerView, mAdapter, mSelectionDelegate);
        when(mRootView.indexOfChild(mTabListEditorLayout)).thenReturn(-1);
    }

    @Test
    public void testInitialize() {
        initializeLayout();
        verify(mTabListEditorLayout).initializeRecyclerView(mAdapter, mRecyclerView);
    }

    @Test
    public void testDestroy() {
        initializeLayout();
        mTabListEditorLayout.destroy();

        verify(mRecyclerView).setOnHierarchyChangeListener(null);
    }

    @Test
    public void testShow() {
        initializeLayout();
        when(mRootView.getChildCount()).thenReturn(0);

        mTabListEditorLayout.show();

        verify(mParentView).addView(mTabListEditorLayout);

        verify(mRecyclerView)
                .setOnHierarchyChangeListener(any(ViewGroup.OnHierarchyChangeListener.class));
    }

    @Test(expected = AssertionError.class)
    public void testShow_notInitialized() {
        mTabListEditorLayout.show();
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testShowAndHide_DescendantFocusability() {
        initializeLayout();
        when(mRootView.getChildCount()).thenReturn(2);
        when(mRootView.getChildAt(0)).thenReturn(mChildView);
        when(mRootView.getChildAt(1)).thenReturn(mChildViewGroup);

        when(mRootView.getDescendantFocusability()).thenReturn(ViewGroup.FOCUS_BEFORE_DESCENDANTS);
        when(mChildViewGroup.getDescendantFocusability())
                .thenReturn(ViewGroup.FOCUS_BEFORE_DESCENDANTS);

        mTabListEditorLayout.show();

        verify(mParentView).addView(mTabListEditorLayout);
        verify(mChildViewGroup).setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);
        verify(mRootView, never()).setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);

        verify(mRecyclerView)
                .setOnHierarchyChangeListener(any(ViewGroup.OnHierarchyChangeListener.class));

        mTabListEditorLayout.hide();
        verify(mParentView).removeView(mTabListEditorLayout);
        verify(mChildViewGroup).setDescendantFocusability(ViewGroup.FOCUS_BEFORE_DESCENDANTS);
        verify(mRootView, never()).setDescendantFocusability(ViewGroup.FOCUS_BEFORE_DESCENDANTS);
        assertEquals(ViewGroup.FOCUS_BEFORE_DESCENDANTS, mRootView.getDescendantFocusability());
    }

    @Test
    public void testShowAndHide_Accessibility() {
        initializeLayout();
        when(mRootView.getChildCount()).thenReturn(2);
        when(mRootView.getChildAt(0)).thenReturn(mChildView);
        when(mRootView.getChildAt(1)).thenReturn(mChildViewGroup);

        when(mRootView.getImportantForAccessibility())
                .thenReturn(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
        when(mChildView.getImportantForAccessibility())
                .thenReturn(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
        when(mChildViewGroup.getImportantForAccessibility())
                .thenReturn(View.IMPORTANT_FOR_ACCESSIBILITY_YES);

        mTabListEditorLayout.show();

        verify(mParentView).addView(mTabListEditorLayout);
        verify(mChildView)
                .setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        verify(mChildViewGroup)
                .setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        verify(mRootView).setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);

        verify(mRecyclerView)
                .setOnHierarchyChangeListener(any(ViewGroup.OnHierarchyChangeListener.class));

        mTabListEditorLayout.hide();
        verify(mParentView).removeView(mTabListEditorLayout);
        verify(mChildView).setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
        verify(mChildViewGroup).setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
        verify(mRootView).setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
    }

    @Test
    public void testOverrideContentDescriptions() {
        initializeLayout();

        int containerDescResId = R.string.accessibility_archived_tabs_dialog;
        int backButtonDescResId = R.string.accessibility_archived_tabs_dialog_back_button;

        String expectedContainerDesc = mActivity.getString(containerDescResId);
        mTabListEditorLayout.overrideContentDescriptions(containerDescResId, backButtonDescResId);

        assertEquals(expectedContainerDesc, mTabListEditorLayout.getContentDescription());
    }
}
