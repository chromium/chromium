// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.ComponentName;
import android.os.Bundle;

import androidx.test.InstrumentationRegistry;
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

/** Test for {@link ManifestMetadataUtil} */
@RunWith(AwJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ManifestMetadataUtilTest {
    /*
     * Metadata keys are deliberately not referencing their production declarations to protect
     * against accidental changes.
     */
    private static final String METRICS_OPT_OUT_METADATA_NAME =
            "android.webkit.WebView.MetricsOptOut";
    private static final String CONTEXT_EXPERIMENT_VALUE_METADATA_NAME =
            "android.webkit.WebView.UseWebViewResourceContext";
    private static final String SAFE_BROWSING_OPT_IN_METADATA_NAME =
            "android.webkit.WebView.EnableSafeBrowsing";
    private static final String METADATA_HOLDER_SERVICE_NAME =
            "android.webkit.MetaDataHolderService";
    private static final String MULTI_PROFILE_NAME_TAG_KEY_METADATA_NAME =
            "android.webkit.WebView.MultiProfileNameTagKey";

    private ManifestMetadataMockApplicationContext mContext;
    private ComponentName mMetadataServiceName;

    @Before
    public void setUp() throws Exception {
        mContext =
                new ManifestMetadataMockApplicationContext(
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

        Bundle appMetadata = ManifestMetadataUtil.getAppMetadata(mContext);
        Assert.assertTrue(ManifestMetadataUtil.isAppOptedOutFromMetricsCollection(appMetadata));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Manifest"})
    public void testMetricsCollectionDefault() throws Exception {
        Bundle appMetadata = ManifestMetadataUtil.getAppMetadata(mContext);
        Assert.assertFalse(ManifestMetadataUtil.isAppOptedOutFromMetricsCollection(appMetadata));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Manifest"})
    public void testContextExperimentOptIn() throws Exception {
        var bundle = new Bundle();
        bundle.putBoolean(CONTEXT_EXPERIMENT_VALUE_METADATA_NAME, true);
        mContext.putServiceMetadata(mContext.getPackageName(), bundle);

        Bundle appMetadata = ManifestMetadataUtil.getAppMetadata(mContext);
        Assert.assertTrue(ManifestMetadataUtil.shouldEnableContextExperiment(appMetadata));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Manifest"})
    public void testContextExperimentOptOut() throws Exception {
        var bundle = new Bundle();
        bundle.putBoolean(CONTEXT_EXPERIMENT_VALUE_METADATA_NAME, false);
        mContext.putServiceMetadata(mContext.getPackageName(), bundle);

        Bundle appMetadata = ManifestMetadataUtil.getAppMetadata(mContext);
        Assert.assertFalse(ManifestMetadataUtil.shouldEnableContextExperiment(appMetadata));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Manifest"})
    public void testContextExperimentDefault() throws Exception {
        Bundle appMetadata = ManifestMetadataUtil.getAppMetadata(mContext);
        Assert.assertNull(ManifestMetadataUtil.shouldEnableContextExperiment(appMetadata));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Manifest"})
    public void testSafeBrowsingDefault() throws Exception {
        Bundle appMetadata = ManifestMetadataUtil.getAppMetadata(mContext);
        Assert.assertNull(ManifestMetadataUtil.getSafeBrowsingAppOptInPreference(appMetadata));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Manifest"})
    public void testSafeBrowsingOptIn() throws Exception {
        var bundle = new Bundle();
        bundle.putBoolean(SAFE_BROWSING_OPT_IN_METADATA_NAME, true);
        mContext.putServiceMetadata(mContext.getPackageName(), bundle);
        Bundle appMetadata = ManifestMetadataUtil.getAppMetadata(mContext);
        var preference = ManifestMetadataUtil.getSafeBrowsingAppOptInPreference(appMetadata);
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
        Bundle appMetadata = ManifestMetadataUtil.getAppMetadata(mContext);
        var preference = ManifestMetadataUtil.getSafeBrowsingAppOptInPreference(appMetadata);
        Assert.assertNotNull(preference);
        Assert.assertFalse(preference);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Manifest"})
    public void testMultiProfileProfileNameTagKeyRetrieval() throws Exception {
        var bundle = new Bundle();
        bundle.putInt(MULTI_PROFILE_NAME_TAG_KEY_METADATA_NAME, 12345);
        mContext.putServiceMetadata(mMetadataServiceName, bundle);

        Bundle holderServiceMetadata =
                ManifestMetadataUtil.getMetadataHolderServiceMetadata(mContext);
        Integer profileNameTagKey =
                ManifestMetadataUtil.getAppMultiProfileProfileNameTagKey(holderServiceMetadata);
        Assert.assertNotNull(profileNameTagKey);
        Assert.assertEquals(12345, profileNameTagKey.intValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Manifest"})
    public void testNullMultiProfileProfileNameTagDefault() throws Exception {
        Bundle holderServiceMetadata =
                ManifestMetadataUtil.getMetadataHolderServiceMetadata(mContext);
        Assert.assertNull(
                ManifestMetadataUtil.getAppMultiProfileProfileNameTagKey(holderServiceMetadata));
    }
}
