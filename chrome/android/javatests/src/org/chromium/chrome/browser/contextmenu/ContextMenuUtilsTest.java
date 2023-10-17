// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.junit.Assert.assertEquals;

import android.webkit.URLUtil;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.blink_public.common.ContextMenuDataMediaType;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.url.GURL;

/** Unit tests for {@link ContextMenuUtils}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ContextMenuUtilsTest {
    private static final String sTitleText = "titleText";
    private static final String sLinkText = "linkText";
    private static final String sSrcUrl = "https://www.google.com/";

    @Test
    @SmallTest
    public void getTitle_hasTitleText() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        org.chromium.blink_public.common.ContextMenuDataMediaType.IMAGE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        sLinkText,
                        GURL.emptyGURL(),
                        new GURL(sSrcUrl),
                        sTitleText,
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        /* additionalNavigationParams= */ null);

        assertEquals(sTitleText, ContextMenuUtils.getTitle(params));
    }

    @Test
    @SmallTest
    public void getTitle_noTitleTextHasLinkText() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        ContextMenuDataMediaType.IMAGE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        sLinkText,
                        GURL.emptyGURL(),
                        new GURL(sSrcUrl),
                        "",
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        /* additionalNavigationParams= */ null);

        assertEquals(sLinkText, ContextMenuUtils.getTitle(params));
    }

    @Test
    @SmallTest
    public void getTitle_noTitleTextOrLinkText() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        ContextMenuDataMediaType.IMAGE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        new GURL(sSrcUrl),
                        "",
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        /* additionalNavigationParams= */ null);

        assertEquals(URLUtil.guessFileName(sSrcUrl, null, null), ContextMenuUtils.getTitle(params));
    }

    @Test
    @SmallTest
    public void getTitle_noShareParams() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        ContextMenuDataMediaType.NONE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        /* additionalNavigationParams= */ null);

        assertEquals("", ContextMenuUtils.getTitle(params));
    }
}
