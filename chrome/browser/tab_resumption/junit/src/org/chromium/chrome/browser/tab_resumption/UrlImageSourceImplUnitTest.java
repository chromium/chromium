// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab_ui.TabContentManager;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UrlImageSourceImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabContentManager mTabContentManager;

    @Test
    @SmallTest
    public void testCreate() {
        Context context = ApplicationProvider.getApplicationContext();
        UrlImageSourceImpl urlImageSource = new UrlImageSourceImpl(context, mTabContentManager);
        Assert.assertNotNull(urlImageSource.createThumbnailProvider());
        Assert.assertNotNull(urlImageSource.createIconGenerator());
    }
}
