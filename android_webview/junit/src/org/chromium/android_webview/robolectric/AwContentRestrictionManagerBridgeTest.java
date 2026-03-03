// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.content.ComponentName;
import android.os.Bundle;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.android_webview.AwContentRestrictionManagerBridge;
import org.chromium.android_webview.ManifestMetadataUtil;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.test.util.ManifestMetadataMockApplicationContext;
import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;

/** Unit tests for the AwContentRestrictionManagerBridge. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AwContentRestrictionManagerBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock AconfigFlaggedApiDelegate mFlaggedApiDelegate;

    private static final String ENABLE_CONTENT_RESTRICTION_METADATA_NAME =
            "android.webkit.WebView.EnableContentRestriction";
    private static final String METADATA_HOLDER_SERVICE_NAME =
            "android.webkit.MetaDataHolderService";

    private ManifestMetadataMockApplicationContext mContext;
    private ComponentName mMetadataServiceName;

    @Before
    public void setUp() {
        mContext = new ManifestMetadataMockApplicationContext(RuntimeEnvironment.application);
        mMetadataServiceName = new ComponentName(mContext, METADATA_HOLDER_SERVICE_NAME);
        ContextUtils.initApplicationContextForTests(mContext);
        AconfigFlaggedApiDelegate.setInstanceForTesting(mFlaggedApiDelegate);
        setEnableContentRestrictionMetadata(true);
    }

    private void setEnableContentRestrictionMetadata(boolean enabled) {
        Bundle bundle = new Bundle();
        bundle.putBoolean(ENABLE_CONTENT_RESTRICTION_METADATA_NAME, enabled);
        mContext.putServiceMetadata(mMetadataServiceName, bundle);
        ManifestMetadataUtil.clearMetadataCache();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @DisableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testIsContentRestrictionEnabled_featureDisabled() {
        Assert.assertFalse(AwContentRestrictionManagerBridge.isContentRestrictionEnabled());
        verifyNoInteractions(mFlaggedApiDelegate);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testIsContentRestrictionEnabled_featureEnabled() {
        when(mFlaggedApiDelegate.isContentRestrictionEnabled()).thenReturn(true);
        Assert.assertTrue(AwContentRestrictionManagerBridge.isContentRestrictionEnabled());
        verify(mFlaggedApiDelegate).isContentRestrictionEnabled();

        when(mFlaggedApiDelegate.isContentRestrictionEnabled()).thenReturn(false);
        Assert.assertFalse(AwContentRestrictionManagerBridge.isContentRestrictionEnabled());
        verify(mFlaggedApiDelegate, times(2)).isContentRestrictionEnabled();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testIsContentRestrictionEnabled_appOptOut() {
        setEnableContentRestrictionMetadata(false);
        Assert.assertFalse(AwContentRestrictionManagerBridge.isContentRestrictionEnabled());
        verifyNoInteractions(mFlaggedApiDelegate);
    }
}
