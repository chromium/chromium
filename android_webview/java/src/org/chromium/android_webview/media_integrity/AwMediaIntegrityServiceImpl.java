// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.media_integrity;

import android.net.Uri;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.MediaIntegrityApiStatus;
import org.chromium.android_webview.common.MediaIntegrityErrorCode;
import org.chromium.android_webview.common.MediaIntegrityProvider;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.ValueOrErrorCallback;
import org.chromium.base.ThreadUtils;
import org.chromium.blink.mojom.WebViewMediaIntegrityErrorCode;
import org.chromium.blink.mojom.WebViewMediaIntegrityProvider;
import org.chromium.blink.mojom.WebViewMediaIntegrityService;
import org.chromium.blink.mojom.WebViewMediaIntegrityTokenResponse;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.mojo.bindings.InterfaceRequest;
import org.chromium.mojo.system.MojoException;
import org.chromium.url.GURL;

import java.util.Objects;

/**
 * Implements the Android WebView Media Integrity API.
 *
 * <p>This class delegates token provider implementation via PlatformServiceBridge.
 *
 * <p>Instances of this class are bound to individual RenderFrameHosts.
 */
@Lifetime.WebView
public class AwMediaIntegrityServiceImpl implements WebViewMediaIntegrityService {
    @NonNull private final RenderFrameHost mRenderFrameHost;

    private static @WebViewMediaIntegrityErrorCode.EnumType int errorCodeToMojomErrorCode(
            @MediaIntegrityErrorCode int code) {
        switch (code) {
            case MediaIntegrityErrorCode.INTERNAL_ERROR:
                return WebViewMediaIntegrityErrorCode.INTERNAL_ERROR;
            case MediaIntegrityErrorCode.NON_RECOVERABLE_ERROR:
                return WebViewMediaIntegrityErrorCode.NON_RECOVERABLE_ERROR;
            case MediaIntegrityErrorCode.API_DISABLED_BY_APPLICATION:
                return WebViewMediaIntegrityErrorCode.API_DISABLED_BY_APPLICATION;
            case MediaIntegrityErrorCode.INVALID_ARGUMENT:
                return WebViewMediaIntegrityErrorCode.INVALID_ARGUMENT;
            case MediaIntegrityErrorCode.TOKEN_PROVIDER_INVALID:
                return WebViewMediaIntegrityErrorCode.TOKEN_PROVIDER_INVALID;
            default:
                throw new IllegalArgumentException(
                        "Unknown MediaIntegrityException.ErrorCode " + code);
        }
    }

    public AwMediaIntegrityServiceImpl(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {
        // Close will also be called in case of connection errors.
    }

    @Override
    public void getIntegrityProvider(
            @NonNull InterfaceRequest<WebViewMediaIntegrityProvider> providerRequest,
            long cloudProjectNumber,
            @NonNull GetIntegrityProvider_Response callback) {
        ThreadUtils.assertOnUiThread();
        // In practice, < 0 means cloudProjectNumber (which is unsigned in the IPC) was >= 2 ** 63.
        // In theory, we should never be called with a number greater than 2 ** 53 - 1 (JavaScript's
        // Number.MAX_SAFE_INTEGER), which the renderer SHOULD reject before passing along the IPC.
        if (cloudProjectNumber < 0
                || cloudProjectNumber > WebViewMediaIntegrityService.MAX_CLOUD_PROJECT_NUMBER) {
            // No Java binding equivalent of ReportBadMessage? Ignore is second-best. Note that this
            // may (on some future garbage collection) result in the Mojo connection being
            // disconnected.
            return;
        }

        final WebContents webContents = WebContentsStatics.fromRenderFrameHost(mRenderFrameHost);
        if (webContents == null) {
            callback.call(WebViewMediaIntegrityErrorCode.INTERNAL_ERROR);
            return;
        }
        final AwSettings awSettings = AwSettings.fromWebContents(webContents);
        if (awSettings == null) {
            callback.call(WebViewMediaIntegrityErrorCode.INTERNAL_ERROR);
            return;
        }
        final GURL gurl = mRenderFrameHost.getLastCommittedURL();
        if (gurl == null) {
            callback.call(WebViewMediaIntegrityErrorCode.INTERNAL_ERROR);
            return;
        }
        final String origin = gurl.getOrigin().getValidSpecOrEmpty();
        @MediaIntegrityApiStatus final int apiStatus;
        if ("".equals(origin)) {
            // An empty origin will be produced for non-http/https URLs.
            apiStatus = awSettings.getWebViewIntegrityApiDefaultStatus();
        } else {
            apiStatus = awSettings.getWebViewIntegrityApiStatusForUri(Uri.parse(origin));
        }
        if (apiStatus == MediaIntegrityApiStatus.DISABLED) {
            callback.call(WebViewMediaIntegrityErrorCode.API_DISABLED_BY_APPLICATION);
            return;
        }

        PlatformServiceBridge.getInstance()
                .getMediaIntegrityProvider(
                        cloudProjectNumber,
                        apiStatus,
                        new ValueOrErrorCallback<MediaIntegrityProvider, Integer>() {
                            @Override
                            public void onResult(MediaIntegrityProvider provider) {
                                ThreadUtils.assertOnUiThread();
                                Objects.requireNonNull(provider);
                                WebViewMediaIntegrityProvider.MANAGER.bind(
                                        new AwMediaIntegrityProviderImpl(provider),
                                        providerRequest);
                                callback.call(null);
                            }

                            @Override
                            public void onError(Integer error) {
                                ThreadUtils.assertOnUiThread();
                                Objects.requireNonNull(error);
                                callback.call(errorCodeToMojomErrorCode(error));
                            }
                        });
    }

    @Lifetime.WebView
    private static class AwMediaIntegrityProviderImpl implements WebViewMediaIntegrityProvider {
        @NonNull private final MediaIntegrityProvider mProvider;

        public AwMediaIntegrityProviderImpl(@NonNull MediaIntegrityProvider provider) {
            mProvider = provider;
        }

        @Override
        public void close() {}

        @Override
        public void onConnectionError(MojoException e) {
            // Close will also be called in case of connection errors.
        }

        @Override
        public void requestToken(
                @Nullable String contentBinding, @NonNull RequestToken_Response callback) {
            ThreadUtils.assertOnUiThread();
            // The provider is responsible for any contentBinding validation.
            mProvider.requestToken(
                    contentBinding,
                    new ValueOrErrorCallback<String, Integer>() {
                        @Override
                        public void onResult(String token) {
                            ThreadUtils.assertOnUiThread();
                            Objects.requireNonNull(token);
                            final WebViewMediaIntegrityTokenResponse response =
                                    new WebViewMediaIntegrityTokenResponse();
                            response.setToken(token);
                            callback.call(response);
                        }

                        @Override
                        public void onError(Integer error) {
                            ThreadUtils.assertOnUiThread();
                            Objects.requireNonNull(error);
                            final WebViewMediaIntegrityTokenResponse response =
                                    new WebViewMediaIntegrityTokenResponse();
                            response.setErrorCode(errorCodeToMojomErrorCode(error));
                            callback.call(response);
                        }
                    });
        }
    }
}
