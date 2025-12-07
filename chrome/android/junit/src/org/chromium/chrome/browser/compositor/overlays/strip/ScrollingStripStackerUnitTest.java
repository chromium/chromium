// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.LocalizationUtils;

import java.util.Arrays;
import java.util.List;

/** Tests for {@link ScrollingStripStacker}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class ScrollingStripStackerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final float OFFSET_Y = 2;
    private static final float WIDTH = 100;
    private static final float FIRST_VIEW_IDEAL_X = -WIDTH - 1;

    private final ScrollingStripStacker mTarget = new ScrollingStripStacker();
    @Mock private StripLayoutTab mView1;
    @Mock private StripLayoutGroupTitle mView2;
    @Mock private StripLayoutTab mView3;
    @Mock private StripLayoutTab mView4;
    @Mock private StripLayoutGroupTitle mView5;
    private StripLayoutView[] mInput;

    @Before
    public void setUp() {
        LocalizationUtils.setRtlForTesting(false);
        mInput = new StripLayoutView[] {mView1, mView2, mView3, mView4, mView5};
    }

    private void setupViews() {
        // First and last tab out of visible area. Offset_x is 0.
        float idealX = FIRST_VIEW_IDEAL_X;
        for (StripLayoutView view : mInput) {
            when(view.getIdealX()).thenReturn(idealX);
            when(view.getOffsetY()).thenReturn(OFFSET_Y);
            if (view instanceof StripLayoutTab) {
                when(view.getWidth()).thenReturn(WIDTH);
                when(view.getDrawX()).thenReturn(idealX);
            } else if (view instanceof StripLayoutGroupTitle title) {
                when(title.getPaddedX()).thenReturn(idealX);
                when(title.getPaddedWidth()).thenReturn(WIDTH);
            }
            idealX += WIDTH;
        }
    }

    private void verifyViews(List<StripLayoutView> expectedHiddenViews) {
        // First and last tab out of visible area.
        float idealX = FIRST_VIEW_IDEAL_X;
        for (StripLayoutView view : mInput) {
            verify(view).setDrawY(OFFSET_Y);
            verify(view).setDrawX(idealX);
            if (expectedHiddenViews.contains(view)) {
                verify(view).setVisible(false);
            } else {
                view.setVisible(true);
            }
            idealX += WIDTH;
        }
    }

    @Test
    public void testPushDrawPropertiesToViews() {
        setupViews();
        mTarget.pushDrawPropertiesToViews(mInput, 0, 2 * WIDTH);
        // First and last view hidden based on initial positions.
        verifyViews(Arrays.asList(mView1, mView5));
    }

    @Test
    public void testPushDrawPropertiesToViewsInRtl() {
        LocalizationUtils.setRtlForTesting(true);
        setupViews();
        mTarget.pushDrawPropertiesToViews(mInput, 0, 2 * WIDTH);
        // First and last view hidden based on initial positions.
        verifyViews(Arrays.asList(mView1, mView5));
    }

    @Test
    public void testPushDrawPropertiesToViewsWithXOffset() {
        setupViews();
        mTarget.pushDrawPropertiesToViews(mInput, WIDTH, 2 * WIDTH);

        // Move the window with xOffset = TAB_WIDTH, will make up to TAB_2 invisible, but rest
        // visible.
        verifyViews(Arrays.asList(mView1, mView2));
    }

    @Test
    public void testPushDrawPropertiesToViewsWithPartialXOffset() {
        setupViews();
        mTarget.pushDrawPropertiesToViews(mInput, WIDTH / 2, 2 * WIDTH);

        // Move the window with xOffset = TAB_WIDTH / 2, will make both up to TAB_2 and TAB_4
        // partially invisible. TAB_5 will still be invisible in this case.
        verifyViews(Arrays.asList(mView1, mView5));
    }
}
