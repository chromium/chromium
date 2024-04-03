// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.media_integrity;

import android.net.Uri;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.android_webview.AwBrowserContext.AwMediaIntegrityProviderKey;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.MediaIntegrityApiStatus;
import org.chromium.android_webview.common.MediaIntegrityErrorCode;
import org.chromium.android_webview.common.MediaIntegrityProvider;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.ValueOrErrorCallback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
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

    private static int sGetTokenProviderCallCounter;
    private static int sCacheHitCounter;
    private static int sCacheMissCounter;
    private static int sProviderCreatedCounter;
    @NonNull private final RenderFrameHost mRenderFrameHost;
    private final WebContents mWebContents;

    public AwMediaIntegrityServiceImpl(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
        mWebContents = WebContentsStatics.fromRenderFrameHost(renderFrameHost);
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

        final AwSettings awSettings = AwSettings.fromWebContents(mWebContents);
        final String sourceOrigin = getOriginFromRenderFrame(mRenderFrameHost);
        if (awSettings == null || sourceOrigin == null) {
            callback.call(WebViewMediaIntegrityErrorCode.INTERNAL_ERROR);
            return;
        }
        @MediaIntegrityApiStatus
        final int apiStatus = getMediaIntegrityApiStatus(sourceOrigin, awSettings);
        if (apiStatus == MediaIntegrityApiStatus.DISABLED) {
            callback.call(WebViewMediaIntegrityErrorCode.API_DISABLED_BY_APPLICATION);
            return;
        }

        RecordHistogram.recordCount100Histogram(
                "Android.WebView.MediaIntegrity.GetTokenProviderCumulativeV2",
                ++sGetTokenProviderCallCounter);

        final AwContents awContents = AwContents.fromWebContents(mWebContents);
        if (awContents == null) {
            callback.call(WebViewMediaIntegrityErrorCode.INTERNAL_ERROR);
            return;
        }

        AwMediaIntegrityProviderKey key =
                createProviderKey(awContents, cloudProjectNumber, apiStatus, sourceOrigin);
        if (key == null) {
            callback.call(WebViewMediaIntegrityErrorCode.INTERNAL_ERROR);
            return;
        }
        WebViewMediaIntegrityProvider cachedProvider = getCachedProvider(awContents, key);

        if (cachedProvider != null) {
            ThreadUtils.assertOnUiThread();
            RecordHistogram.recordCount100Histogram(
                    "Android.WebView.MediaIntegrity.TokenProviderCacheHitsCumulativeV2",
                    ++sCacheHitCounter);
            WebViewMediaIntegrityProvider.MANAGER.bind(cachedProvider, providerRequest);
            callback.call(/* error= */ null);
            return;
        }
        RecordHistogram.recordCount100Histogram(
                "Android.WebView.MediaIntegrity.TokenProviderCacheMissesCumulativeV2",
                ++sCacheMissCounter);
        PlatformServiceBridge.getInstance()
                .getMediaIntegrityProvider(
                        cloudProjectNumber,
                        key.getRequestMode(),
                        new ValueOrErrorCallback<MediaIntegrityProvider, Integer>() {
                            @Override
                            public void onResult(MediaIntegrityProvider provider) {
                                ThreadUtils.assertOnUiThread();
                                Objects.requireNonNull(provider);
                                RecordHistogram.recordCount100Histogram(
                                        "Android.WebView.MediaIntegrity"
                                                + ".TokenProviderCreatedCumulativeV2",
                                        ++sProviderCreatedCounter);
                                WebViewMediaIntegrityProvider integrityProvider =
                                        new AwMediaIntegrityProviderImpl(provider);
                                WebViewMediaIntegrityProvider.MANAGER.bind(
                                        integrityProvider, providerRequest);
                                callback.call(/* error= */ null);
                                if (!key.getSourceOrigin().toString().isEmpty()) {
                                    // Cache the provider only if source origin is
                                    // non-opaque
                                    putProviderInCache(awContents, key, integrityProvider);
                                }
                            }

                            @Override
                            public void onError(Integer error) {
                                ThreadUtils.assertOnUiThread();
                                Objects.requireNonNull(error);
                                callback.call(errorCodeToMojomErrorCode(error));
                            }
                        });
    }

    @Nullable
    private WebViewMediaIntegrityProvider getCachedProvider(
            @NonNull AwContents awContents, @NonNull AwMediaIntegrityProviderKey key) {
        return awContents.getBrowserContext().getCachedAwMediaIntegrityProvider(key);
    }

    private void putProviderInCache(
            @NonNull AwContents awContents,
            @NonNull AwMediaIntegrityProviderKey key,
            @NonNull WebViewMediaIntegrityProvider provider) {
        awContents.getBrowserContext().putAwMediaIntegrityProviderInCache(key, provider);
    }

    @Nullable
    private AwMediaIntegrityProviderKey createProviderKey(
            @NonNull AwContents awContents,
            long cloudProjectNumber,
            @MediaIntegrityApiStatus int apiStatus,
            @NonNull String sourceOrigin) {
        final String topLevelOrigin = getOriginFromRenderFrame(mRenderFrameHost.getMainFrame());
        if (topLevelOrigin == null) {
            return null;
        }

        return awContents
                .getBrowserContext()
                .createAwMediaIntegrityProviderKey(
                        Uri.parse(sourceOrigin),
                        Uri.parse(topLevelOrigin),
                        apiStatus,
                        cloudProjectNumber);
    }

    private @MediaIntegrityApiStatus int getMediaIntegrityApiStatus(
            @NonNull String sourceOrigin, @NonNull AwSettings awSettings) {
        @MediaIntegrityApiStatus int apiStatus;
        if ("".equals(sourceOrigin)) {
            // An empty origin will be produced for non-http/https URLs.
            apiStatus = awSettings.getWebViewIntegrityApiDefaultStatus();
        } else {
            apiStatus = awSettings.getWebViewIntegrityApiStatusForUri(Uri.parse(sourceOrigin));
        }
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.MediaIntegrity.ApiStatusV2",
                apiStatus,
                MediaIntegrityApiStatus.COUNT);
        return apiStatus;
    }

    @Nullable
    private String getOriginFromRenderFrame(RenderFrameHost host) {
        final GURL sourceGurl = host.getLastCommittedURL();
        if (sourceGurl == null) {
            return null;
        }
        return sourceGurl.getOrigin().getValidSpecOrEmpty();
    }

    @Lifetime.WebView
    private static class AwMediaIntegrityProviderImpl implements WebViewMediaIntegrityProvider {
        @NonNull private final MediaIntegrityProvider mProvider;
        private int mRequestCounter;

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
            RecordHistogram.recordCount1000Histogram(
                    "Android.WebView.MediaIntegrity.GetTokenCumulativeV2", ++mRequestCounter);
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
