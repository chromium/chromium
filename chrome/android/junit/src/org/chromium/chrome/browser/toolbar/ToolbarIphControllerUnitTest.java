// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;

@RunWith(BaseRobolectricTestRunner.class)
public class ToolbarIphControllerUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock UserEducationHelper mEducationHelper;
    private @Mock View mAnchorView;

    private Context mContext;
    private ToolbarIphController mToolbarIphController;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mToolbarIphController = new ToolbarIphController(mContext, mEducationHelper);
    }

    @Test
    public void testShowPriceDropIph() {
        int y_inset = mContext.getResources().getDimensionPixelSize(R.dimen.toolbar_iph_y_inset);
        mToolbarIphController.showPriceDropIph(mAnchorView);

        HighlightParams expectedParams = new HighlightParams(HighlightShape.CIRCLE);
        expectedParams.setBoundsRespectPadding(true);
        Rect expectedRect = new Rect(0, 0, 0, -y_inset);

        ArgumentCaptor<IphCommand> captor = ArgumentCaptor.forClass(IphCommand.class);
        verify(mEducationHelper).requestShowIph(captor.capture());

        IphCommand cmd = captor.getValue();
        assertEquals(R.string.price_drop_spotted_iph, cmd.accessibilityStringId);
        assertEquals(R.string.price_drop_spotted_iph, cmd.stringId);
        assertEquals(mAnchorView, cmd.anchorView);
        assertEquals(expectedRect, cmd.insetRect);
        assertEquals(HighlightShape.CIRCLE, cmd.highlightParams.getShape());
        assertTrue(cmd.dismissOnTouch);
    }
}
