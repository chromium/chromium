// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup.LayoutParams;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBarControlLayout.ControlLayoutParams;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests for InfoBarControlLayout.  This suite doesn't check for specific details, like margins
 * paddings, and instead focuses on whether controls are placed correctly.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class InfoBarControlLayoutTest {
    private static final int SWITCH_ID_1 = 1;
    private static final int SWITCH_ID_2 = 2;
    private static final int SWITCH_ID_3 = 3;
    private static final int SWITCH_ID_4 = 4;
    private static final int SWITCH_ID_5 = 5;
    private static final int INFOBAR_WIDTH = 3200;

    private Context mContext;

    @Rule
    public UiThreadTestRule mRule = new UiThreadTestRule();

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
        mContext.setTheme(R.style.Theme_Chromium_WithWindowAnimation);
    }

    /**
     * A small control on the last line takes up the full width.
     */
    @Test
    @SmallTest
    @UiThreadTest
    public void testOneSmallControlTakesFullWidth() {
        InfoBarControlLayout layout = new InfoBarControlLayout(mContext);
        layout.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        View smallSwitch = layout.addSwitch(0, 0, "A", SWITCH_ID_1, false);

        // Trigger the measurement algorithm.
        int parentWidthSpec =
                MeasureSpec.makeMeasureSpec(INFOBAR_WIDTH, MeasureSpec.AT_MOST);
        int parentHeightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        layout.measure(parentWidthSpec, parentHeightSpec);

        // Small control takes the full width of the layout because it's put on its own line.
        ControlLayoutParams params = InfoBarControlLayout.getControlLayoutParams(smallSwitch);
        Assert.assertEquals(0, params.top);
        Assert.assertEquals(0, params.start);
        Assert.assertEquals(2, params.columnsRequired);
        Assert.assertEquals(INFOBAR_WIDTH, smallSwitch.getMeasuredWidth());
    }

    /**
     * Tests the layout algorithm on a set of five controls, the second of which is a huge control
     * and takes up the whole line.  The other smaller controls try to pack themselves as tightly
     * as possible, strecthing out if necessary for aesthetics, resulting in a layout like this:
     *
     * -------------------------
     * | A (small)             |
     * -------------------------
     * | B (big)               |
     * -------------------------
     * | C (small) | D (small) |
     * -------------------------
     * | E (small)             |
     * -------------------------
     */
    @Test
    @SmallTest
    @UiThreadTest
    public void testComplexSwitchLayout() {
        // Add five controls to the layout.
        InfoBarControlLayout layout = new InfoBarControlLayout(mContext);
        layout.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        View switch1 = layout.addSwitch(0, 0, "A", SWITCH_ID_1, false);
        View switch2 = layout.addSwitch(0, 0, "B", SWITCH_ID_2, false);
        View switch3 = layout.addSwitch(0, 0, "C", SWITCH_ID_3, false);
        View switch4 = layout.addSwitch(0, 0, "D", SWITCH_ID_4, false);
        View switch5 = layout.addSwitch(0, 0, "E", SWITCH_ID_4, false);

        // Make the second control require the full layout width.
        switch2.setMinimumWidth(INFOBAR_WIDTH);

        // Trigger the measurement algorithm.
        int parentWidthSpec =
                MeasureSpec.makeMeasureSpec(INFOBAR_WIDTH, MeasureSpec.AT_MOST);
        int parentHeightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        layout.measure(parentWidthSpec, parentHeightSpec);

        ControlLayoutParams params1 = InfoBarControlLayout.getControlLayoutParams(switch1);
        ControlLayoutParams params2 = InfoBarControlLayout.getControlLayoutParams(switch2);
        ControlLayoutParams params3 = InfoBarControlLayout.getControlLayoutParams(switch3);
        ControlLayoutParams params4 = InfoBarControlLayout.getControlLayoutParams(switch4);
        ControlLayoutParams params5 = InfoBarControlLayout.getControlLayoutParams(switch5);

        // Small control takes the full width of the layout because the next one doesn't fit.
        Assert.assertEquals(0, params1.top);
        Assert.assertEquals(0, params1.start);
        Assert.assertEquals(2, params1.columnsRequired);
        Assert.assertEquals(INFOBAR_WIDTH, switch1.getMeasuredWidth());

        // Big control gets shunted onto the next row and takes up the whole space.
        Assert.assertTrue(params2.top > switch1.getMeasuredHeight());
        Assert.assertEquals(0, params2.start);
        Assert.assertEquals(2, params2.columnsRequired);
        Assert.assertEquals(INFOBAR_WIDTH, switch2.getMeasuredWidth());

        // Small control gets placed onto the next line and takes only half the width.
        int bottomOfSwitch2 = params2.top + switch2.getMeasuredHeight();
        Assert.assertTrue(params3.top > bottomOfSwitch2);
        Assert.assertEquals(0, params3.start);
        Assert.assertEquals(1, params3.columnsRequired);
        Assert.assertTrue(switch3.getMeasuredWidth() < INFOBAR_WIDTH);

        // Small control gets placed next to the previous small control.
        Assert.assertEquals(params3.top, params4.top);
        Assert.assertTrue(params4.start > switch3.getMeasuredWidth());
        Assert.assertEquals(1, params4.columnsRequired);
        Assert.assertTrue(switch4.getMeasuredWidth() < INFOBAR_WIDTH);

        // Last small control has no room left and gets put on its own line, taking the full width.
        int bottomOfSwitch4 = params4.top + switch4.getMeasuredHeight();
        Assert.assertTrue(params5.top > bottomOfSwitch4);
        Assert.assertEquals(0, params5.start);
        Assert.assertEquals(2, params5.columnsRequired);
        Assert.assertEquals(INFOBAR_WIDTH, switch5.getMeasuredWidth());
    }

    /**
     * Tests that the message is always the full width of the layout.
     */
    @Test
    @SmallTest
    @UiThreadTest
    public void testFullWidthMessageControl() {
        // Add two controls to the layout.  The main message automatically requests the full width.
        InfoBarControlLayout layout = new InfoBarControlLayout(mContext);
        layout.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        View view1 = layout.addMainMessage("A");
        View view2 = layout.addSwitch(0, 0, "B", SWITCH_ID_2, false);

        // Trigger the measurement algorithm.
        int parentWidthSpec =
                MeasureSpec.makeMeasureSpec(INFOBAR_WIDTH, MeasureSpec.AT_MOST);
        int parentHeightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        layout.measure(parentWidthSpec, parentHeightSpec);

        ControlLayoutParams params1 = InfoBarControlLayout.getControlLayoutParams(view1);
        ControlLayoutParams params2 = InfoBarControlLayout.getControlLayoutParams(view2);

        // Main message takes up the full space.
        Assert.assertEquals(0, params1.top);
        Assert.assertEquals(0, params1.start);
        Assert.assertEquals(2, params1.columnsRequired);
        Assert.assertEquals(INFOBAR_WIDTH, view1.getMeasuredWidth());

        // Small control gets shunted onto the next row.
        Assert.assertTrue(params2.top > view1.getMeasuredHeight());
        Assert.assertEquals(0, params2.start);
        Assert.assertEquals(2, params2.columnsRequired);
        Assert.assertEquals(INFOBAR_WIDTH, view2.getMeasuredWidth());
    }
}
