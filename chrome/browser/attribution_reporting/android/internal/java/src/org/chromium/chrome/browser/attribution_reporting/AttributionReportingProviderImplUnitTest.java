// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;

/**
 * Unit tests for the AttributionReportingProviderImpl
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AttributionReportingProviderImplUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private ContentProviderClient mContentProviderClient;
    private Uri mContentUri;

    @Before
    public void setUp() {
        Context context = ContextUtils.getApplicationContext();
        String authority = context.getPackageName() + ".AttributionReporting/";
        mContentUri = Uri.parse("content://" + authority);
        mContentProviderClient =
                context.getContentResolver().acquireContentProviderClient(mContentUri);
    }

    @Test
    @SmallTest
    @Features.DisableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
    public void testInvalidAttribution_Disabled() throws Exception {
        ContentValues values = new ContentValues();
        values.put("badkey", "value");
        Assert.assertNull(mContentProviderClient.insert(mContentUri, values));
    }

    @Test
    @SmallTest
    @Features.DisableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
    public void testValidAttribution_Disabled() throws Exception {
        ContentValues values = AttributionReportingProviderImplTest.makeContentValues(
                "event", "destination", "reportTo", 12345L);
        Assert.assertNull(mContentProviderClient.insert(mContentUri, values));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
    public void testInvalidAttribution_Enabled() throws Exception {
        ContentValues values = new ContentValues();
        values.put("badkey", "value");
        Exception exception = null;
        try {
            mContentProviderClient.insert(mContentUri, values);
        } catch (IllegalArgumentException e) {
            exception = e;
        }
        Assert.assertNotNull(exception);
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
    public void testQuery() throws Exception {
        Exception exception = null;
        try {
            mContentProviderClient.query(mContentUri, null, null, null, null);
        } catch (UnsupportedOperationException e) {
            exception = e;
        }
        Assert.assertNotNull(exception);
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
    public void testDelete() throws Exception {
        Exception exception = null;
        try {
            mContentProviderClient.delete(mContentUri, null, null);
        } catch (UnsupportedOperationException e) {
            exception = e;
        }
        Assert.assertNotNull(exception);
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
    public void testUpdate() throws Exception {
        Exception exception = null;
        try {
            mContentProviderClient.update(mContentUri, null, null, null);
        } catch (UnsupportedOperationException e) {
            exception = e;
        }
        Assert.assertNotNull(exception);
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
    public void testGetType() throws Exception {
        Assert.assertNull(mContentProviderClient.getType(mContentUri));
    }
}
