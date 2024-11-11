// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchUtils.USE_LARGE_FAVICON;

import android.content.Context;
import android.content.res.Resources;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;

import java.io.File;

/** Unit tests for AuxiliarySearchUtils. */
@RunWith(BaseRobolectricTestRunner.class)
public class AuxiliarySearchUtilsUnitTest {
    @Test
    public void testGetTabDonateFile() {
        Context context = ContextUtils.getApplicationContext();
        File file = AuxiliarySearchUtils.getTabDonateFile(context);
        assertEquals("tabs_donate", file.getName());
    }

    @Test
    public void testGetFaviconSize_small() {
        Resources resources = ContextUtils.getApplicationContext().getResources();
        int faviconSizeSmall =
                resources.getDimensionPixelSize(R.dimen.auxiliary_search_favicon_size_small);

        assertEquals(faviconSizeSmall, AuxiliarySearchUtils.getFaviconSize(resources));
    }

    @Test
    @EnableFeatures("AndroidAppIntegrationWithFavicon:use_large_favicon/true")
    public void testGetFaviconSize() {
        assertTrue(USE_LARGE_FAVICON.getValue());

        Resources resources = ContextUtils.getApplicationContext().getResources();
        int faviconSize = resources.getDimensionPixelSize(R.dimen.auxiliary_search_favicon_size);

        assertEquals(faviconSize, AuxiliarySearchUtils.getFaviconSize(resources));
    }
}
