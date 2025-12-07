// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import static org.chromium.android_webview.ShouldInterceptRequestMediator.overridesShouldInterceptRequest;

import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ShouldInterceptRequestMediatorTest {
    @Test
    @SmallTest
    @Feature({"WebView"})
    public void overridesWebViewClient_noOverride() throws NoSuchMethodException {
        Assert.assertFalse(overridesShouldInterceptRequest(new WebViewClient()));
    }

    @Test
    @SmallTest
    @Feature({"WebView"})
    public void overridesWebViewClient_overridesStringOverload() throws NoSuchMethodException {
        Assert.assertTrue(
                overridesShouldInterceptRequest(
                        new WebViewClient() {
                            @Nullable
                            @Override
                            public WebResourceResponse shouldInterceptRequest(
                                    WebView view, String url) {
                                return super.shouldInterceptRequest(view, url);
                            }
                        }));
    }

    @Test
    @SmallTest
    @Feature({"WebView"})
    public void overridesWebViewClient_overridesWebResourceRequestOverload()
            throws NoSuchMethodException {
        Assert.assertTrue(
                overridesShouldInterceptRequest(
                        new WebViewClient() {
                            @Nullable
                            @Override
                            public WebResourceResponse shouldInterceptRequest(
                                    WebView view, WebResourceRequest request) {
                                return super.shouldInterceptRequest(view, request);
                            }
                        }));
    }
}
