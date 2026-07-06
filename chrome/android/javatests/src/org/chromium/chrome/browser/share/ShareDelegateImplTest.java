// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.url.GURL;

/** Tests (requiring native) of the ShareDelegateImpl. */
@Batch(Batch.PER_CLASS)
@RunWith(BaseJUnit4ClassRunner.class)
public class ShareDelegateImplTest {

    @Test
    @SmallTest
    public void testGetUrlToShare() {
        Context context = ApplicationProvider.getApplicationContext();
        Assert.assertEquals("", ShareDelegateImpl.getUrlToShare(GURL.emptyGURL(), context));

        final GURL httpsUrl = new GURL("https://blah.com");
        Assert.assertEquals(httpsUrl.getSpec(), ShareDelegateImpl.getUrlToShare(httpsUrl, context));
    }
}
