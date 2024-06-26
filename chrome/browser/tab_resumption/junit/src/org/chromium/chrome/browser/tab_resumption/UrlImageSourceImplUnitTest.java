// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab_ui.TabContentManager;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UrlImageSourceImplUnitTest {
    @Mock private TabContentManager mTabContentManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @SmallTest
    public void testCreate() {
        Context context = ApplicationProvider.getApplicationContext();
        UrlImageSourceImpl urlImageSource = new UrlImageSourceImpl(context, mTabContentManager);
        Assert.assertNotNull(urlImageSource.createThumbnailProvider());
        Assert.assertNotNull(urlImageSource.createIconGenerator());
    }
}
