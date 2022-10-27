// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.ComponentName;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.ManifestMetadataUtil;
import org.chromium.android_webview.test.util.ManifestMetadataMockApplicationContext;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;

import java.util.Collections;
import java.util.Set;

/**
 * Test for {@link ManifestMetadataUtil}
 */
@RunWith(AwJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ManifestMetadataUtilTest {
    /*
     * Metadata keys are deliberately not referencing their production declarations to protect
     * against accidental changes.
     */
    private static final String METRICS_OPT_OUT_METADATA_NAME =
            "android.webkit.WebView.MetricsOptOut";
    private static final String SAFE_BROWSING_OPT_IN_METADATA_NAME =
            "android.webkit.WebView.EnableSafeBrowsing";
    private static final String METADATA_HOLDER_SERVICE_NAME =
            "android.webkit.MetaDataHolderService";
    private static final String XRW_ALLOWLIST_METADATA_NAME =
            "REQUESTED_WITH_HEADER_ORIGIN_ALLOW_LIST";

    private static final int XRW_ALLOWLIST_RESOURCE_ID = 0xcafebabe;
    private static final String[] XRW_ALLOWLIST = {"*.example.com", "*.google.com"};
    private static final int INVALID_XRW_ALLOWLIST_RESOURCE_ID = 0xdead;

    private ManifestMetadataMockApplicationContext mContext;
    private ComponentName mMetadataServiceName;

    @Before
    public void setUp() throws Exception {
        mContext = new ManifestMetadataMockApplicationContext(
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext());
        ContextUtils.initApplicationContextForTests(mContext);
        mMetadataServiceName = new ComponentName(mContext, METADATA_HOLDER_SERVICE_NAME);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Manifest"})
    public void testMetricsCollectionOptOut() throws Exception {
        var bundle = new Bundle();
        bundle.putBoolean(METRICS_OPT_OUT_METADATA_NAME, true);
        mContext.putServiceMetadata(mContext.getPackageName(), bundle);
        Assert.assertTrue(ManifestMetadataUtil.isAppOptedOutFromMetricsCollection());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Manifest"})
    public void testMetricsCollectionDefault() throws Exception {
        Assert.assertFalse(ManifestMetadataUtil.isAppOptedOutFromMetricsCollection());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Manifest"})
    public void testSafeBrowsingDefault() throws Exception {
        Assert.assertNull(ManifestMetadataUtil.getSafeBrowsingAppOptInPreference());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Manifest"})
    public void testSafeBrowsingOptIn() throws Exception {
        var bundle = new Bundle();
        bundle.putBoolean(SAFE_BROWSING_OPT_IN_METADATA_NAME, true);
        mContext.putServiceMetadata(mContext.getPackageName(), bundle);
        var preference = ManifestMetadataUtil.getSafeBrowsingAppOptInPreference();
        Assert.assertNotNull(preference);
        Assert.assertTrue(preference);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Manifest"})
    public void testSafeBrowsingOptOut() throws Exception {
        var bundle = new Bundle();
        bundle.putBoolean(SAFE_BROWSING_OPT_IN_METADATA_NAME, false);
        mContext.putServiceMetadata(mContext.getPackageName(), bundle);
        var preference = ManifestMetadataUtil.getSafeBrowsingAppOptInPreference();
        Assert.assertNotNull(preference);
        Assert.assertFalse(preference);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Manifest"})
    public void testGetStringListFromServiceBundle() throws Exception {
        var bundle = new Bundle();
        bundle.putInt(XRW_ALLOWLIST_METADATA_NAME, XRW_ALLOWLIST_RESOURCE_ID);
        mContext.putServiceMetadata(mMetadataServiceName, bundle);
        mContext.putStringArrayResource(XRW_ALLOWLIST_RESOURCE_ID, XRW_ALLOWLIST);

        Set<String> allowList = ManifestMetadataUtil.getXRequestedWithAllowList();
        Assert.assertEquals(Set.of(XRW_ALLOWLIST), allowList);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Manifest"})
    public void testExceptionIfServiceMetadataNotFound() throws Exception {
        Assert.assertEquals(
                Collections.emptySet(), ManifestMetadataUtil.getXRequestedWithAllowList());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Manifest"})
    public void testNoErrorsIfXrwAllowListKeyNotSet() throws Exception {
        mContext.putServiceMetadata(mMetadataServiceName, new Bundle());

        Assert.assertEquals(
                Collections.emptySet(), ManifestMetadataUtil.getXRequestedWithAllowList());
    }

    @Test(expected = IllegalArgumentException.class)
    @SmallTest
    @Feature({"AndroidWebView", "Manifest"})
    public void testExceptionIfInvalidXrwAllowListResourceId() throws Exception {
        var bundle = new Bundle();
        bundle.putInt(XRW_ALLOWLIST_METADATA_NAME, INVALID_XRW_ALLOWLIST_RESOURCE_ID);

        mContext.putServiceMetadata(mMetadataServiceName, bundle);

        mContext.putStringArrayResource(XRW_ALLOWLIST_RESOURCE_ID, XRW_ALLOWLIST);

        ManifestMetadataUtil.getXRequestedWithAllowList();
        Assert.fail("An IllegalArgumentException should have been thrown");
    }

    @Test(expected = IllegalArgumentException.class)
    @SmallTest
    @Feature({"AndroidWebView", "Manifest"})
    public void testNoErrorsIfMetadataValueIsNotInt() throws Exception {
        var bundle = new Bundle();
        bundle.putString(XRW_ALLOWLIST_METADATA_NAME, "not an int");

        mContext.putServiceMetadata(mMetadataServiceName, bundle);

        ManifestMetadataUtil.getXRequestedWithAllowList();
        Assert.fail("An IllegalArgumentException should have been thrown");
    }
}
