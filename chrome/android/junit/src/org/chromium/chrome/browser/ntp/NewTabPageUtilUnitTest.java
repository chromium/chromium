// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.VerticalDisplayStyle;

/** Unit tests for helper functions in {@link NewTabPage} and {@link NewTabPageLayout} classes. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NewTabPageUtilUnitTest {

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testIsInNarrowWindowOnTablet() {
        UiConfig uiConfig = Mockito.mock(UiConfig.class);

        UiConfig.DisplayStyle displayStyleWide =
                new DisplayStyle(HorizontalDisplayStyle.WIDE, VerticalDisplayStyle.REGULAR);
        when(uiConfig.getCurrentDisplayStyle()).thenReturn(displayStyleWide);

        assertFalse(
                "It isn't a narrow window on tablet when displayStyleWide =="
                        + " HorizontalDisplayStyle.WIDE.",
                NewTabPageLayout.isInNarrowWindowOnTablet(true, uiConfig));

        UiConfig.DisplayStyle displayStyleRegular =
                new DisplayStyle(HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR);
        when(uiConfig.getCurrentDisplayStyle()).thenReturn(displayStyleRegular);
        assertFalse(
                "It isn't a narrow window on tablet when |isTablet| is false.",
                NewTabPageLayout.isInNarrowWindowOnTablet(false, uiConfig));

        assertTrue(NewTabPageLayout.isInNarrowWindowOnTablet(true, uiConfig));
    }
}
