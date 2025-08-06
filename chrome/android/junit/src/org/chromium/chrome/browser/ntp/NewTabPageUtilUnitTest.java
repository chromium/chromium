// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.VerticalDisplayStyle;
import org.chromium.components.omnibox.OmniboxFeatureList;

/** Unit tests for helper functions in {@link NewTabPage} and {@link NewTabPageLayout} classes. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NewTabPageUtilUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

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

    @Test
    @Features.DisableFeatures({OmniboxFeatureList.OMNIBOX_MOBILE_PARITY_UPDATE_V2})
    public void testIsInSingleUrlBarMode() {
        // Verifies isInSingleUrlBarMode() returns false on tablets.
        assertFalse(
                NewTabPage.isInSingleUrlBarMode(
                        /* isTablet= */ true, /* searchProviderHasLogo= */ false));
        assertFalse(
                NewTabPage.isInSingleUrlBarMode(
                        /* isTablet= */ true, /* searchProviderHasLogo= */ true));
        // Verifies isInSingleUrlBarMode() depends on searchProviderHasLogo.
        assertFalse(
                NewTabPage.isInSingleUrlBarMode(
                        /* isTablet= */ false, /* searchProviderHasLogo= */ false));
        assertTrue(
                NewTabPage.isInSingleUrlBarMode(
                        /* isTablet= */ false, /* searchProviderHasLogo= */ true));
    }

    @Test
    @Features.EnableFeatures({OmniboxFeatureList.OMNIBOX_MOBILE_PARITY_UPDATE_V2})
    public void testIsInSingleUrlBarMode_OmniboxMobileParityUpdateV2Enabled() {
        // Verifies isInSingleUrlBarMode() returns false on tablets.
        assertFalse(
                NewTabPage.isInSingleUrlBarMode(
                        /* isTablet= */ true, /* searchProviderHasLogo= */ false));
        assertFalse(
                NewTabPage.isInSingleUrlBarMode(
                        /* isTablet= */ true, /* searchProviderHasLogo= */ true));

        // Verifies that isInSingleUrlBarMode() return true without depending on
        // searchProviderHasLogo.
        assertTrue(
                NewTabPage.isInSingleUrlBarMode(
                        /* isTablet= */ false, /* searchProviderHasLogo= */ false));
        assertTrue(
                NewTabPage.isInSingleUrlBarMode(
                        /* isTablet= */ false, /* searchProviderHasLogo= */ true));
    }
}
