// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.animation.ObjectAnimator;
import android.app.Activity;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.InputDevice;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.R.string;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.UiTabState;
import org.chromium.chrome.browser.actor.ui.TabIndicatorStatus;
import org.chromium.chrome.browser.tab.MediaState;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData.TabActionButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.util.TextResolver;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.components.tab_groups.TabGroupsFeatureMap;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link TabVerticalViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabVerticalViewBinderUnitTest {
    private static final String TEST_TITLE = "Google Website";
    private static final String TEST_DESCRIPTION = "Normal Tab Description";
    private static final String TEST_ACCESSIBILITY_DESCRIPTION = "Accessibility Tab Description";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabActionListener mCloseListener;
    @Mock private TabActionListener mClickListener;
    @Mock private TabActionListener mLongClickListener;
    @Mock private TabActionListener mContextClickListener;
    @Mock private View.AccessibilityDelegate mAccessibilityDelegate;
    @Mock private Drawable mFaviconDrawable;
    @Mock private TabFavicon mTabFavicon;
    @Mock private TabFaviconFetcher mFaviconFetcher;
    @Mock private TabFaviconFetcher mFaviconFetcher1;
    @Mock private TabFaviconFetcher mFaviconFetcher2;

    private ViewGroup mItemView;
    private TextView mTitleView;
    private ImageView mFaviconView;
    private ImageView mCloseButton;
    private ImageView mMediaIndicatorView;
    private View mIndicatorView;
    private ImageView mActuationSparkView;
    private ImageView mActuationSpinnerView;
    private PropertyModel mModel;
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        Configuration config = mActivity.getResources().getConfiguration();
        config.smallestScreenWidthDp = 600;
        mActivity
                .getResources()
                .updateConfiguration(config, mActivity.getResources().getDisplayMetrics());

        mItemView =
                (ViewGroup)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.vertical_tab_item, null, false);
        mActivity.setContentView(mItemView);
        mTitleView = mItemView.findViewById(R.id.tab_title);
        mFaviconView = mItemView.findViewById(R.id.tab_favicon);
        mCloseButton = mItemView.findViewById(R.id.action_button);
        mMediaIndicatorView = mItemView.findViewById(R.id.media_indicator_icon);
        mIndicatorView = mItemView.findViewById(R.id.ai_indicator);
        mActuationSparkView = mItemView.findViewById(R.id.actuation_spark);
        mActuationSpinnerView = mItemView.findViewById(R.id.actuation_spinner);

        when(mFaviconDrawable.mutate()).thenReturn(mFaviconDrawable);
        when(mTabFavicon.getDefaultDrawable()).thenReturn(mFaviconDrawable);
        when(mTabFavicon.getSelectedDrawable()).thenReturn(mFaviconDrawable);
        doAnswer(
                        invocation -> {
                            Callback<TabFavicon> callback = invocation.getArgument(0);
                            callback.onResult(mTabFavicon);
                            return null;
                        })
                .when(mFaviconFetcher)
                .fetch(any());

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
    public void testBindActorIndicator() {
        mModel.set(
                TabProperties.ACTOR_UI_STATE,
                new UiTabState(0, null, null, TabIndicatorStatus.DYNAMIC, false));
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.ACTOR_UI_STATE);
        assertEquals(View.VISIBLE, mActuationSparkView.getVisibility());
        assertEquals(View.VISIBLE, mActuationSpinnerView.getVisibility());
        ObjectAnimator animator =
                (ObjectAnimator) mActuationSpinnerView.getTag(R.id.actuation_spinner);
        assertNotNull(animator);
        assertTrue(animator.isRunning());

        mModel.set(
                TabProperties.ACTOR_UI_STATE,
                new UiTabState(0, null, null, TabIndicatorStatus.STATIC, false));
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.ACTOR_UI_STATE);
        assertEquals(View.GONE, mActuationSparkView.getVisibility());
        assertEquals(View.GONE, mActuationSpinnerView.getVisibility());
        assertFalse(animator.isRunning());

        mModel.set(
                TabProperties.ACTOR_UI_STATE,
                new UiTabState(0, null, null, TabIndicatorStatus.NONE, false));
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.ACTOR_UI_STATE);
        assertEquals(View.GONE, mActuationSparkView.getVisibility());
        assertEquals(View.GONE, mActuationSpinnerView.getVisibility());
    }

    @Test
    @SmallTest
    public void testBindGlicIndicator() {
        mModel.set(TabProperties.TITLE, TEST_TITLE);
        TextResolver resolver = context -> TEST_DESCRIPTION;
        mModel.set(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER, resolver);

        mModel.set(TabProperties.IS_GLIC_ACTIVE, true);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_GLIC_ACTIVE);
        assertIndicatorStateAndDescription(
                mIndicatorView,
                mItemView,
                View.VISIBLE,
                mActivity.getString(R.string.tab_ax_label_actor_accessing, TEST_TITLE));

        mModel.set(TabProperties.IS_GLIC_ACTIVE, false);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_GLIC_ACTIVE);
        assertIndicatorStateAndDescription(mIndicatorView, mItemView, View.GONE, TEST_DESCRIPTION);
    }

    @Test
    @SmallTest
    public void testBindGlicIndicator_WithActorUiState() {
        mModel.set(TabProperties.TITLE, TEST_TITLE);
        TextResolver resolver = context -> TEST_DESCRIPTION;
        mModel.set(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER, resolver);

        // Turn on both Glic and Actor UI State.
        mModel.set(TabProperties.IS_GLIC_ACTIVE, true);
        UiTabState actorState = new UiTabState(0, null, null, TabIndicatorStatus.DYNAMIC, false);
        mModel.set(TabProperties.ACTOR_UI_STATE, actorState);

        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_GLIC_ACTIVE);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.ACTOR_UI_STATE);

        assertIndicatorStateAndDescription(
                mIndicatorView,
                mItemView,
                View.VISIBLE,
                mActivity.getString(R.string.tab_ax_label_actor_accessing, TEST_TITLE));

        // Deactivate Glic while Actor remains active.
        mModel.set(TabProperties.IS_GLIC_ACTIVE, false);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_GLIC_ACTIVE);

        // Accessibility description should NOT be reset because Actor is still active.
        assertIndicatorStateAndDescription(
                mIndicatorView,
                mItemView,
                View.GONE,
                mActivity.getString(R.string.tab_ax_label_actor_accessing, TEST_TITLE));
    }

    @Test
    @SmallTest
    public void testBindContentDescription() {
        TextResolver resolver = context -> TEST_ACCESSIBILITY_DESCRIPTION;
        mModel.set(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER, resolver);
        TabVerticalViewBinder.bindTab(
                mModel, mItemView, TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER);

        assertEquals(TEST_ACCESSIBILITY_DESCRIPTION, mItemView.getContentDescription().toString());
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
        mModel.set(TabProperties.FAVICON_FETCHER, mFaviconFetcher);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.FAVICON_FETCHER);

        assertEquals(View.VISIBLE, mFaviconView.getVisibility());
        assertEquals(mFaviconDrawable, mFaviconView.getDrawable());
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
    public void testBindMediaIndicator() {
        mModel.set(TabProperties.MEDIA_INDICATOR, MediaState.AUDIBLE);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.MEDIA_INDICATOR);

        assertEquals(View.VISIBLE, mMediaIndicatorView.getVisibility());

        mModel.set(TabProperties.MEDIA_INDICATOR, MediaState.NONE);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.MEDIA_INDICATOR);

        assertEquals(View.GONE, mMediaIndicatorView.getVisibility());
    }

    @Test
    @SmallTest
    public void testBindClickListeners() {
        mModel.set(TabProperties.TAB_ID, 123);
        mModel.set(TabProperties.TAB_CLICK_LISTENER, mClickListener);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.TAB_CLICK_LISTENER);

        mItemView.performClick();
        verify(mClickListener).run(any(View.class), eq(123), any());
    }

    @Test
    @SmallTest
    public void testBindCloseButtonClickListener() {
        TabActionButtonData actionButtonData =
                new TabActionButtonData(TabActionButtonType.CLOSE, mCloseListener);
        mModel.set(TabProperties.TAB_ID, 123);
        mModel.set(TabProperties.TAB_ACTION_BUTTON_DATA, actionButtonData);
        mModel.set(TabProperties.IS_SELECTED, true);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.TAB_ACTION_BUTTON_DATA);

        mCloseButton.performClick();
        verify(mCloseListener).run(any(View.class), eq(123), any());
    }

    @Test
    @SmallTest
    public void testBindActionButtonDescription() {
        mModel.set(
                TabProperties.ACTION_BUTTON_DESCRIPTION_TEXT_RESOLVER,
                (context) -> "Close Google tab");
        TabVerticalViewBinder.bindTab(
                mModel, mItemView, TabProperties.ACTION_BUTTON_DESCRIPTION_TEXT_RESOLVER);

        assertEquals("Close Google tab", mCloseButton.getContentDescription());
    }

    @Test
    @SmallTest
    public void testBindAccessibilityDelegate() {
        mModel.set(TabProperties.ACCESSIBILITY_DELEGATE, mAccessibilityDelegate);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.ACCESSIBILITY_DELEGATE);

        assertEquals(mAccessibilityDelegate, mItemView.getAccessibilityDelegate());
    }

    @Test
    @SmallTest
    public void testCloseButtonHover() {
        TabActionButtonData actionButtonData =
                new TabActionButtonData(TabActionButtonType.CLOSE, mCloseListener);
        mModel.set(TabProperties.TAB_ACTION_BUTTON_DATA, actionButtonData);
        mModel.set(TabProperties.IS_SELECTED, false);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_SELECTED);

        assertEquals(View.GONE, mCloseButton.getVisibility());

        MotionEvent hoverEnterEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 0f, 0f, 0);
        hoverEnterEvent.setSource(InputDevice.SOURCE_MOUSE);
        mItemView.dispatchGenericMotionEvent(hoverEnterEvent);
        assertEquals(View.VISIBLE, mCloseButton.getVisibility());

        MotionEvent hoverExitEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_EXIT, 0f, 0f, 0);
        hoverExitEvent.setSource(InputDevice.SOURCE_MOUSE);
        mItemView.dispatchGenericMotionEvent(hoverExitEvent);
        assertEquals(View.GONE, mCloseButton.getVisibility());
    }

    @Test
    @SmallTest
    public void testCloseButtonHover_Selected() {
        TabActionButtonData actionButtonData =
                new TabActionButtonData(TabActionButtonType.CLOSE, mCloseListener);
        mModel.set(TabProperties.TAB_ACTION_BUTTON_DATA, actionButtonData);
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
    public void testTabHoverBackground() {
        mModel.set(TabProperties.IS_SELECTED, false);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_SELECTED);

        ColorStateList bgTint = mItemView.getBackgroundTintList();
        assertNotNull(bgTint);
        assertEquals(Color.TRANSPARENT, bgTint.getDefaultColor());

        MotionEvent hoverEnterEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 0f, 0f, 0);
        hoverEnterEvent.setSource(InputDevice.SOURCE_MOUSE);
        mItemView.dispatchGenericMotionEvent(hoverEnterEvent);

        bgTint = mItemView.getBackgroundTintList();
        assertNotNull(bgTint);
        assertEquals(
                TabUiThemeUtil.getHoveredTabContainerColor(
                        mItemView.getContext(), /* isIncognito= */ false),
                bgTint.getDefaultColor());

        MotionEvent hoverExitEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_EXIT, 0f, 0f, 0);
        hoverExitEvent.setSource(InputDevice.SOURCE_MOUSE);
        mItemView.dispatchGenericMotionEvent(hoverExitEvent);

        bgTint = mItemView.getBackgroundTintList();
        assertNotNull(bgTint);
        assertEquals(Color.TRANSPARENT, bgTint.getDefaultColor());
    }

    @Test
    @SmallTest
    public void testTabHoverBackground_Selected() {
        mModel.set(TabProperties.IS_SELECTED, true);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_SELECTED);

        ColorStateList bgTintBefore = mItemView.getBackgroundTintList();
        assertNotNull("Background tint should not be null when selected", bgTintBefore);

        MotionEvent hoverEnterEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 0f, 0f, 0);
        hoverEnterEvent.setSource(InputDevice.SOURCE_MOUSE);
        mItemView.dispatchGenericMotionEvent(hoverEnterEvent);

        ColorStateList bgTintAfter = mItemView.getBackgroundTintList();
        assertEquals(bgTintBefore, bgTintAfter);

        MotionEvent hoverExitEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_EXIT, 0f, 0f, 0);
        hoverExitEvent.setSource(InputDevice.SOURCE_MOUSE);
        mItemView.dispatchGenericMotionEvent(hoverExitEvent);

        bgTintAfter = mItemView.getBackgroundTintList();
        assertEquals(bgTintBefore, bgTintAfter);
    }

    @Test
    @SmallTest
    public void testTabHover_ExitToActionButton_DoesNotClearHover() {
        // Lay out item view and close button so child bounds are valid
        mItemView.layout(0, 0, 100, 50);
        mCloseButton.layout(80, 10, 95, 40);

        TabActionButtonData actionButtonData =
                new TabActionButtonData(TabActionButtonType.CLOSE, mCloseListener);
        mModel.set(TabProperties.TAB_ACTION_BUTTON_DATA, actionButtonData);
        mModel.set(TabProperties.IS_SELECTED, false);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_SELECTED);

        // Enter hover on tab row
        MotionEvent hoverEnterEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 10f, 10f, 0);
        hoverEnterEvent.setSource(InputDevice.SOURCE_MOUSE);
        mItemView.dispatchGenericMotionEvent(hoverEnterEvent);

        ColorStateList hoveredTint = mItemView.getBackgroundTintList();
        assertNotNull(hoveredTint);
        assertEquals(
                TabUiThemeUtil.getHoveredTabContainerColor(
                        mItemView.getContext(), /* isIncognito= */ false),
                hoveredTint.getDefaultColor());
        assertEquals(View.VISIBLE, mCloseButton.getVisibility());

        // Simulate close button being hovered when hover exits item view onto close button
        mCloseButton.setHovered(true);

        MotionEvent hoverExitEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_EXIT, 85f, 25f, 0);
        hoverExitEvent.setSource(InputDevice.SOURCE_MOUSE);
        mItemView.dispatchGenericMotionEvent(hoverExitEvent);

        // Verify hover background tint and close button visibility are preserved
        ColorStateList tintAfterExit = mItemView.getBackgroundTintList();
        assertNotNull(tintAfterExit);
        assertEquals(hoveredTint.getDefaultColor(), tintAfterExit.getDefaultColor());
        assertEquals(View.VISIBLE, mCloseButton.getVisibility());
    }

    @Test
    @SmallTest
    public void testActionButtonHover_ExitOutsideView_ClearsHover() {
        // Lay out item view so width and height are known (> 0)
        mItemView.layout(0, 0, 100, 50);
        mCloseButton.layout(80, 10, 95, 40);

        TabActionButtonData actionButtonData =
                new TabActionButtonData(TabActionButtonType.CLOSE, mCloseListener);
        mModel.set(TabProperties.TAB_ACTION_BUTTON_DATA, actionButtonData);
        mModel.set(TabProperties.IS_SELECTED, false);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_SELECTED);

        // Enter hover on tab row
        MotionEvent hoverEnterEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 0f, 0f, 0);
        hoverEnterEvent.setSource(InputDevice.SOURCE_MOUSE);
        mItemView.dispatchGenericMotionEvent(hoverEnterEvent);
        assertEquals(View.VISIBLE, mCloseButton.getVisibility());

        // Dispatch ACTION_HOVER_EXIT on close button with coordinates outside mItemView bounds
        // Notice v.getLeft() + x = 80 + 50 = 130 > mItemView.getWidth() (100)
        MotionEvent buttonExitEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_EXIT, 50f, 0f, 0);
        buttonExitEvent.setSource(InputDevice.SOURCE_MOUSE);
        mCloseButton.dispatchGenericMotionEvent(buttonExitEvent);

        // Verify row un-hovered and close button is gone
        ColorStateList bgTint = mItemView.getBackgroundTintList();
        assertNotNull(bgTint);
        assertEquals(Color.TRANSPARENT, bgTint.getDefaultColor());
        assertEquals(View.GONE, mCloseButton.getVisibility());
    }

    @Test
    @SmallTest
    public void testActionButtonTouchDelegate_SetAndCleared() {
        mItemView.layout(0, 0, 100, 32);
        mCloseButton.layout(80, 8, 96, 24);

        TabActionButtonData actionButtonData =
                new TabActionButtonData(TabActionButtonType.CLOSE, mCloseListener);
        mModel.set(TabProperties.TAB_ACTION_BUTTON_DATA, actionButtonData);

        // Touch delegate is set when action button is wanted
        mModel.set(TabProperties.IS_SELECTED, true);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_SELECTED);
        ShadowLooper.idleMainLooper();
        assertNotNull(mItemView.getTouchDelegate());

        // Touch delegate is cleared when action button is not wanted
        mModel.set(TabProperties.IS_SELECTED, false);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_SELECTED);
        ShadowLooper.idleMainLooper();
        assertNull(mItemView.getTouchDelegate());
    }

    @Test
    @SmallTest
    public void testActionButtonTouchDelegate_SetOnHover() {
        mItemView.layout(0, 0, 100, 32);
        mCloseButton.layout(80, 8, 96, 24);

        TabActionButtonData actionButtonData =
                new TabActionButtonData(TabActionButtonType.CLOSE, mCloseListener);
        mModel.set(TabProperties.TAB_ACTION_BUTTON_DATA, actionButtonData);
        mModel.set(TabProperties.IS_SELECTED, false);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_SELECTED);
        ShadowLooper.idleMainLooper();

        // Before hover, touch delegate is null
        assertNull(mItemView.getTouchDelegate());

        // Hover over the tab row
        MotionEvent hoverEnterEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 0f, 0f, 0);
        hoverEnterEvent.setSource(InputDevice.SOURCE_MOUSE);
        mItemView.dispatchGenericMotionEvent(hoverEnterEvent);
        ShadowLooper.idleMainLooper();

        // Touch delegate is set when hovered
        assertNotNull(mItemView.getTouchDelegate());
    }

    @Test
    @SmallTest
    public void testActionButtonTouchDelegate_CollapsedRail() {
        mItemView.layout(0, 0, 100, 32);
        mCloseButton.layout(80, 8, 96, 24);

        TabActionButtonData actionButtonData =
                new TabActionButtonData(TabActionButtonType.CLOSE, mCloseListener);
        mModel.set(TabProperties.TAB_ACTION_BUTTON_DATA, actionButtonData);
        mModel.set(TabProperties.RAIL_COLLAPSE_STATE, RailCollapseState.COLLAPSED);
        mModel.set(TabProperties.IS_SELECTED, true);

        // In collapsed rail mode, selected tab without hover does not show action button
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_SELECTED);
        ShadowLooper.idleMainLooper();
        assertNull(mItemView.getTouchDelegate());

        // Hover over tab row while selected in collapsed mode shows action button
        MotionEvent hoverEnterEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 0f, 0f, 0);
        hoverEnterEvent.setSource(InputDevice.SOURCE_MOUSE);
        mItemView.dispatchGenericMotionEvent(hoverEnterEvent);
        ShadowLooper.idleMainLooper();
        assertNotNull(mItemView.getTouchDelegate());
    }

    @Test
    @SmallTest
    public void testActionButtonTouchDelegate_UnattachedToWindow() {
        ViewGroup unattachedView =
                (ViewGroup)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.vertical_tab_item, null, false);
        View closeButton = unattachedView.findViewById(R.id.action_button);
        unattachedView.layout(0, 0, 100, 32);
        closeButton.layout(80, 8, 96, 24);

        TabActionButtonData actionButtonData =
                new TabActionButtonData(TabActionButtonType.CLOSE, mCloseListener);
        mModel.set(TabProperties.TAB_ACTION_BUTTON_DATA, actionButtonData);
        mModel.set(TabProperties.IS_SELECTED, true);

        TabVerticalViewBinder.bindTab(mModel, unattachedView, TabProperties.IS_SELECTED);
        ShadowLooper.idleMainLooper();

        assertNull(unattachedView.getTouchDelegate());
    }

    @Test
    @SmallTest
    public void testActionButtonHover_EnterAndMove_HighlightsRowBackground() {
        TabActionButtonData actionButtonData =
                new TabActionButtonData(TabActionButtonType.CLOSE, mCloseListener);
        mModel.set(TabProperties.TAB_ACTION_BUTTON_DATA, actionButtonData);
        mModel.set(TabProperties.IS_SELECTED, false);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_SELECTED);

        // Directly enter hover on close button without triggering enter on row
        MotionEvent hoverEnterEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 0f, 0f, 0);
        hoverEnterEvent.setSource(InputDevice.SOURCE_MOUSE);
        mCloseButton.dispatchGenericMotionEvent(hoverEnterEvent);

        ColorStateList hoveredTint = mItemView.getBackgroundTintList();
        assertNotNull(hoveredTint);
        assertEquals(
                TabUiThemeUtil.getHoveredTabContainerColor(
                        mItemView.getContext(), /* isIncognito= */ false),
                hoveredTint.getDefaultColor());
        assertEquals(View.VISIBLE, mCloseButton.getVisibility());

        // Dispatch hover move on close button
        MotionEvent hoverMoveEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_MOVE, 0f, 0f, 0);
        hoverMoveEvent.setSource(InputDevice.SOURCE_MOUSE);
        mCloseButton.dispatchGenericMotionEvent(hoverMoveEvent);

        ColorStateList tintAfterMove = mItemView.getBackgroundTintList();
        assertNotNull(tintAfterMove);
        assertEquals(hoveredTint.getDefaultColor(), tintAfterMove.getDefaultColor());
        assertEquals(View.VISIBLE, mCloseButton.getVisibility());
    }

    @Test
    @SmallTest
    public void testBindPinnedTab_FaviconAndClick() {
        ViewGroup pinnedView = inflatePinnedTabView();
        ImageView faviconView = pinnedView.findViewById(R.id.tab_favicon);

        // 1. Test Favicon fetching
        mModel.set(TabProperties.FAVICON_FETCHER, mFaviconFetcher);
        TabVerticalViewBinder.bindPinnedTab(mModel, pinnedView, TabProperties.FAVICON_FETCHER);
        assertEquals(mFaviconDrawable, faviconView.getDrawable());

        // 2. Test Click Listener
        mModel.set(TabProperties.TAB_ID, 123);
        mModel.set(TabProperties.TAB_CLICK_LISTENER, mClickListener);
        TabVerticalViewBinder.bindPinnedTab(mModel, pinnedView, TabProperties.TAB_CLICK_LISTENER);
        pinnedView.performClick();
        verify(mClickListener).run(any(View.class), eq(123), any());
    }

    @Test
    @SmallTest
    public void testBindPinnedTab_LongAndContextClick() {
        ViewGroup pinnedView = inflatePinnedTabView();

        // 1. Test Long Click Listener
        mModel.set(TabProperties.TAB_ID, 123);
        mModel.set(TabProperties.TAB_LONG_CLICK_LISTENER, mLongClickListener);
        TabVerticalViewBinder.bindPinnedTab(
                mModel, pinnedView, TabProperties.TAB_LONG_CLICK_LISTENER);
        pinnedView.performLongClick();
        verify(mLongClickListener).run(any(View.class), eq(123), any());

        // 2. Test Context Click Listener
        mModel.set(TabProperties.TAB_CONTEXT_CLICK_LISTENER, mContextClickListener);
        TabVerticalViewBinder.bindPinnedTab(
                mModel, pinnedView, TabProperties.TAB_CONTEXT_CLICK_LISTENER);
        pinnedView.performContextClick(0f, 0f);
        verify(mContextClickListener).run(any(View.class), eq(123), any());
    }

    @Test
    @SmallTest
    public void testBindPinnedTab_SelectionColors() {
        ViewGroup pinnedView = inflatePinnedTabView();

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
        ViewGroup pinnedView = inflatePinnedTabView();

        mModel.set(TabProperties.TITLE, TEST_TITLE);
        TabVerticalViewBinder.bindPinnedTab(mModel, pinnedView, TabProperties.TITLE);

        assertEquals(TEST_TITLE, pinnedView.getContentDescription().toString());
    }

    @Test
    @SmallTest
    public void testBindPinnedTab_GlicIndicator() {
        ViewGroup pinnedView = inflatePinnedTabView();
        View glicIndicator = pinnedView.findViewById(R.id.ai_indicator);
        assertNotNull(glicIndicator);

        mModel.set(TabProperties.TITLE, TEST_TITLE);
        mModel.set(TabProperties.IS_GLIC_ACTIVE, true);
        TabVerticalViewBinder.bindPinnedTab(mModel, pinnedView, TabProperties.IS_GLIC_ACTIVE);
        assertIndicatorStateAndDescription(
                glicIndicator,
                pinnedView,
                View.VISIBLE,
                mActivity.getString(R.string.tab_ax_label_actor_accessing, TEST_TITLE));

        mModel.set(TabProperties.IS_GLIC_ACTIVE, false);
        TabVerticalViewBinder.bindPinnedTab(mModel, pinnedView, TabProperties.IS_GLIC_ACTIVE);
        assertIndicatorStateAndDescription(glicIndicator, pinnedView, View.GONE, TEST_TITLE);
    }

    @Test
    @SmallTest
    @DisableFeatures({TabGroupsFeatureMap.UPDATE_TAB_GROUP_COLORS})
    public void testBindTabGroupHeader_TitleAndColors() {
        ViewGroup headerView = inflateGroupHeaderView();
        TextView titleView = headerView.findViewById(R.id.group_title);
        ImageView expandChevron = headerView.findViewById(R.id.expand_chevron);

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

        int expectedBackgroundColor =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                        mActivity, TabGroupColorId.RED, /* isIncognito= */ false);
        assertEquals(expectedBackgroundColor, tintList.getDefaultColor());

        int expectedForegroundColor =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemTextColor(
                        mActivity, TabGroupColorId.RED, /* isIncognito= */ false);
        assertEquals(expectedForegroundColor, titleView.getCurrentTextColor());
        assertEquals(expectedForegroundColor, expandChevron.getImageTintList().getDefaultColor());
    }

    @Test
    @SmallTest
    public void testBindTabGroupHeader_NoGlicIndicator() {
        ViewGroup headerView = inflateGroupHeaderView();
        mModel.set(TabProperties.IS_GLIC_ACTIVE, true);
        TabVerticalViewBinder.bindTabGroupHeader(mModel, headerView, TabProperties.IS_GLIC_ACTIVE);

        View glicIndicator = headerView.findViewById(R.id.ai_indicator);
        assertNull(glicIndicator);
    }

    @Test
    @SmallTest
    public void testBindTabGroupHeader_ContentDescription() {
        ViewGroup headerView = inflateGroupHeaderView();

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
        ViewGroup headerView = inflateGroupHeaderView();
        ImageView expandChevron = headerView.findViewById(R.id.expand_chevron);

        // Test Detached / Recycled State (should snap instantly)
        assertFalse(headerView.isAttachedToWindow());
        mModel.set(TabProperties.IS_COLLAPSED, false);
        TabVerticalViewBinder.bindTabGroupHeader(mModel, headerView, TabProperties.IS_COLLAPSED);
        assertEquals(180f, expandChevron.getRotation(), 0.0f);

        // Toggling to Collapsed while detached (should instantly snap to 0 degrees)
        mModel.set(TabProperties.IS_COLLAPSED, true);
        TabVerticalViewBinder.bindTabGroupHeader(mModel, headerView, TabProperties.IS_COLLAPSED);
        assertEquals(0f, expandChevron.getRotation(), 0.0f);

        // Test Attached / Clicked State (should animate)
        mActivity.setContentView(headerView);
        assertTrue(headerView.isAttachedToWindow());

        // Toggling back to Expanded while attached (should animate to 180 degrees)
        mModel.set(TabProperties.IS_COLLAPSED, false);
        TabVerticalViewBinder.bindTabGroupHeader(mModel, headerView, TabProperties.IS_COLLAPSED);

        assertEquals(0f, expandChevron.getRotation(), 0.0f);
        ShadowLooper.idleMainLooper(
                TabVerticalViewBinder.CHEVRON_ANIMATION_DURATION_MS, TimeUnit.MILLISECONDS);
        assertEquals(180f, expandChevron.getRotation(), 0.0f);
    }

    @Test
    @SmallTest
    public void testTabGroupHeaderAccessibilityDelegate() {
        ViewGroup headerView = inflateGroupHeaderView();

        // Initially collapsed = true.
        mModel.set(TabProperties.IS_COLLAPSED, true);
        TabVerticalViewBinder.bindTabGroupHeader(mModel, headerView, TabProperties.IS_COLLAPSED);

        // Get the accessibility delegate.
        View.AccessibilityDelegate delegate = headerView.getAccessibilityDelegate();
        assertNotNull("Accessibility delegate should be set", delegate);

        AccessibilityNodeInfo nodeInfo = AccessibilityNodeInfo.obtain();
        delegate.onInitializeAccessibilityNodeInfo(headerView, nodeInfo);

        // Verify action click description is "Expand section".
        boolean hasExpandAction = false;
        String expandLabel = mActivity.getString(string.accessibility_expand_section);
        for (AccessibilityNodeInfo.AccessibilityAction action : nodeInfo.getActionList()) {
            if (action.getId() == AccessibilityNodeInfo.ACTION_CLICK) {
                assertEquals(expandLabel, action.getLabel());
                hasExpandAction = true;
            }
        }
        assertTrue("Should contain expand click action", hasExpandAction);

        // Toggle to expanded = false.
        mModel.set(TabProperties.IS_COLLAPSED, false);
        TabVerticalViewBinder.bindTabGroupHeader(mModel, headerView, TabProperties.IS_COLLAPSED);

        delegate = headerView.getAccessibilityDelegate();
        assertNotNull("Accessibility delegate should not be null after model update", delegate);

        nodeInfo = AccessibilityNodeInfo.obtain();
        delegate.onInitializeAccessibilityNodeInfo(headerView, nodeInfo);

        // Verify action click description updates to "Collapse section".
        boolean hasCollapseAction = false;
        String collapseLabel = mActivity.getString(string.accessibility_collapse_section);
        for (AccessibilityNodeInfo.AccessibilityAction action : nodeInfo.getActionList()) {
            if (action.getId() == AccessibilityNodeInfo.ACTION_CLICK) {
                assertEquals(collapseLabel, action.getLabel());
                hasCollapseAction = true;
            }
        }
        assertTrue("Should contain collapse click action", hasCollapseAction);
    }

    @Test
    @SmallTest
    public void testBindTabGroupId_Padding() {
        mItemView.setLayoutParams(
                new ViewGroup.MarginLayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        mModel.set(TabProperties.TAB_GROUP_ID, new Token(1L, 2L));
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

    @Test
    @SmallTest
    public void testBindLoadingState_WithFavicon() {
        View spinner = mItemView.findViewById(R.id.tab_loading_spinner);
        assertNotNull(spinner);

        mModel.set(TabProperties.FAVICON_FETCHER, mFaviconFetcher1);

        // 1. Loading
        mModel.set(TabProperties.IS_LOADING, true);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_LOADING);
        assertEquals(View.VISIBLE, spinner.getVisibility());
        assertNotEquals(View.VISIBLE, mFaviconView.getVisibility());

        // 2. Favicon fetcher updated while loading (should not break INVISIBLE state)
        mModel.set(TabProperties.FAVICON_FETCHER, mFaviconFetcher2);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.FAVICON_FETCHER);
        assertEquals(View.VISIBLE, spinner.getVisibility());
        assertNotEquals(View.VISIBLE, mFaviconView.getVisibility());

        // 3. Not Loading
        mModel.set(TabProperties.IS_LOADING, false);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_LOADING);
        assertNotEquals(View.VISIBLE, spinner.getVisibility());
        assertEquals(View.VISIBLE, mFaviconView.getVisibility());
    }

    @Test
    @SmallTest
    public void testBindLoadingState_WithoutFavicon() {
        View spinner = mItemView.findViewById(R.id.tab_loading_spinner);
        assertNotNull(spinner);

        mModel.set(TabProperties.FAVICON_FETCHER, null);

        // 1. Loading
        mModel.set(TabProperties.IS_LOADING, true);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_LOADING);
        assertEquals(View.VISIBLE, spinner.getVisibility());
        assertEquals(View.GONE, mFaviconView.getVisibility());

        // 2. Not Loading
        mModel.set(TabProperties.IS_LOADING, false);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_LOADING);
        assertEquals(View.GONE, spinner.getVisibility());
        assertEquals(View.GONE, mFaviconView.getVisibility());
    }

    @Test
    @SmallTest
    public void testPinnedTabHoverBackground() {
        ViewGroup pinnedView = inflatePinnedTabView();

        // Pinned tabs should not have an action button
        assertNull(pinnedView.findViewById(R.id.action_button));

        mModel.set(TabProperties.IS_SELECTED, false);
        TabVerticalViewBinder.bindPinnedTab(mModel, pinnedView, TabProperties.IS_SELECTED);

        // Initially, background tint should be null for resting pinned tab
        assertNull(pinnedView.getBackgroundTintList());

        // Hover enter
        MotionEvent hoverEnterEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 0f, 0f, 0);
        hoverEnterEvent.setSource(InputDevice.SOURCE_MOUSE);
        pinnedView.dispatchGenericMotionEvent(hoverEnterEvent);

        ColorStateList bgTint = pinnedView.getBackgroundTintList();
        assertNotNull(bgTint);
        assertEquals(
                TabUiThemeUtil.getHoveredTabContainerColor(
                        pinnedView.getContext(), /* isIncognito= */ false),
                bgTint.getDefaultColor());

        // Hover exit
        MotionEvent hoverExitEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_EXIT, 0f, 0f, 0);
        hoverExitEvent.setSource(InputDevice.SOURCE_MOUSE);
        pinnedView.dispatchGenericMotionEvent(hoverExitEvent);

        // Should go back to null (not TRANSPARENT) to allow XML background to show
        assertNull(pinnedView.getBackgroundTintList());
    }

    @Test
    @SmallTest
    public void testPinnedTabHoverBackground_Selected() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        ViewGroup pinnedView =
                (ViewGroup)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.vertical_tab_pinned_item, null, false);

        mModel.set(TabProperties.IS_SELECTED, true);
        TabVerticalViewBinder.bindPinnedTab(mModel, pinnedView, TabProperties.IS_SELECTED);

        ColorStateList bgTintBefore = pinnedView.getBackgroundTintList();
        assertNotNull("Background tint should not be null when selected", bgTintBefore);

        // Hover enter
        MotionEvent hoverEnterEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 0f, 0f, 0);
        hoverEnterEvent.setSource(InputDevice.SOURCE_MOUSE);
        pinnedView.dispatchGenericMotionEvent(hoverEnterEvent);

        // Hovering shouldn't change the selected background tint
        ColorStateList bgTintAfter = pinnedView.getBackgroundTintList();
        assertEquals(bgTintBefore, bgTintAfter);

        // Hover exit
        MotionEvent hoverExitEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_EXIT, 0f, 0f, 0);
        hoverExitEvent.setSource(InputDevice.SOURCE_MOUSE);
        pinnedView.dispatchGenericMotionEvent(hoverExitEvent);

        bgTintAfter = pinnedView.getBackgroundTintList();
        assertEquals(bgTintBefore, bgTintAfter);
    }

    @Test
    @SmallTest
    public void testBindTab_RailCollapsed() {
        mItemView.setLayoutParams(
                new ViewGroup.MarginLayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mModel.set(TabProperties.TITLE, "Google");
        TextResolver resolver = context -> "Google";
        mModel.set(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER, resolver);
        mModel.set(TabProperties.RAIL_COLLAPSE_STATE, RailCollapseState.COLLAPSED);
        mModel.set(TabProperties.TAB_GROUP_ID, new Token(1L, 2L)); // In group

        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.RAIL_COLLAPSE_STATE);

        int expectedSize =
                mItemView
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_item_collapsed_size);
        assertEquals(expectedSize, mItemView.getLayoutParams().width);
        assertEquals(expectedSize, mItemView.getLayoutParams().height);

        assertNotEquals(View.VISIBLE, mTitleView.getVisibility());
        assertEquals("Google", mItemView.getContentDescription());
        assertNotEquals(View.VISIBLE, mCloseButton.getVisibility());
        assertNotEquals(View.VISIBLE, mMediaIndicatorView.getVisibility());
        assertNotEquals(View.VISIBLE, mIndicatorView.getVisibility());

        // Verify padding is collapsed margin
        ViewGroup.MarginLayoutParams lp =
                (ViewGroup.MarginLayoutParams) mItemView.getLayoutParams();
        int expectedMargin =
                TabVerticalViewBinder.getCollapsedChildMarginStart(mItemView.getContext());
        assertEquals(expectedMargin, lp.getMarginStart());
    }

    @Test
    @SmallTest
    public void testBindTab_RailExpanded_InGroup() {
        mItemView.setLayoutParams(
                new ViewGroup.MarginLayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mModel.set(TabProperties.TITLE, "Google");
        mModel.set(TabProperties.RAIL_COLLAPSE_STATE, RailCollapseState.EXPANDED);
        mModel.set(TabProperties.IS_SELECTED, true);
        mModel.set(TabProperties.TAB_GROUP_ID, new Token(1L, 2L)); // In group
        mModel.set(
                TabProperties.TAB_ACTION_BUTTON_DATA,
                new TabActionButtonData(TabActionButtonType.CLOSE, mCloseListener));

        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.RAIL_COLLAPSE_STATE);

        assertEquals(ViewGroup.LayoutParams.MATCH_PARENT, mItemView.getLayoutParams().width);
        assertEquals(ViewGroup.LayoutParams.WRAP_CONTENT, mItemView.getLayoutParams().height);

        assertEquals(View.VISIBLE, mTitleView.getVisibility());
        assertEquals("Google", mTitleView.getText());
        assertEquals(View.VISIBLE, mCloseButton.getVisibility());

        // Verify padding is nesting margin
        ViewGroup.MarginLayoutParams lp =
                (ViewGroup.MarginLayoutParams) mItemView.getLayoutParams();
        int expectedMargin =
                mItemView
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_child_nesting_margin);
        assertEquals(expectedMargin, lp.getMarginStart());
    }

    @Test
    @SmallTest
    public void testBindPinnedTab_RailCollapsed() {
        ViewGroup pinnedView = inflatePinnedTabView();
        pinnedView.setLayoutParams(
                new ViewGroup.MarginLayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        mModel.set(TabProperties.RAIL_COLLAPSE_STATE, RailCollapseState.COLLAPSED);

        TabVerticalViewBinder.bindPinnedTab(mModel, pinnedView, TabProperties.RAIL_COLLAPSE_STATE);

        int expectedSize =
                pinnedView
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_item_collapsed_size);
        assertEquals(expectedSize, pinnedView.getLayoutParams().width);
        assertEquals(expectedSize, pinnedView.getLayoutParams().height);

        // Verify padding is collapsed margin
        ViewGroup.MarginLayoutParams lp =
                (ViewGroup.MarginLayoutParams) pinnedView.getLayoutParams();
        int expectedMargin =
                TabVerticalViewBinder.getCollapsedChildMarginStart(pinnedView.getContext());
        assertEquals(expectedMargin, lp.getMarginStart());
    }

    @Test
    @SmallTest
    public void testBindPinnedTab_RailExpanded() {
        ViewGroup pinnedView = inflatePinnedTabView();
        pinnedView.setLayoutParams(
                new ViewGroup.MarginLayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        mModel.set(TabProperties.RAIL_COLLAPSE_STATE, RailCollapseState.EXPANDED);

        TabVerticalViewBinder.bindPinnedTab(mModel, pinnedView, TabProperties.RAIL_COLLAPSE_STATE);

        int expectedWidth =
                pinnedView
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_pinned_item_width);
        int expectedHeight =
                pinnedView
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_pinned_item_height);
        assertEquals(expectedWidth, pinnedView.getLayoutParams().width);
        assertEquals(expectedHeight, pinnedView.getLayoutParams().height);

        // Verify padding is 0
        ViewGroup.MarginLayoutParams lp =
                (ViewGroup.MarginLayoutParams) pinnedView.getLayoutParams();
        assertEquals(0, lp.getMarginStart());
    }

    @Test
    @SmallTest
    public void testBindTabGroupHeader_RailCollapsed() {
        ViewGroup headerView = inflateGroupHeaderView();
        headerView.setLayoutParams(
                new ViewGroup.MarginLayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        TextView titleView = headerView.findViewById(R.id.group_title);

        mModel.set(TabProperties.TITLE, "My Group");
        mModel.set(TabProperties.RAIL_COLLAPSE_STATE, RailCollapseState.COLLAPSED);

        TabVerticalViewBinder.bindTabGroupHeader(
                mModel, headerView, TabProperties.RAIL_COLLAPSE_STATE);

        int expectedSize =
                headerView
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_item_collapsed_size);
        assertEquals(expectedSize, headerView.getLayoutParams().width);
        assertEquals(expectedSize, headerView.getLayoutParams().height);
        assertEquals(View.GONE, titleView.getVisibility());

        // Verify padding is collapsed margin
        ViewGroup.MarginLayoutParams lp =
                (ViewGroup.MarginLayoutParams) headerView.getLayoutParams();
        int expectedMargin =
                TabVerticalViewBinder.getCollapsedChildMarginStart(headerView.getContext());
        assertEquals(expectedMargin, lp.getMarginStart());
    }

    @Test
    @SmallTest
    public void testBindTabGroupHeader_RailExpanded() {
        ViewGroup headerView = inflateGroupHeaderView();
        headerView.setLayoutParams(
                new ViewGroup.MarginLayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        TextView titleView = headerView.findViewById(R.id.group_title);

        mModel.set(TabProperties.TITLE, "My Group");
        mModel.set(TabProperties.RAIL_COLLAPSE_STATE, RailCollapseState.EXPANDED);

        TabVerticalViewBinder.bindTabGroupHeader(
                mModel, headerView, TabProperties.RAIL_COLLAPSE_STATE);

        assertEquals(ViewGroup.LayoutParams.MATCH_PARENT, headerView.getLayoutParams().width);
        assertEquals(ViewGroup.LayoutParams.WRAP_CONTENT, headerView.getLayoutParams().height);
        assertEquals(View.VISIBLE, titleView.getVisibility());
        assertEquals("My Group", titleView.getText());

        // Verify padding is 0
        ViewGroup.MarginLayoutParams lp =
                (ViewGroup.MarginLayoutParams) headerView.getLayoutParams();
        assertEquals(0, lp.getMarginStart());
    }

    @Test
    @SmallTest
    public void testIconPriorities_RailCollapsed() {
        // Setup favicon fetcher
        mModel.set(TabProperties.FAVICON_FETCHER, mFaviconFetcher);

        // Setup all other icons to be active
        mModel.set(TabProperties.IS_LOADING, true);
        mModel.set(TabProperties.MEDIA_INDICATOR, MediaState.AUDIBLE);
        mModel.set(
                TabProperties.ACTOR_UI_STATE,
                new UiTabState(0, null, null, TabIndicatorStatus.DYNAMIC, false));
        TabActionButtonData actionButtonData =
                new TabActionButtonData(TabActionButtonType.CLOSE, mCloseListener);
        mModel.set(TabProperties.TAB_ACTION_BUTTON_DATA, actionButtonData);
        mModel.set(TabProperties.IS_SELECTED, true);
        mModel.set(TabProperties.RAIL_COLLAPSE_STATE, RailCollapseState.COLLAPSED);

        // Bind all properties
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.RAIL_COLLAPSE_STATE);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.FAVICON_FETCHER);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_LOADING);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.MEDIA_INDICATOR);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.ACTOR_UI_STATE);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.TAB_ACTION_BUTTON_DATA);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_SELECTED);

        View spinner = mItemView.findViewById(R.id.tab_loading_spinner);

        // Hover to show close button
        MotionEvent hoverEnterEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 0f, 0f, 0);
        hoverEnterEvent.setSource(InputDevice.SOURCE_MOUSE);
        mItemView.dispatchGenericMotionEvent(hoverEnterEvent);

        // --- Priority 1: Action Button ---
        assertEquals(View.VISIBLE, mCloseButton.getVisibility());
        assertNotEquals(View.VISIBLE, mActuationSparkView.getVisibility());
        assertNotEquals(View.VISIBLE, mMediaIndicatorView.getVisibility());
        assertNotEquals(View.VISIBLE, spinner.getVisibility());
        assertNotEquals(View.VISIBLE, mFaviconView.getVisibility());

        // --- Priority 2: AI Indicator ---
        // Un-hover to hide close button
        MotionEvent hoverExitEvent =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_EXIT, 0f, 0f, 0);
        hoverExitEvent.setSource(InputDevice.SOURCE_MOUSE);
        mItemView.dispatchGenericMotionEvent(hoverExitEvent);

        assertNotEquals(View.VISIBLE, mCloseButton.getVisibility());
        assertEquals(View.VISIBLE, mActuationSparkView.getVisibility());
        assertNotEquals(View.VISIBLE, mMediaIndicatorView.getVisibility());
        assertNotEquals(View.VISIBLE, spinner.getVisibility());
        assertNotEquals(View.VISIBLE, mFaviconView.getVisibility());

        // --- Priority 3: Media Indicator ---
        // Disable AI
        mModel.set(
                TabProperties.ACTOR_UI_STATE,
                new UiTabState(0, null, null, TabIndicatorStatus.NONE, false));
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.ACTOR_UI_STATE);

        assertNotEquals(View.VISIBLE, mCloseButton.getVisibility());
        assertNotEquals(View.VISIBLE, mActuationSparkView.getVisibility());
        assertEquals(View.VISIBLE, mMediaIndicatorView.getVisibility());
        assertNotEquals(View.VISIBLE, spinner.getVisibility());
        assertNotEquals(View.VISIBLE, mFaviconView.getVisibility());

        // --- Priority 4: Loading Spinner ---
        // Disable Media
        mModel.set(TabProperties.MEDIA_INDICATOR, MediaState.NONE);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.MEDIA_INDICATOR);

        assertNotEquals(View.VISIBLE, mCloseButton.getVisibility());
        assertNotEquals(View.VISIBLE, mActuationSparkView.getVisibility());
        assertNotEquals(View.VISIBLE, mMediaIndicatorView.getVisibility());
        assertEquals(View.VISIBLE, spinner.getVisibility());
        assertNotEquals(View.VISIBLE, mFaviconView.getVisibility());

        // --- Priority 5: Favicon ---
        // Disable Loading
        mModel.set(TabProperties.IS_LOADING, false);
        TabVerticalViewBinder.bindTab(mModel, mItemView, TabProperties.IS_LOADING);

        assertNotEquals(View.VISIBLE, mCloseButton.getVisibility());
        assertNotEquals(View.VISIBLE, mActuationSparkView.getVisibility());
        assertNotEquals(View.VISIBLE, mMediaIndicatorView.getVisibility());
        assertNotEquals(View.VISIBLE, spinner.getVisibility());
        assertEquals(View.VISIBLE, mFaviconView.getVisibility());
    }

    private ViewGroup inflatePinnedTabView() {
        return (ViewGroup)
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.vertical_tab_pinned_item, null, false);
    }

    private ViewGroup inflateGroupHeaderView() {
        return (ViewGroup)
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.vertical_tab_group_header, null, false);
    }

    private void assertIndicatorStateAndDescription(
            View indicatorView,
            View parentView,
            int expectedVisibility,
            String expectedContentDescription) {
        assertEquals(expectedVisibility, indicatorView.getVisibility());
        assertEquals(
                expectedContentDescription,
                parentView.getContentDescription() != null
                        ? parentView.getContentDescription().toString()
                        : null);
    }
}
