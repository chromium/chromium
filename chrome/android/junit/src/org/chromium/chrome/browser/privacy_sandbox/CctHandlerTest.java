// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CctHandlerTest {
    @Rule public MockitoRule rule = MockitoJUnit.rule();

    @Mock private Context mContext;

    private CctHandler mCctHandler;

    private static final String TEST_URL = "https://www.example.com";

    @Before
    public void setup() {
        mCctHandler = new CctHandler(mContext);
    }

    @Test
    public void testPrepareIntentSuccess() {
        Intent expectedIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(TEST_URL));
        expectedIntent.setPackage(mContext.getPackageName());
        expectedIntent.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());

        Intent actualIntent = mCctHandler.prepareIntent(TEST_URL).getIntent();

        assertNotNull(actualIntent);
        assertEquals(Intent.ACTION_VIEW, actualIntent.getAction());
        assertEquals(Uri.parse(TEST_URL), actualIntent.getData());
        assertEquals(mContext.getPackageName(), actualIntent.getPackage());
        assertTrue(actualIntent.hasExtra(Browser.EXTRA_APPLICATION_ID));
    }

    @Test
    public void testOpenUrlInCctFailure_nullIntent() {
        assertThrows(AssertionError.class, () -> mCctHandler.openUrlInCct());
    }

    @Test
    public void testOpenUrlInCctFailure_untrustedPackage() {
        doReturn("untrusted_package").when(mContext).getPackageName();

        assertThrows(
                AssertionError.class, () -> mCctHandler.prepareIntent(TEST_URL).openUrlInCct());
    }

    @Test
    public void testOpenUrlInCctSuccess() {
        doReturn(ContextUtils.getApplicationContext().getPackageName())
                .when(mContext)
                .getPackageName();

        CctHandler testCctHandler = mCctHandler.prepareIntent(TEST_URL).openUrlInCct();

        assertNotNull(testCctHandler);
    }
}
