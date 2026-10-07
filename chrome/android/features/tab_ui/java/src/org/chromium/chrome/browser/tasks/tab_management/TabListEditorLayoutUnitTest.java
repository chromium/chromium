// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.VectorDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.core.widget.ImageViewCompat;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.GraphicsMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link TabListEditorLayout}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabListEditorLayoutUnitTest {
    /** An empty adapter. */
    private static class TestAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
        @Override
        public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            return new RecyclerView.ViewHolder(new View(parent.getContext())) {};
        }

        @Override
        public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {}

        @Override
        public int getItemCount() {
            return 0;
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private SelectionDelegate<TabListEditorItemSelectionId> mSelectionDelegate;

    private Context mActivity;
    private TabListEditorLayout mTabListEditorLayout;
    private ViewGroup mRootView;
    private ViewGroup mParentView;
    private View mChildView;
    private ViewGroup mChildViewGroup;
    private TabListRecyclerView mRecyclerView;
    private RecyclerView.Adapter mAdapter;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        mRootView = new FrameLayout(mActivity);
        mChildView = new View(mActivity);
        mChildViewGroup = new FrameLayout(mActivity);
        mRootView.addView(mChildView);
        mRootView.addView(mChildViewGroup);
        mParentView = new FrameLayout(mActivity);
        mRecyclerView = new TabListRecyclerView(mActivity, null);
        mAdapter = new TestAdapter();
        mTabListEditorLayout =
                (TabListEditorLayout)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.tab_list_editor_layout, mParentView, false);
    }

    private void initializeLayout() {
        mTabListEditorLayout.initialize(
                mRootView, mParentView, mRecyclerView, mAdapter, mSelectionDelegate);
    }

    @Test
    public void testInitialize() {
        initializeLayout();
        assertEquals(mAdapter, mRecyclerView.getAdapter());
        assertEquals(
                mTabListEditorLayout.findViewById(R.id.list_content), mRecyclerView.getParent());
    }

    @Test
    public void testDestroy() {
        initializeLayout();
        mTabListEditorLayout.show();
        mTabListEditorLayout.destroy();

        assertNull(mRecyclerView.getAdapter());
        verify(mSelectionDelegate, atLeastOnce()).removeObserver(any());
    }

    @Test
    public void testShow() {
        mRootView.removeAllViews();
        initializeLayout();

        mTabListEditorLayout.show();

        assertEquals(mParentView, mTabListEditorLayout.getParent());
    }

    @Test(expected = AssertionError.class)
    public void testShow_notInitialized() {
        mTabListEditorLayout.show();
    }

    @Test
    public void testShowAndHide_DescendantFocusability() {
        initializeLayout();
        mRootView.setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);
        mChildViewGroup.setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);

        mTabListEditorLayout.show();

        assertEquals(mParentView, mTabListEditorLayout.getParent());
        assertEquals(
                ViewGroup.FOCUS_BLOCK_DESCENDANTS, mChildViewGroup.getDescendantFocusability());
        assertEquals(ViewGroup.FOCUS_AFTER_DESCENDANTS, mRootView.getDescendantFocusability());

        mTabListEditorLayout.hide();
        assertNull(mTabListEditorLayout.getParent());
        assertEquals(
                ViewGroup.FOCUS_AFTER_DESCENDANTS, mChildViewGroup.getDescendantFocusability());
        assertEquals(ViewGroup.FOCUS_AFTER_DESCENDANTS, mRootView.getDescendantFocusability());
    }

    @Test
    public void testShowAndHide_Accessibility() {
        initializeLayout();
        mRootView.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
        mChildView.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
        mChildViewGroup.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);

        mTabListEditorLayout.show();

        assertEquals(mParentView, mTabListEditorLayout.getParent());
        assertEquals(
                View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS,
                mChildView.getImportantForAccessibility());
        assertEquals(
                View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS,
                mChildViewGroup.getImportantForAccessibility());
        assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_NO, mRootView.getImportantForAccessibility());

        mTabListEditorLayout.hide();
        assertNull(mTabListEditorLayout.getParent());
        assertEquals(
                View.IMPORTANT_FOR_ACCESSIBILITY_YES, mChildView.getImportantForAccessibility());
        assertEquals(
                View.IMPORTANT_FOR_ACCESSIBILITY_YES,
                mChildViewGroup.getImportantForAccessibility());
        assertEquals(
                View.IMPORTANT_FOR_ACCESSIBILITY_YES, mRootView.getImportantForAccessibility());
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

    @Test
    @GraphicsMode(GraphicsMode.Mode.NATIVE)
    public void testToolbarNavigationIcon_VectorDrawableAndRtlMirroring() {
        initializeLayout();
        TabListEditorToolbar toolbar = mTabListEditorLayout.getToolbar();
        Drawable navIcon = toolbar.getNavigationIcon();
        assertNotNull(navIcon);
        assertTrue(navIcon instanceof LayerDrawable);

        LayerDrawable layerDrawable = (LayerDrawable) navIcon;
        assertEquals(2, layerDrawable.getNumberOfLayers());

        Drawable iconLayer = layerDrawable.getDrawable(1);
        assertNotNull(iconLayer);
        assertTrue("Icon layer must be a VectorDrawable", iconLayer instanceof VectorDrawable);
        assertTrue(iconLayer.isAutoMirrored());

        ColorStateList tint = ColorStateList.valueOf(Color.RED);
        toolbar.setButtonTint(tint);
        assertEquals(
                tint,
                ImageViewCompat.getImageTintList(toolbar.findViewById(R.id.list_menu_button)));

        toolbar.setIsIncognito(true);
        LayerDrawable incognitoNavIcon = (LayerDrawable) toolbar.getNavigationIcon();
        assertNotNull(incognitoNavIcon);
        assertSame(iconLayer, incognitoNavIcon.getDrawable(1));
        assertTrue(incognitoNavIcon.getDrawable(1).isAutoMirrored());

        int inset =
                toolbar.getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.search_box_nav_button_background_inset);
        int width = inset * 4;
        int height = inset * 4;
        incognitoNavIcon.setBounds(0, 0, width, height);
        incognitoNavIcon.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);

        assertEquals(View.LAYOUT_DIRECTION_RTL, iconLayer.getLayoutDirection());
        assertEquals(new Rect(inset, inset, width - inset, height - inset), iconLayer.getBounds());

        Bitmap rtlBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas rtlCanvas = new Canvas(rtlBitmap);
        incognitoNavIcon.getDrawable(0).setAlpha(0);
        incognitoNavIcon.draw(rtlCanvas);

        boolean hasRenderedIconPixels = false;
        for (int y = inset; y < height - inset; y++) {
            for (int x = inset; x < width - inset; x++) {
                if (rtlBitmap.getPixel(x, y) != 0) {
                    hasRenderedIconPixels = true;
                    break;
                }
            }
            if (hasRenderedIconPixels) {
                break;
            }
        }

        assertTrue(
                "VectorDrawable should render non-zero pixels under native graphics",
                hasRenderedIconPixels);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (x < inset || x >= width - inset || y < inset || y >= height - inset) {
                    assertEquals(0, rtlBitmap.getPixel(x, y));
                }
            }
        }
    }
}
