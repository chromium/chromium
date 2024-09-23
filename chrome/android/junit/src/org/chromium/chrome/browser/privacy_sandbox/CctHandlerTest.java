// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CctHandlerTest {
    @Test
    public void testOpenUrlInCct() {
        Context context = mock(Context.class);

        String testUrl = "https://www.example.com";
        Intent expectedIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(testUrl));
        expectedIntent.setPackage(context.getPackageName());
        expectedIntent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());

        // 2. Call the Method
        Intent actualIntent = new CctHandler(context).prepareIntent(testUrl).getIntent();

        // 3. Assertions
        assertNotNull(actualIntent);
        assertEquals(Intent.ACTION_VIEW, actualIntent.getAction());
        assertEquals(Uri.parse(testUrl), actualIntent.getData());
        assertEquals(context.getPackageName(), actualIntent.getPackage());
        assertTrue(actualIntent.hasExtra(Browser.EXTRA_APPLICATION_ID));
    }
}
