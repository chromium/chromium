// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.content.ComponentName;
import android.net.Uri;
import android.os.Bundle;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.android_webview.AwContentRestrictionManagerBridge;
import org.chromium.android_webview.ManifestMetadataUtil;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.test.util.ManifestMetadataMockApplicationContext;
import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ContextUtils;
import org.chromium.base.JniOnceCallback;
import org.chromium.base.Promise;
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
    private static final String TEST_URL = "https://example.com";
    private static final String TEST_MIME_TYPE = "text/html";

    private ManifestMetadataMockApplicationContext mContext;
    private ComponentName mMetadataServiceName;
    private Boolean mCallbackResult;
    private final JniOnceCallback<Boolean> mMockCallback =
            new JniOnceCallback<Boolean>() {
                @Override
                public void onResult(Boolean result) {
                    mCallbackResult = result;
                }

                @Override
                public void destroy() {}
            };

    @Before
    public void setUp() {
        mCallbackResult = null;
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

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testRequestContentClassification_invalidUrl() {
        AwContentRestrictionManagerBridge.requestContentClassification(
                /* url= */ null, TEST_MIME_TYPE, mMockCallback);
        Assert.assertEquals(false, mCallbackResult);
        verifyNoInteractions(mFlaggedApiDelegate);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testRequestContentClassification_delegateMissing() {
        AconfigFlaggedApiDelegate.setInstanceForTesting(null);
        AwContentRestrictionManagerBridge.requestContentClassification(
                TEST_URL, TEST_MIME_TYPE, mMockCallback);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(false, mCallbackResult);
        verifyNoInteractions(mFlaggedApiDelegate);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testRequestContentClassification_allowed() {
        Promise<Boolean> promise = new Promise<>();
        when(mFlaggedApiDelegate.requestContentRestrictionClassification(
                        Mockito.any(), Mockito.eq(null), Mockito.eq(TEST_MIME_TYPE), Mockito.any()))
                .thenReturn(promise);
        AwContentRestrictionManagerBridge.requestContentClassification(
                TEST_URL, TEST_MIME_TYPE, mMockCallback);
        promise.fulfill(true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(true, mCallbackResult);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testRequestContentClassification_blocked() {
        Promise<Boolean> promise = new Promise<>();
        when(mFlaggedApiDelegate.requestContentRestrictionClassification(
                        Mockito.any(), Mockito.eq(null), Mockito.eq(TEST_MIME_TYPE), Mockito.any()))
                .thenReturn(promise);
        AwContentRestrictionManagerBridge.requestContentClassification(
                TEST_URL, TEST_MIME_TYPE, mMockCallback);
        promise.fulfill(false);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(false, mCallbackResult);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testSendShowRestrictedContentIntent_invalidUrl() {
        Assert.assertFalse(AwContentRestrictionManagerBridge.sendShowRestrictedContentIntent(null));
        verifyNoInteractions(mFlaggedApiDelegate);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testSendShowRestrictedContentIntent_delegateMissing() {
        AconfigFlaggedApiDelegate.setInstanceForTesting(null);
        Assert.assertFalse(
                AwContentRestrictionManagerBridge.sendShowRestrictedContentIntent(TEST_URL));
        verifyNoInteractions(mFlaggedApiDelegate);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testSendShowRestrictedContentIntent_success() {
        Uri testUri = Uri.parse(TEST_URL);
        when(mFlaggedApiDelegate.sendShowRestrictedContentIntent(Mockito.eq(testUri)))
                .thenReturn(true);
        Assert.assertTrue(
                AwContentRestrictionManagerBridge.sendShowRestrictedContentIntent(TEST_URL));
        verify(mFlaggedApiDelegate).sendShowRestrictedContentIntent(Mockito.eq(testUri));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testSendShowRestrictedContentIntent_failure() {
        Uri testUri = Uri.parse(TEST_URL);
        when(mFlaggedApiDelegate.sendShowRestrictedContentIntent(Mockito.eq(testUri)))
                .thenReturn(false);
        Assert.assertFalse(
                AwContentRestrictionManagerBridge.sendShowRestrictedContentIntent(TEST_URL));
        verify(mFlaggedApiDelegate).sendShowRestrictedContentIntent(Mockito.eq(testUri));
    }
}
