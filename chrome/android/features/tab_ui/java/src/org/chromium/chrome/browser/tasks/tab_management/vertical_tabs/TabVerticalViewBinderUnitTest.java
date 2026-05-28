// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.InputDevice;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData.TabActionButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.util.TextResolver;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.components.tab_groups.TabGroupsFeatureMap;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link TabVerticalViewBinder}. */
// TODO(crbug.com/515147675): Create an instrumented RenderTest class once Pinned Tabs and
//  Tab Group Spines are implemented, to capture pixel snapshots of the rows in all visual states
//  (resting, selected, incognito, pinned, etc.).
@RunWith(BaseRobolectricTestRunner.class)
public class TabVerticalViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private LinearLayout mItemView;
    private TextView mTitleView;
    private ImageView mFaviconView;
    private ImageView mCloseButton;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mItemView =
                (LinearLayout)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.vertical_tab_item, null, false);
        mTitleView = mItemView.findViewById(R.id.tab_title);
        mFaviconView = mItemView.findViewById(R.id.tab_favicon);
        mCloseButton = mItemView.findViewById(R.id.action_button);

        mModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(TabProperties.IS_INCOGNITO, false)
                        .build();
    }

    @Test
    @SmallTest
    public void testBindTitle() {
        mModel.set(TabProperties.TITLE, "Google");
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.TITLE);

        assertEquals("Google", mTitleView.getText());
    }

    @Test
    @SmallTest
    public void testBindSelectionColors_Selected() {
        mModel.set(TabProperties.IS_SELECTED, true);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_SELECTED);

        ColorStateList bgTint = mItemView.getBackgroundTintList();
        assertNotNull("Background tint should not be null when selected", bgTint);
        assertNotNull(mTitleView.getTextColors());
    }

    @Test
    @SmallTest
    public void testBindSelectionColors_Unselected() {
        mModel.set(TabProperties.IS_SELECTED, false);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_SELECTED);

        ColorStateList bgTint = mItemView.getBackgroundTintList();
        assertNotNull(bgTint);
        assertEquals(Color.TRANSPARENT, bgTint.getDefaultColor());
    }

    @Test
    @SmallTest
    public void testBindFavicon() {
        TabFaviconFetcher mockFetcher = mock(TabFaviconFetcher.class);
        TabFavicon mockFavicon = mock(TabFavicon.class);
        Drawable mockDrawable = mock(Drawable.class);
        when(mockDrawable.mutate()).thenReturn(mockDrawable);
        when(mockFavicon.getDefaultDrawable()).thenReturn(mockDrawable);

        doAnswer(
                        invocation -> {
                            Callback<TabFavicon> callback = invocation.getArgument(0);
                            callback.onResult(mockFavicon);
                            return null;
                        })
                .when(mockFetcher)
                .fetch(any());

        mModel.set(TabProperties.FAVICON_FETCHER, mockFetcher);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.FAVICON_FETCHER);

        assertEquals(View.VISIBLE, mFaviconView.getVisibility());
        assertEquals(mockDrawable, mFaviconView.getDrawable());
    }

    @Test
    @SmallTest
    public void testBindFavicon_NullFetcher() {
        mModel.set(TabProperties.FAVICON_FETCHER, null);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.FAVICON_FETCHER);

        assertEquals(View.GONE, mFaviconView.getVisibility());
        assertNull(mFaviconView.getDrawable());
    }

    @Test
    @SmallTest
    public void testBindClickListeners() {
        TabActionListener mockClickListener = mock(TabActionListener.class);
        mModel.set(TabProperties.TAB_ID, 123);
        mModel.set(TabProperties.TAB_CLICK_LISTENER, mockClickListener);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.TAB_CLICK_LISTENER);

        mItemView.performClick();
        verify(mockClickListener).run(any(View.class), eq(123), any());
    }

    @Test
    @SmallTest
    public void testBindCloseButtonClickListener() {
        TabActionListener mockCloseListener = mock(TabActionListener.class);
        TabActionButtonData actionButtonData =
                new TabActionButtonData(TabActionButtonType.CLOSE, mockCloseListener);
        mModel.set(TabProperties.TAB_ID, 123);
        mModel.set(TabProperties.TAB_ACTION_BUTTON_DATA, actionButtonData);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.TAB_ACTION_BUTTON_DATA);

        mCloseButton.performClick();
        verify(mockCloseListener).run(any(View.class), eq(123), any());
    }

    @Test
    @SmallTest
    public void testCloseButtonHover() {
        mModel.set(TabProperties.IS_SELECTED, false);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_SELECTED);

        assertEquals(View.INVISIBLE, mCloseButton.getVisibility());

        MotionEvent hoverEnterEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 0f, 0f, 0);
        hoverEnterEvent.setSource(InputDevice.SOURCE_MOUSE);
        mItemView.dispatchGenericMotionEvent(hoverEnterEvent);
        assertEquals(View.VISIBLE, mCloseButton.getVisibility());

        MotionEvent hoverExitEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_EXIT, 0f, 0f, 0);
        hoverExitEvent.setSource(InputDevice.SOURCE_MOUSE);
        mItemView.dispatchGenericMotionEvent(hoverExitEvent);
        assertEquals(View.INVISIBLE, mCloseButton.getVisibility());
    }

    @Test
    @SmallTest
    public void testCloseButtonHover_Selected() {
        mModel.set(TabProperties.IS_SELECTED, true);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_SELECTED);

        assertEquals(View.VISIBLE, mCloseButton.getVisibility());

        MotionEvent hoverEnterEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 0f, 0f, 0);
        hoverEnterEvent.setSource(InputDevice.SOURCE_MOUSE);
        mItemView.dispatchGenericMotionEvent(hoverEnterEvent);
        assertEquals(View.VISIBLE, mCloseButton.getVisibility());

        MotionEvent hoverExitEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_EXIT, 0f, 0f, 0);
        hoverExitEvent.setSource(InputDevice.SOURCE_MOUSE);
        mItemView.dispatchGenericMotionEvent(hoverExitEvent);
        assertEquals(View.VISIBLE, mCloseButton.getVisibility());
    }

    @Test
    @SmallTest
    public void testBindPinnedTab_FaviconAndClick() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        ViewGroup pinnedView =
                (ViewGroup)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.vertical_tab_pinned_item, null, false);
        ImageView faviconView = pinnedView.findViewById(R.id.tab_favicon);

        // 1. Test Favicon fetching
        TabFaviconFetcher mockFetcher = mock(TabFaviconFetcher.class);
        TabFavicon mockFavicon = mock(TabFavicon.class);
        Drawable mockDrawable = mock(Drawable.class);
        when(mockDrawable.mutate()).thenReturn(mockDrawable);
        when(mockFavicon.getDefaultDrawable()).thenReturn(mockDrawable);
        doAnswer(
                        invocation -> {
                            Callback<TabFavicon> callback = invocation.getArgument(0);
                            callback.onResult(mockFavicon);
                            return null;
                        })
                .when(mockFetcher)
                .fetch(any());

        mModel.set(TabProperties.FAVICON_FETCHER, mockFetcher);
        TabVerticalViewBinder.bindPinnedTab(mModel, pinnedView, TabProperties.FAVICON_FETCHER);
        assertEquals(mockDrawable, faviconView.getDrawable());

        // 2. Test Click Listener
        TabActionListener mockClickListener = mock(TabActionListener.class);
        mModel.set(TabProperties.TAB_ID, 123);
        mModel.set(TabProperties.TAB_CLICK_LISTENER, mockClickListener);
        TabVerticalViewBinder.bindPinnedTab(mModel, pinnedView, TabProperties.TAB_CLICK_LISTENER);
        pinnedView.performClick();
        verify(mockClickListener).run(any(View.class), eq(123), any());
    }

    @Test
    @SmallTest
    public void testBindPinnedTab_LongAndContextClick() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        ViewGroup pinnedView =
                (ViewGroup)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.vertical_tab_pinned_item, null, false);

        // 1. Test Long Click Listener
        TabActionListener mockLongClickListener = mock(TabActionListener.class);
        mModel.set(TabProperties.TAB_ID, 123);
        mModel.set(TabProperties.TAB_LONG_CLICK_LISTENER, mockLongClickListener);
        TabVerticalViewBinder.bindPinnedTab(
                mModel, pinnedView, TabProperties.TAB_LONG_CLICK_LISTENER);
        pinnedView.performLongClick();
        verify(mockLongClickListener).run(any(View.class), eq(123), any());

        // 2. Test Context Click Listener
        TabActionListener mockContextClickListener = mock(TabActionListener.class);
        mModel.set(TabProperties.TAB_CONTEXT_CLICK_LISTENER, mockContextClickListener);
        TabVerticalViewBinder.bindPinnedTab(
                mModel, pinnedView, TabProperties.TAB_CONTEXT_CLICK_LISTENER);
        pinnedView.performContextClick(0f, 0f);
        verify(mockContextClickListener).run(any(View.class), eq(123), any());
    }

    @Test
    @SmallTest
    public void testBindPinnedTab_SelectionColors() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        ViewGroup pinnedView =
                (ViewGroup)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.vertical_tab_pinned_item, null, false);

        // 1. When Pinned Tab is Selected
        mModel.set(TabProperties.IS_SELECTED, true);
        TabVerticalViewBinder.bindPinnedTab(mModel, pinnedView, TabProperties.IS_SELECTED);
        ColorStateList selectedTint = pinnedView.getBackgroundTintList();
        assertNotNull("Background tint should not be null when selected", selectedTint);

        // 2. When Pinned Tab is Resting (Unselected)
        mModel.set(TabProperties.IS_SELECTED, false);
        TabVerticalViewBinder.bindPinnedTab(mModel, pinnedView, TabProperties.IS_SELECTED);
        ColorStateList restingTint = pinnedView.getBackgroundTintList();
        assertNull("Background tint should be null when resting to allow XML color", restingTint);
    }

    @Test
    @SmallTest
    public void testBindPinnedTab_ContentDescription() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        ViewGroup pinnedView =
                (ViewGroup)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.vertical_tab_pinned_item, null, false);

        mModel.set(TabProperties.TITLE, "Google Website");
        TabVerticalViewBinder.bindPinnedTab(mModel, pinnedView, TabProperties.TITLE);

        assertEquals("Google Website", pinnedView.getContentDescription().toString());
    }

    @Test
    @SmallTest
    @DisableFeatures({TabGroupsFeatureMap.UPDATE_TAB_GROUP_COLORS})
    public void testBindTabGroupHeader_TitleAndColors() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        ViewGroup headerView =
                (ViewGroup)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.vertical_tab_group_header, null, false);
        TextView titleView = headerView.findViewById(R.id.group_title);

        // 1. Test Title binding
        mModel.set(TabProperties.TITLE, "My Research Group");
        TabVerticalViewBinder.bindTabGroupHeader(mModel, headerView, TabProperties.TITLE);
        assertEquals("My Research Group", titleView.getText());

        // 2. Test Colors tinting
        mModel.set(TabProperties.TAB_GROUP_CARD_COLOR, TabGroupColorId.RED);
        TabVerticalViewBinder.bindTabGroupHeader(
                mModel, headerView, TabProperties.TAB_GROUP_CARD_COLOR);

        Drawable bg = headerView.getBackground();
        assertNotNull("Background drawable should not be null", bg);

        ColorStateList tintList = headerView.getBackgroundTintList();
        assertNotNull("Background tint list should be set", tintList);

        int expectedColor =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                        activity, TabGroupColorId.RED, /* isIncognito= */ false);
        assertEquals(expectedColor, tintList.getDefaultColor());

        // 3. Test Colors tinting in Incognito mode
        PropertyModel incognitoModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(TabProperties.IS_INCOGNITO, true)
                        .with(TabProperties.TAB_GROUP_CARD_COLOR, TabGroupColorId.RED)
                        .build();
        TabVerticalViewBinder.bindTabGroupHeader(
                incognitoModel, headerView, TabProperties.IS_INCOGNITO);

        tintList = headerView.getBackgroundTintList();
        assertNotNull("Background tint list should be set in Incognito", tintList);
        int expectedIncognitoColor =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                        activity, TabGroupColorId.RED, /* isIncognito= */ true);
        assertEquals(expectedIncognitoColor, tintList.getDefaultColor());
    }

    @Test
    @SmallTest
    public void testBindTabGroupHeader_ContentDescription() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        ViewGroup headerView =
                (ViewGroup)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.vertical_tab_group_header, null, false);

        TextResolver resolver = context -> "Accessibility Group Description";

        mModel.set(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER, resolver);
        TabVerticalViewBinder.bindTabGroupHeader(
                mModel, headerView, TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER);

        assertEquals(
                "Accessibility Group Description", headerView.getContentDescription().toString());
    }

    @Test
    @SmallTest
    public void testBindTabGroupHeader_CollapsedState() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        ViewGroup headerView =
                (ViewGroup)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.vertical_tab_group_header, null, false);
        ImageView expandChevron = headerView.findViewById(R.id.expand_chevron);

        // 0. Test Default/Null State (should point up - 180 degrees as false = expanded)
        TabVerticalViewBinder.bindTabGroupHeader(mModel, headerView, TabProperties.IS_COLLAPSED);
        assertEquals(180f, expandChevron.getRotation(), 0.0f);

        // 1. Test Collapsed State (should point down - 0 degrees)
        mModel.set(TabProperties.IS_COLLAPSED, true);
        TabVerticalViewBinder.bindTabGroupHeader(mModel, headerView, TabProperties.IS_COLLAPSED);
        assertEquals(0f, expandChevron.getRotation(), 0.0f);

        // 2. Test Expanded/Not-Collapsed State (should point up - 180 degrees)
        mModel.set(TabProperties.IS_COLLAPSED, false);
        TabVerticalViewBinder.bindTabGroupHeader(mModel, headerView, TabProperties.IS_COLLAPSED);
        assertEquals(180f, expandChevron.getRotation(), 0.0f);
    }

    @Test
    @SmallTest
    public void testBindTabGroupId_Padding() {
        mItemView.setLayoutParams(
                new ViewGroup.MarginLayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        mModel.set(TabProperties.TAB_GROUP_ID, new org.chromium.base.Token(1L, 2L));
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.TAB_GROUP_ID);

        ViewGroup.MarginLayoutParams lp =
                (ViewGroup.MarginLayoutParams) mItemView.getLayoutParams();
        assertNotNull("MarginLayoutParams should not be null", lp);
        int expectedMargin =
                mItemView
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_child_nesting_margin);
        assertEquals(expectedMargin, lp.getMarginStart());

        mModel.set(TabProperties.TAB_GROUP_ID, null);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.TAB_GROUP_ID);
        assertEquals(0, lp.getMarginStart());
    }
}
