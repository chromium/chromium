// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshotprovider;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;

import android.net.Uri;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowContentResolver;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.io.FileNotFoundException;

/** Unit tests for {@link ScreenshotContentProviderImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowContentResolver.class})
public class ScreenshotContentProviderImplUnitTest {

    private ScreenshotContentProviderImpl mProvider;

    @Before
    public void setUp() {
        mProvider = new ScreenshotContentProviderImpl();
    }

    @Test
    public void testQueryReturnsNull() {
        assertNull(mProvider.query(Uri.EMPTY, null, null, null, null));
    }

    @Test
    public void testGetTypeReturnsNull() {
        assertNull(mProvider.getType(Uri.EMPTY));
    }

    @Test
    public void testInsertReturnsNull() {
        assertNull(mProvider.insert(Uri.EMPTY, null));
    }

    @Test
    public void testDeleteReturnsZero() {
        assertEquals(0, mProvider.delete(Uri.EMPTY, null, null));
    }

    @Test
    public void testUpdateReturnsZero() {
        assertEquals(0, mProvider.update(Uri.EMPTY, null, null, null));
    }

    @Test
    public void testOpenFileThrowsException() {
        assertThrows(FileNotFoundException.class, () -> mProvider.openFile(Uri.EMPTY, "r"));
    }
}
