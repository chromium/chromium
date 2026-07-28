// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.InputDevice;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.LinearLayout;

import androidx.test.filters.SmallTest;

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
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.tab_ui.R;

/** Unit tests for {@link VerticalTabRailLayout}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class VerticalTabRailLayoutUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<Integer> mMockHoverListener;

    private VerticalTabRailLayout mRailLayout;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mRailLayout =
                (VerticalTabRailLayout)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.vertical_tab_layout, null, false);
        mRailLayout.setExpandOrCollapseOnHoverListener(mMockHoverListener);
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
}
