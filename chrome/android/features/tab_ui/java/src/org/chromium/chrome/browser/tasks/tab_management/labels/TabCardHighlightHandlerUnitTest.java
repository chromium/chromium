// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.labels;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabCardHighlightState;

/** Unit tests for {@link TabCardHighlightHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabCardHighlightHandlerUnitTest {
    private Context mContext;
    private View mCardWrapper;
    private TabCardHighlightHandler mManager;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mCardWrapper = new View(mContext);
        mManager = new TabCardHighlightHandler(mCardWrapper);
    }

    @Test
    public void testMaybeAnimateForHighlightState_Highlighted() {
        mCardWrapper.setVisibility(View.GONE);

        mManager.maybeAnimateForHighlightState(
                TabCardHighlightState.HIGHLIGHTED, /* isIncognito= */ false);

        assertEquals(View.VISIBLE, mCardWrapper.getVisibility());
        assertNotNull(mCardWrapper.getBackground());
    }

    @Test
    public void testMaybeAnimateForHighlightState_ToBeHighlighted() {
        mManager.maybeAnimateForHighlightState(
                TabCardHighlightState.TO_BE_HIGHLIGHTED, /* isIncognito= */ false);

        RobolectricUtil.runAllBackgroundAndUi();

        assertNotNull(mCardWrapper.getBackground());
    }

    @Test
    public void testMaybeAnimateForHighlightState_NotHighlighted() {
        mCardWrapper.setBackground(new ColorDrawable(Color.RED));
        mCardWrapper.setAlpha(0.5f);

        mManager.maybeAnimateForHighlightState(
                TabCardHighlightState.NOT_HIGHLIGHTED, /* isIncognito= */ false);

        RobolectricUtil.runAllBackgroundAndUi();

        assertNull(mCardWrapper.getBackground());
        assertEquals(1f, mCardWrapper.getAlpha(), 0f);
    }

    @Test
    public void testClearHighlight() {
        mCardWrapper.setBackground(new ColorDrawable(Color.RED));

        mManager.clearHighlight();

        assertNull(mCardWrapper.getBackground());
        assertEquals(View.GONE, mCardWrapper.getVisibility());
    }
}
