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
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.graphics.drawable.ColorDrawable;
import android.view.DragEvent;
import android.view.Gravity;
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

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link VerticalTabRailLayout} and {@link VerticalTabListViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabRailLayoutUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<Integer> mMockHoverListener;
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
        Configuration config = mActivity.getResources().getConfiguration();
        config.smallestScreenWidthDp = 600;
        mActivity
                .getResources()
                .updateConfiguration(config, mActivity.getResources().getDisplayMetrics());
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
                        .with(
                                VerticalTabListProperties.ON_SEARCH_CLICK_LISTENER,
                                mSearchClickListener)
                        .with(
                                VerticalTabListProperties.ON_NEW_TAB_CLICK_LISTENER,
                                mNewTabClickListener)
                        .with(VerticalTabListProperties.IS_INCOGNITO_BUTTON_VISIBLE, false)
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
        DeviceInfo.resetIsDesktopForTesting();
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(null);
    }

    @Test
    @SmallTest
    public void testChildViewInflation() {
        assertNotNull(mRailLayout.getRecyclerView());
        assertNotNull(mRailLayout.getPinnedTabsRecyclerView());
        assertNotNull(mRailLayout.getHeaderContainer());
        assertNotNull(mRailLayout.getFooterContainer());
        assertNotNull(mRailLayout.getIncognitoButton());
    }

    @Test
    @SmallTest
    public void testHeaderAndNewTabButtonTooltips() {
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

        View incognitoButton = mRailLayout.findViewById(R.id.new_incognito_tab_button);
        assertNotNull(incognitoButton);
        assertEquals(
                mRailLayout
                        .getContext()
                        .getString(R.string.accessibility_toolbar_btn_new_incognito_tab),
                incognitoButton.getTooltipText());
    }

    @Test
    @SmallTest
    public void testSetCollapseState_ExpandedAndCollapsed() {
        int minSingleRowRailWidthPx = mRailLayout.getMinSingleButtonRowWidthPxForTesting();

        LinearLayout header = mRailLayout.getHeaderContainer();
        View spacer = mRailLayout.findViewById(R.id.header_spacer);

        // 1. Expanded Wide (single row)
        mRailLayout.setCollapseState(RailCollapseState.EXPANDED);
        measureAndLayout(mRailLayout, minSingleRowRailWidthPx, 500);
        assertEquals(LinearLayout.HORIZONTAL, header.getOrientation());
        assertEquals(View.VISIBLE, spacer.getVisibility());

        // 2. Explicit Collapsed State (single column)
        mRailLayout.setCollapseState(RailCollapseState.COLLAPSED);
        assertEquals(LinearLayout.VERTICAL, header.getOrientation());
        assertEquals(View.GONE, spacer.getVisibility());

        View newTabButton = mRailLayout.findViewById(R.id.new_tab_button);
        assertEquals(
                minSingleRowRailWidthPx > 0
                        ? mRailLayout.findViewById(R.id.collapse_button).getLayoutParams().width
                        : 0,
                newTabButton.getLayoutParams().width);
        assertEquals(
                mRailLayout.findViewById(R.id.collapse_button).getLayoutParams().height,
                newTabButton.getLayoutParams().height);
        assertEquals(
                0.0f, ((LinearLayout.LayoutParams) newTabButton.getLayoutParams()).weight, 0.01f);
        assertEquals(
                Gravity.CENTER_HORIZONTAL,
                mRailLayout.getFooterContainer().getGravity() & Gravity.HORIZONTAL_GRAVITY_MASK);
    }

    @Test
    @SmallTest
    public void testOnMeasure_SkipsUpdateWhenHeaderModeUnchanged() {
        int minSingleRowRailWidthPx = mRailLayout.getMinSingleButtonRowWidthPxForTesting();
        int narrowRailWidthPx = minSingleRowRailWidthPx - 1;
        int buttonSize =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                VerticalTabUtils.isTablet(mActivity)
                                        ? R.dimen.vertical_tabs_header_button_size_tablet
                                        : R.dimen.vertical_tabs_header_button_size);

        View searchButton = mRailLayout.findViewById(R.id.tab_search_button);

        // 1. Initial size in single row mode.
        mRailLayout.setCollapseState(RailCollapseState.EXPANDED);
        measureAndLayout(mRailLayout, minSingleRowRailWidthPx + 100, 500);
        assertEquals(buttonSize, searchButton.getLayoutParams().width);

        // 2. Mutate a layout param to a custom value to verify it is not overwritten.
        var params = searchButton.getLayoutParams();
        params.width = 999;
        searchButton.setLayoutParams(params);

        // 3. Size change with a different wide width (still single row).
        measureAndLayout(mRailLayout, minSingleRowRailWidthPx + 50, 500);
        // Action is skipped because mode hasn't changed; width remains custom value 999.
        assertEquals(999, searchButton.getLayoutParams().width);

        // 4. Size change with narrow width (transitions to a vertical layout).
        measureAndLayout(mRailLayout, narrowRailWidthPx, 500);
        // Action is performed; width is reset.
        assertEquals(buttonSize, searchButton.getLayoutParams().width);
    }

    @Test
    @SmallTest
    public void testColdStart_UnmeasuredToNarrowLayout_TransitionsToVertical() {
        int minSingleRowRailWidthPx = mRailLayout.getMinSingleButtonRowWidthPxForTesting();
        int narrowRailWidthPx = minSingleRowRailWidthPx - 1;

        LinearLayout header = mRailLayout.getHeaderContainer();

        // Initial cold start inflate has width 0 (unmeasured), defaults to single row.
        assertEquals(LinearLayout.HORIZONTAL, header.getOrientation());

        // Measure with narrow width must configure vertical layout before layout.
        int widthSpec =
                View.MeasureSpec.makeMeasureSpec(narrowRailWidthPx, View.MeasureSpec.EXACTLY);
        int heightSpec = View.MeasureSpec.makeMeasureSpec(500, View.MeasureSpec.EXACTLY);
        mRailLayout.measure(widthSpec, heightSpec);
        assertEquals(LinearLayout.VERTICAL, header.getOrientation());

        // Layout pass at narrow width preserves vertical layout.
        measureAndLayout(mRailLayout, narrowRailWidthPx, 500);
        assertEquals(LinearLayout.VERTICAL, header.getOrientation());

        // Subsequent PropertyModel bind must not overwrite the vertical layout params.
        VerticalTabListViewBinder.bind(mModel, mRailLayout, VerticalTabListProperties.IS_INCOGNITO);
        assertEquals(LinearLayout.VERTICAL, header.getOrientation());
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
        measureAndLayout(mRailLayout, 200, 500);

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
        measureAndLayout(mRailLayout, 200, 500);

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
    public void testSearchButtonBackground() {
        View searchButton = mRailLayout.findViewById(R.id.tab_search_button);
        assertEquals(
                R.drawable.vertical_tabs_button_background,
                shadowOf(searchButton.getBackground()).getCreatedFromResId());
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

        View searchButton = mRailLayout.findViewById(R.id.tab_search_button);
        assertNull(searchButton.getBackgroundTintList());
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

        View searchButton = mRailLayout.findViewById(R.id.tab_search_button);
        ColorStateList buttonBgTint = searchButton.getBackgroundTintList();
        assertNotNull(buttonBgTint);
        assertEquals(
                mActivity.getColor(R.color.incognito_vertical_tabs_button_background_color),
                buttonBgTint.getDefaultColor());
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

    @Test
    @SmallTest
    public void testButtonDimensions_TabletVsNonTablet() {
        // Touch tablet device (default in setUp, loads values-sw600dp)
        int expectedTouchButtonSize =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tabs_header_button_size_tablet);
        int expectedTouchNewTabHeight =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tabs_footer_button_height_tablet);

        assertEquals(36, expectedTouchButtonSize);
        assertEquals(40, expectedTouchNewTabHeight);

        View collapseButton = mRailLayout.findViewById(R.id.collapse_button);
        assertEquals(expectedTouchButtonSize, collapseButton.getLayoutParams().width);
        assertEquals(expectedTouchButtonSize, collapseButton.getLayoutParams().height);

        View newTabButton = mRailLayout.findViewById(R.id.new_tab_button);
        assertEquals(expectedTouchNewTabHeight, newTabButton.getLayoutParams().height);

        // Non-tablet device (loads values)
        Configuration nonTabletConfig =
                new Configuration(mActivity.getResources().getConfiguration());
        nonTabletConfig.smallestScreenWidthDp = 320;
        android.content.Context nonTabletContext =
                mActivity.createConfigurationContext(nonTabletConfig);

        int expectedDefaultButtonSize =
                nonTabletContext
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tabs_header_button_size);
        int expectedDefaultNewTabHeight =
                nonTabletContext
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tabs_footer_button_height);

        assertEquals(32, expectedDefaultButtonSize);
        assertEquals(32, expectedDefaultNewTabHeight);
    }

    @Test
    @SmallTest
    public void testBindIncognitoButtonVisibilityAndLayout() {
        View incognitoButton = mRailLayout.findViewById(R.id.new_incognito_tab_button);
        View newTabButton = mRailLayout.findViewById(R.id.new_tab_button);
        LinearLayout footerContainer = mRailLayout.findViewById(R.id.vertical_tab_footer_container);
        assertNotNull(incognitoButton);
        assertNotNull(newTabButton);
        assertNotNull(footerContainer);

        // Initially gone
        assertEquals(View.GONE, incognitoButton.getVisibility());

        // Set visible
        mModel.set(VerticalTabListProperties.IS_INCOGNITO_BUTTON_VISIBLE, true);
        VerticalTabListViewBinder.bind(
                mModel, mRailLayout, VerticalTabListProperties.IS_INCOGNITO_BUTTON_VISIBLE);
        assertEquals(View.VISIBLE, incognitoButton.getVisibility());

        // In expanded state with incognito button visible
        mRailLayout.setCollapseState(RailCollapseState.EXPANDED);
        assertEquals(LinearLayout.HORIZONTAL, footerContainer.getOrientation());
        LinearLayout.LayoutParams newTabParams =
                (LinearLayout.LayoutParams) newTabButton.getLayoutParams();
        LinearLayout.LayoutParams incognitoParams =
                (LinearLayout.LayoutParams) incognitoButton.getLayoutParams();
        assertEquals(1.0f, newTabParams.weight, 0.01f);
        assertEquals(0, newTabParams.width);
        int expectedChipSize =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                VerticalTabUtils.isTablet(mActivity)
                                        ? R.dimen.vertical_tabs_footer_button_height_tablet
                                        : R.dimen.vertical_tabs_footer_button_height);
        assertEquals(expectedChipSize, incognitoParams.width);
        assertEquals(expectedChipSize, incognitoParams.height);

        // In collapsed state
        mRailLayout.setCollapseState(RailCollapseState.COLLAPSED);
        assertEquals(LinearLayout.VERTICAL, footerContainer.getOrientation());
        LinearLayout.LayoutParams collapsedNewTabParams =
                (LinearLayout.LayoutParams) newTabButton.getLayoutParams();
        LinearLayout.LayoutParams collapsedIncognitoParams =
                (LinearLayout.LayoutParams) incognitoButton.getLayoutParams();
        int expectedCollapsedSize =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                VerticalTabUtils.isTablet(mActivity)
                                        ? R.dimen.vertical_tabs_header_button_size_tablet
                                        : R.dimen.vertical_tabs_header_button_size);
        assertEquals(expectedCollapsedSize, collapsedNewTabParams.width);
        assertEquals(expectedCollapsedSize, collapsedNewTabParams.height);
        assertEquals(expectedCollapsedSize, collapsedIncognitoParams.width);
        assertEquals(expectedCollapsedSize, collapsedIncognitoParams.height);

        // Set gone again
        mModel.set(VerticalTabListProperties.IS_INCOGNITO_BUTTON_VISIBLE, false);
        VerticalTabListViewBinder.bind(
                mModel, mRailLayout, VerticalTabListProperties.IS_INCOGNITO_BUTTON_VISIBLE);
        assertEquals(View.GONE, incognitoButton.getVisibility());
    }

    @Test
    @SmallTest
    public void testButtonDimensions_TabletVsDesktop() {
        DeviceInfo.setIsDesktopForTesting(false);
        VerticalTabRailLayout tabletLayout =
                (VerticalTabRailLayout)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.vertical_tab_layout, null, false);
        assertEquals(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tabs_header_button_size_tablet),
                tabletLayout.getButtonSizePxForTesting());
        assertEquals(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tabs_footer_button_height_tablet),
                tabletLayout.getIncognitoChipSizePxForTesting());

        DeviceInfo.setIsDesktopForTesting(true);
        VerticalTabRailLayout desktopLayout =
                (VerticalTabRailLayout)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.vertical_tab_layout, null, false);
        assertEquals(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tabs_header_button_size),
                desktopLayout.getButtonSizePxForTesting());
        assertEquals(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tabs_footer_button_height),
                desktopLayout.getIncognitoChipSizePxForTesting());
    }

    @Test
    @SmallTest
    public void testPinnedTabsRecyclerViewPadding() {
        int expectedPaddingTop =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_pinned_tabs_padding_top);
        int expectedPaddingBottom =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_pinned_tabs_padding_bottom);
        assertEquals(expectedPaddingTop, mRailLayout.getPinnedTabsRecyclerView().getPaddingTop());
        assertEquals(
                expectedPaddingBottom, mRailLayout.getPinnedTabsRecyclerView().getPaddingBottom());
    }

    private void measureAndLayout(View view, int width, int height) {
        view.measure(
                View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY));
        view.layout(0, 0, width, height);
    }
}
