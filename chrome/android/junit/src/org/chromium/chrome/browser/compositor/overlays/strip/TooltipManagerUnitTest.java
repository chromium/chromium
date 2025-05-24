// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;
import static org.robolectric.Robolectric.buildActivity;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;

/** Unit tests for {@link TooltipManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, qualifiers = "w640dp-h360dp")
public class TooltipManagerUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private CompositorButton mButton;

    private TooltipManager mTooltipManager;
    private FrameLayout mTooltipView;
    private Context mContext;

    @Before
    public void setUp() {
        Activity activity = buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mTooltipView =
                (FrameLayout)
                        activity.getLayoutInflater()
                                .inflate(R.layout.tab_strip_tooltip_holder, null);
        mTooltipManager = new TooltipManager(mTooltipView, () -> 0f);

        mContext = mTooltipView.getContext();
        mContext.getResources().getDisplayMetrics().density = 1f;
    }

    @Test
    public void showAndHide() {
        when(mButton.getAccessibilityDescription()).thenReturn("The button description");
        when(mButton.getDrawX()).thenReturn(100f);
        when(mButton.getDrawY()).thenReturn(100f);
        when(mButton.getWidth()).thenReturn(100f);
        when(mButton.getHeight()).thenReturn(100f);

        mTooltipManager.showImmediatelyFor(mButton);
        assertEquals(
                "|mTooltipView| should be visible.", View.VISIBLE, mTooltipView.getVisibility());

        mTooltipManager.hideImmediately();
        assertEquals("|mTooltipView| should be gone.", View.GONE, mTooltipView.getVisibility());
    }

    @Test
    public void correctText() {
        when(mButton.getAccessibilityDescription()).thenReturn("The button description");
        when(mButton.getDrawX()).thenReturn(100f);
        when(mButton.getDrawY()).thenReturn(100f);
        when(mButton.getWidth()).thenReturn(100f);
        when(mButton.getHeight()).thenReturn(100f);

        mTooltipManager.showImmediatelyFor(mButton);

        assertEquals(
                "|mTooltipView| should be visible.", View.VISIBLE, mTooltipView.getVisibility());
        assertEquals(
                "Tooltip text is incorrect.",
                mButton.getAccessibilityDescription(),
                ((TextView) mTooltipView.findViewById(R.id.tooltip_label)).getText());
    }

    private void testCorrectPosition() {
        assertEquals(
                "|mTooltipView| should be visible.", View.VISIBLE, mTooltipView.getVisibility());
        assertTrue(
                "|mTooltipView| should not cross the left window border.",
                mTooltipView.getX() >= 0f);
        assertTrue(
                "|mTooltipView| should not cross the right window border.",
                mTooltipView.getX() + mTooltipView.getWidth() <= 640f);
        assertTrue(
                "|mTooltipView| should not cross the top window border.",
                mTooltipView.getY() >= 0f);
        assertTrue(
                "|mTooltipView| should not cross the bottom window border.",
                mTooltipView.getY() + mTooltipView.getHeight() <= 360f);
    }

    @Test
    public void tooltipStaysInTheWindowLeftBound() {
        when(mButton.getAccessibilityDescription()).thenReturn("A very long button description");
        when(mButton.getDrawX()).thenReturn(0f);
        when(mButton.getDrawY()).thenReturn(0f);
        when(mButton.getWidth()).thenReturn(10f);
        when(mButton.getHeight()).thenReturn(10f);

        mTooltipManager.showImmediatelyFor(mButton);

        testCorrectPosition();
    }

    @Test
    public void tooltipStaysInTheWindowRightBound() {
        when(mButton.getAccessibilityDescription()).thenReturn("A very long button description");
        when(mButton.getDrawX()).thenReturn(630f);
        when(mButton.getDrawY()).thenReturn(0f);
        when(mButton.getWidth()).thenReturn(10f);
        when(mButton.getHeight()).thenReturn(10f);

        mTooltipManager.showImmediatelyFor(mButton);

        testCorrectPosition();
    }
}
