// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.drawable.ColorDrawable;
import android.view.DragEvent;
import android.view.InputDevice;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.core.widget.ImageViewCompat;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link VerticalTabRailLayout} and {@link VerticalTabListViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class VerticalTabRailLayoutUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<Integer> mMockHoverListener;
    @Mock private View.OnClickListener mGridClickListener;
    @Mock private View.OnClickListener mSearchClickListener;
    @Mock private View.OnClickListener mNewTabClickListener;
    @Mock private View.OnClickListener mCollapseClickListener;

    private Activity mActivity;
    private VerticalTabRailLayout mRailLayout;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(false);

        mRailLayout =
                (VerticalTabRailLayout)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.vertical_tab_layout, null, false);
        mRailLayout.setExpandOrCollapseOnHoverListener(mMockHoverListener);

        mModel =
                new PropertyModel.Builder(VerticalTabListProperties.ALL_KEYS)
                        .with(
                                VerticalTabListProperties.EXPAND_OR_COLLAPSE_ON_HOVER_LISTENER,
                                mMockHoverListener)
                        .with(VerticalTabListProperties.ON_GRID_CLICK_LISTENER, mGridClickListener)
                        .with(
                                VerticalTabListProperties.ON_SEARCH_CLICK_LISTENER,
                                mSearchClickListener)
                        .with(
                                VerticalTabListProperties.ON_NEW_TAB_CLICK_LISTENER,
                                mNewTabClickListener)
                        .with(
                                VerticalTabListProperties.ON_COLLAPSE_CLICK_LISTENER,
                                mCollapseClickListener)
                        .with(VerticalTabListProperties.IS_COLLAPSE_BUTTON_ENABLED, true)
                        .with(VerticalTabListProperties.COLLAPSE_STATE, RailCollapseState.EXPANDED)
                        .with(VerticalTabListProperties.IS_INCOGNITO, false)
                        .build();
    }

    @After
    public void tearDown() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(null);
    }

    @Test
    @SmallTest
    public void testChildViewInflation() {
        assertNotNull(mRailLayout.getRecyclerView());
        assertNotNull(mRailLayout.getPinnedTabsRecyclerView());
        assertNotNull(mRailLayout.getHeaderContainer());
    }

    @Test
    @SmallTest
    public void testHeaderAndNewTabButtonTooltips() {
        View gridButton = mRailLayout.findViewById(R.id.grid_button);
        assertNotNull(gridButton);
        assertEquals(
                mRailLayout.getContext().getString(R.string.accessibility_tab_groups),
                gridButton.getTooltipText());

        View searchButton = mRailLayout.findViewById(R.id.tab_search_button);
        assertNotNull(searchButton);
        assertEquals(
                mRailLayout
                        .getContext()
                        .getString(R.string.accessibility_search_loupe_tooltip_text),
                searchButton.getTooltipText());

        View newTabButton = mRailLayout.findViewById(R.id.new_tab_button);
        assertNotNull(newTabButton);
        assertEquals(
                mRailLayout.getContext().getString(R.string.accessibility_toolbar_btn_new_tab),
                newTabButton.getTooltipText());
    }

    @Test
    @SmallTest
    public void testSetCollapseState_ExpandedAndCollapsed() {
        mRailLayout.setCollapseState(RailCollapseState.EXPANDED);
        LinearLayout header = mRailLayout.getHeaderContainer();
        assertEquals(LinearLayout.HORIZONTAL, header.getOrientation());
        View spacer = mRailLayout.findViewById(R.id.header_spacer);
        assertEquals(View.VISIBLE, spacer.getVisibility());

        mRailLayout.setCollapseState(RailCollapseState.COLLAPSED);
        assertEquals(LinearLayout.VERTICAL, header.getOrientation());
        assertEquals(View.GONE, spacer.getVisibility());
    }

    @Test
    @SmallTest
    public void testSetDesktopWindowSpacerVisible() {
        View spacer = mRailLayout.findViewById(R.id.desktop_window_spacer);
        mRailLayout.setDesktopWindowSpacerVisible(true);
        assertEquals(View.VISIBLE, spacer.getVisibility());

        mRailLayout.setDesktopWindowSpacerVisible(false);
        assertEquals(View.GONE, spacer.getVisibility());
    }

    @Test
    @SmallTest
    public void testDispatchGenericMotionEvent_HoverInsideAndOutside() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS, "expand_on_hover", true);
        mRailLayout.setCollapseState(RailCollapseState.COLLAPSED);
        mRailLayout.layout(0, 0, 200, 500);

        // Hover inside
        MotionEvent hoverEnter =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 50f, 50f, 0);
        hoverEnter.setSource(InputDevice.SOURCE_MOUSE);
        mRailLayout.dispatchGenericMotionEvent(hoverEnter);
        verify(mMockHoverListener).onResult(RailCollapseState.EXPANDED_FOR_HOVERING);

        // Hover outside
        mRailLayout.setCollapseState(RailCollapseState.EXPANDED_FOR_HOVERING);
        MotionEvent hoverExit =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_EXIT, 500f, 50f, 0);
        hoverExit.setSource(InputDevice.SOURCE_MOUSE);
        mRailLayout.dispatchGenericMotionEvent(hoverExit);
        verify(mMockHoverListener).onResult(RailCollapseState.COLLAPSED);
    }

    @Test
    public void testDispatchGenericMotionEvent_consumesMouseButtonEvent() {
        mRailLayout.setCollapseState(RailCollapseState.COLLAPSED);
        mRailLayout.layout(0, 0, 200, 500);

        MotionEvent pressEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_BUTTON_PRESS, 50f, 50f, 0);
        pressEvent.setSource(InputDevice.SOURCE_MOUSE);
        assertTrue(mRailLayout.dispatchGenericMotionEvent(pressEvent));

        MotionEvent releaseEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_BUTTON_RELEASE, 50f, 50f, 0);
        releaseEvent.setSource(InputDevice.SOURCE_MOUSE);
        assertTrue(mRailLayout.dispatchGenericMotionEvent(releaseEvent));

        MotionEvent otherEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_MOVE, 50f, 50f, 0);
        otherEvent.setSource(InputDevice.SOURCE_MOUSE);
        assertFalse(mRailLayout.dispatchGenericMotionEvent(otherEvent));
    }

    @Test
    public void testOnWindowFocusChanged_CollapsesRailOnFocusLost() {
        mRailLayout.onWindowFocusChanged(false);
        verify(mMockHoverListener).onResult(RailCollapseState.COLLAPSED);
    }

    @Test
    public void testOnDragEvent_CollapsesRailOnDragExitedOrEnded() {
        DragEvent exitEvent = mock(DragEvent.class);
        when(exitEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_EXITED);
        mRailLayout.onDragEvent(exitEvent);
        verify(mMockHoverListener).onResult(RailCollapseState.COLLAPSED);

        DragEvent endEvent = mock(DragEvent.class);
        when(endEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_ENDED);
        mRailLayout.onDragEvent(endEvent);
        verify(mMockHoverListener, times(2)).onResult(RailCollapseState.COLLAPSED);
    }

    @Test
    @SmallTest
    public void testBindClickListeners() {
        VerticalTabListViewBinder.bind(
                mModel, mRailLayout, VerticalTabListProperties.ON_GRID_CLICK_LISTENER);
        View gridButton = mRailLayout.findViewById(R.id.grid_button);
        gridButton.performClick();
        verify(mGridClickListener).onClick(gridButton);

        VerticalTabListViewBinder.bind(
                mModel, mRailLayout, VerticalTabListProperties.ON_SEARCH_CLICK_LISTENER);
        View searchButton = mRailLayout.findViewById(R.id.tab_search_button);
        searchButton.performClick();
        verify(mSearchClickListener).onClick(searchButton);

        VerticalTabListViewBinder.bind(
                mModel, mRailLayout, VerticalTabListProperties.ON_NEW_TAB_CLICK_LISTENER);
        View newTabButton = mRailLayout.findViewById(R.id.new_tab_button);
        newTabButton.performClick();
        verify(mNewTabClickListener).onClick(newTabButton);

        VerticalTabListViewBinder.bind(
                mModel, mRailLayout, VerticalTabListProperties.ON_COLLAPSE_CLICK_LISTENER);
        View collapseButton = mRailLayout.findViewById(R.id.collapse_button);
        collapseButton.performClick();
        verify(mCollapseClickListener).onClick(collapseButton);
    }

    @Test
    @SmallTest
    public void testBindCollapseButtonEnabled() {
        View collapseButton = mRailLayout.findViewById(R.id.collapse_button);

        mModel.set(VerticalTabListProperties.IS_COLLAPSE_BUTTON_ENABLED, false);
        VerticalTabListViewBinder.bind(
                mModel, mRailLayout, VerticalTabListProperties.IS_COLLAPSE_BUTTON_ENABLED);
        assertFalse(collapseButton.isEnabled());
        assertEquals(0.38f, collapseButton.getAlpha(), 0.01f);

        mModel.set(VerticalTabListProperties.IS_COLLAPSE_BUTTON_ENABLED, true);
        VerticalTabListViewBinder.bind(
                mModel, mRailLayout, VerticalTabListProperties.IS_COLLAPSE_BUTTON_ENABLED);
        assertTrue(collapseButton.isEnabled());
        assertEquals(1.0f, collapseButton.getAlpha(), 0.01f);
    }

    @Test
    @SmallTest
    public void testBindCollapseState() {
        mModel.set(VerticalTabListProperties.COLLAPSE_STATE, RailCollapseState.COLLAPSED);
        VerticalTabListViewBinder.bind(
                mModel, mRailLayout, VerticalTabListProperties.COLLAPSE_STATE);
        assertEquals(View.GONE, mRailLayout.findViewById(R.id.header_spacer).getVisibility());

        mModel.set(VerticalTabListProperties.COLLAPSE_STATE, RailCollapseState.EXPANDED);
        VerticalTabListViewBinder.bind(
                mModel, mRailLayout, VerticalTabListProperties.COLLAPSE_STATE);
        assertEquals(View.VISIBLE, mRailLayout.findViewById(R.id.header_spacer).getVisibility());
    }

    @Test
    @SmallTest
    public void testBindIncognitoColors_Regular() {
        mModel.set(VerticalTabListProperties.IS_INCOGNITO, false);
        VerticalTabListViewBinder.bind(mModel, mRailLayout, VerticalTabListProperties.IS_INCOGNITO);

        ColorDrawable bg = (ColorDrawable) mRailLayout.getBackground();
        assertNotNull(bg);
        assertEquals(
                TabUiThemeUtil.getTabStripBackgroundColor(mActivity, /* isIncognito= */ false),
                bg.getColor());

        ImageView collapseButton = mRailLayout.findViewById(R.id.collapse_button);
        ColorStateList iconTint = ImageViewCompat.getImageTintList(collapseButton);
        assertNotNull(iconTint);
        assertEquals(SemanticColorUtils.getDefaultIconColor(mActivity), iconTint.getDefaultColor());

        View gridButton = mRailLayout.findViewById(R.id.grid_button);
        assertNull(gridButton.getBackgroundTintList());
    }

    @Test
    @SmallTest
    public void testBindIncognitoColors_Incognito() {
        mModel.set(VerticalTabListProperties.IS_INCOGNITO, true);
        VerticalTabListViewBinder.bind(mModel, mRailLayout, VerticalTabListProperties.IS_INCOGNITO);

        ColorDrawable bg = (ColorDrawable) mRailLayout.getBackground();
        assertNotNull(bg);
        assertEquals(
                TabUiThemeUtil.getTabStripBackgroundColor(mActivity, /* isIncognito= */ true),
                bg.getColor());

        ImageView collapseButton = mRailLayout.findViewById(R.id.collapse_button);
        ColorStateList iconTint = ImageViewCompat.getImageTintList(collapseButton);
        assertNotNull(iconTint);
        assertEquals(
                mActivity.getColor(R.color.incognito_tab_action_button_color),
                iconTint.getDefaultColor());

        View gridButton = mRailLayout.findViewById(R.id.grid_button);
        ColorStateList buttonBgTint = gridButton.getBackgroundTintList();
        assertNotNull(buttonBgTint);
        assertEquals(
                mActivity.getColor(R.color.gm3_baseline_surface_container_dark),
                buttonBgTint.getDefaultColor());
        assertEquals(
                mActivity.getColor(R.color.gm3_baseline_surface_container_high_dark),
                buttonBgTint.getColorForState(
                        new int[] {android.R.attr.state_hovered}, buttonBgTint.getDefaultColor()));
        assertEquals(
                mActivity.getColor(R.color.gm3_baseline_surface_container_high_dark),
                buttonBgTint.getColorForState(
                        new int[] {android.R.attr.state_pressed}, buttonBgTint.getDefaultColor()));
    }

    @Test
    @SmallTest
    public void testBindIncognitoColors_WhenShouldOpenIncognitoAsWindow() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);

        mRailLayout.setBackground(null);
        mModel.set(VerticalTabListProperties.IS_INCOGNITO, true);
        VerticalTabListViewBinder.bind(mModel, mRailLayout, VerticalTabListProperties.IS_INCOGNITO);

        // When shouldOpenIncognitoAsWindow is true, updateIncognitoColors returns early without
        // overriding background or tints.
        assertNull(mRailLayout.getBackground());
    }
}
