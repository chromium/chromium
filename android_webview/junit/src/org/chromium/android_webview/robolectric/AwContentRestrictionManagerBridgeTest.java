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
import android.os.ParcelFileDescriptor;

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
    private static final long TEST_NAVIGATION_ID = 123;

    private ManifestMetadataMockApplicationContext mContext;
    private ComponentName mMetadataServiceName;
    private Boolean mCallbackResult;
    private AwContentRestrictionManagerBridge mBridge;

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
    public void setUp() throws Exception {
        mCallbackResult = null;
        mContext = new ManifestMetadataMockApplicationContext(RuntimeEnvironment.application);
        mMetadataServiceName = new ComponentName(mContext, METADATA_HOLDER_SERVICE_NAME);
        ContextUtils.initApplicationContextForTests(mContext);
        AconfigFlaggedApiDelegate.setInstanceForTesting(mFlaggedApiDelegate);
        setEnableContentRestrictionMetadata(true);

        // Stub out ParcelFileDescriptor.createPipe() to prevent crashes on detachFd().
        ParcelFileDescriptor mockReadFd = Mockito.mock(ParcelFileDescriptor.class);
        ParcelFileDescriptor mockWriteFd = Mockito.mock(ParcelFileDescriptor.class);
        when(mockWriteFd.detachFd()).thenReturn(42);
        AwContentRestrictionManagerBridge.setParcelFileDescriptorPipeFactoryForTesting(
                () -> new ParcelFileDescriptor[] {mockReadFd, mockWriteFd});

        mBridge = new AwContentRestrictionManagerBridge();
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
        Assert.assertFalse(mBridge.isContentRestrictionEnabled());
        verifyNoInteractions(mFlaggedApiDelegate);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testIsContentRestrictionEnabled_featureEnabled() {
        when(mFlaggedApiDelegate.isContentRestrictionEnabled()).thenReturn(true);
        Assert.assertTrue(mBridge.isContentRestrictionEnabled());
        verify(mFlaggedApiDelegate).isContentRestrictionEnabled();

        when(mFlaggedApiDelegate.isContentRestrictionEnabled()).thenReturn(false);
        Assert.assertFalse(mBridge.isContentRestrictionEnabled());
        verify(mFlaggedApiDelegate, times(2)).isContentRestrictionEnabled();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testIsContentRestrictionEnabled_appOptOut() {
        setEnableContentRestrictionMetadata(false);
        Assert.assertFalse(mBridge.isContentRestrictionEnabled());
        verifyNoInteractions(mFlaggedApiDelegate);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testRequestContentClassification_invalidUrl() {
        mBridge.requestContentClassification(
                TEST_NAVIGATION_ID, /* url= */ null, TEST_MIME_TYPE, mMockCallback);
        Assert.assertFalse("Should block requests with invalid URL", mCallbackResult);
        verifyNoInteractions(mFlaggedApiDelegate);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testRequestContentClassification_delegateMissing() {
        AconfigFlaggedApiDelegate.setInstanceForTesting(null);
        mBridge.requestContentClassification(
                TEST_NAVIGATION_ID, TEST_URL, TEST_MIME_TYPE, mMockCallback);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertFalse("Should block requests when delegate is missing", mCallbackResult);
        verifyNoInteractions(mFlaggedApiDelegate);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testRequestContentClassification_allowed() {
        Promise<Boolean> promise = new Promise<>();
        when(mFlaggedApiDelegate.requestContentRestrictionClassification(
                        /* uri= */ Mockito.any(),
                        /* requestBody= */ Mockito.eq(null),
                        /* mimeType= */ Mockito.eq(TEST_MIME_TYPE),
                        /* executor= */ Mockito.any()))
                .thenReturn(promise);
        mBridge.requestContentClassification(
                TEST_NAVIGATION_ID, TEST_URL, TEST_MIME_TYPE, mMockCallback);
        promise.fulfill(true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertTrue("Should allow request when delegate allows", mCallbackResult);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testRequestContentClassification_blocked() {
        Promise<Boolean> promise = new Promise<>();
        when(mFlaggedApiDelegate.requestContentRestrictionClassification(
                        /* uri= */ Mockito.any(),
                        /* requestBody= */ Mockito.eq(null),
                        /* mimeType= */ Mockito.eq(TEST_MIME_TYPE),
                        /* executor= */ Mockito.any()))
                .thenReturn(promise);
        mBridge.requestContentClassification(
                TEST_NAVIGATION_ID, TEST_URL, TEST_MIME_TYPE, mMockCallback);
        promise.fulfill(false);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertFalse("Should block request when delegate denies", mCallbackResult);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testRequestContentClassification_withRequestBody() {
        mBridge.createRequestBodyPipeAndGetWriteFd(TEST_NAVIGATION_ID);
        Promise<Boolean> promise = new Promise<>();
        when(mFlaggedApiDelegate.requestContentRestrictionClassification(
                        /* uri= */ Mockito.any(),
                        /* requestBody= */ Mockito.isNotNull(),
                        /* mimeType= */ Mockito.eq(TEST_MIME_TYPE),
                        /* executor= */ Mockito.any()))
                .thenReturn(promise);
        mBridge.requestContentClassification(
                TEST_NAVIGATION_ID, TEST_URL, TEST_MIME_TYPE, mMockCallback);
        promise.fulfill(true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertTrue("Should allow request when delegate allows", mCallbackResult);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testDestroyCleansUpReadFileDescriptorMap() {
        mBridge.createRequestBodyPipeAndGetWriteFd(TEST_NAVIGATION_ID);
        mBridge.destroy();

        // Although non-conventional, we verify that there is no request body being tracked by
        // triggering a request to classify content.
        Promise<Boolean> promise = new Promise<>();
        when(mFlaggedApiDelegate.requestContentRestrictionClassification(
                        /* uri= */ Mockito.any(),
                        /* requestBody= */ Mockito.eq(null),
                        /* mimeType= */ Mockito.eq(TEST_MIME_TYPE),
                        /* executor= */ Mockito.any()))
                .thenReturn(promise);
        mBridge.requestContentClassification(
                TEST_NAVIGATION_ID, TEST_URL, TEST_MIME_TYPE, mMockCallback);
        promise.fulfill(true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertTrue("Should allow request when delegate allows", mCallbackResult);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testSendShowRestrictedContentIntent_invalidUrl() {
        Assert.assertFalse(mBridge.sendShowRestrictedContentIntent(null));
        verifyNoInteractions(mFlaggedApiDelegate);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testSendShowRestrictedContentIntent_delegateMissing() {
        AconfigFlaggedApiDelegate.setInstanceForTesting(null);
        Assert.assertFalse(mBridge.sendShowRestrictedContentIntent(TEST_URL));
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
        Assert.assertTrue(mBridge.sendShowRestrictedContentIntent(TEST_URL));
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
        Assert.assertFalse(mBridge.sendShowRestrictedContentIntent(TEST_URL));
        verify(mFlaggedApiDelegate).sendShowRestrictedContentIntent(Mockito.eq(testUri));
    }
}
