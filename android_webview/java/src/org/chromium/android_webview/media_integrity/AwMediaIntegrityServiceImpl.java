// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.media_integrity;

import android.net.Uri;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserContext.MediaIntegrityProviderKey;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.MediaIntegrityApiStatus;
import org.chromium.android_webview.common.MediaIntegrityErrorCode;
import org.chromium.android_webview.common.MediaIntegrityErrorWrapper;
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
import org.chromium.url.Origin;

import java.lang.ref.WeakReference;
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
    @Nullable private final WebContents mWebContents;

    public AwMediaIntegrityServiceImpl(@NonNull RenderFrameHost renderFrameHost) {
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

        if (mWebContents == null) {
            callback.call(WebViewMediaIntegrityErrorCode.INTERNAL_ERROR);
            return;
        }

        final RenderFrameHost mainFrame = mRenderFrameHost.getMainFrame();
        if (mainFrame == null) {
            callback.call(WebViewMediaIntegrityErrorCode.INTERNAL_ERROR);
            return;
        }

        final Origin sourceOrigin = mRenderFrameHost.getLastCommittedOrigin();
        final Origin topLevelOrigin = mainFrame.getLastCommittedOrigin();
        if (sourceOrigin == null || topLevelOrigin == null) {
            callback.call(WebViewMediaIntegrityErrorCode.INTERNAL_ERROR);
            return;
        }

        final AwSettings awSettings = AwSettings.fromWebContents(mWebContents);
        if (awSettings == null) {
            callback.call(WebViewMediaIntegrityErrorCode.INTERNAL_ERROR);
            return;
        }

        // The GURL-based, string-based, and android.net.Uri-based origin representations are
        // lossy. They are only used for the API-status check. Prefer using Origin-based origins in
        // all other cases.
        final GURL sourceGurl = mRenderFrameHost.getLastCommittedURL();
        if (sourceGurl == null) {
            callback.call(WebViewMediaIntegrityErrorCode.INTERNAL_ERROR);
            return;
        }
        final GURL sourceOriginGurl = sourceGurl.getOrigin();
        final String sourceOriginString = sourceOriginGurl.getValidSpecOrEmpty();
        if (!Objects.equals(sourceOrigin.getScheme(), sourceOriginGurl.getScheme())
                || "".equals(sourceOriginString)) {
            // Note that sourceOrigin and sourceOriginGurl (getLastCommittedOrigin and
            // getLastCommittedURL) may not agree on the origin in certain situations, including
            // non-standard URIs and pages loaded via loadDataWithBaseURL. For now, we do not
            // support these or loadDataWithBaseURL.
            callback.call(WebViewMediaIntegrityErrorCode.NON_RECOVERABLE_ERROR);
            return;
        }

        @MediaIntegrityApiStatus
        final int apiStatus = getMediaIntegrityApiStatus(sourceOriginString, awSettings);
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
        final AwBrowserContext awBrowserContext = awContents.getBrowserContext();

        final MediaIntegrityProviderKey key =
                new MediaIntegrityProviderKey(
                        sourceOrigin, topLevelOrigin, apiStatus, cloudProjectNumber);
        final MediaIntegrityProvider cachedProvider =
                awBrowserContext.getCachedMediaIntegrityProvider(key);
        if (cachedProvider != null) {
            RecordHistogram.recordCount100Histogram(
                    "Android.WebView.MediaIntegrity.TokenProviderCacheHitsCumulativeV2",
                    ++sCacheHitCounter);
            final WebViewMediaIntegrityProvider integrityProvider =
                    new WebViewMediaIntegrityProviderImpl(cachedProvider, key, awBrowserContext);
            WebViewMediaIntegrityProvider.MANAGER.bind(integrityProvider, providerRequest);
            callback.call(/* error= */ null);
            return;
        }
        RecordHistogram.recordCount100Histogram(
                "Android.WebView.MediaIntegrity.TokenProviderCacheMissesCumulativeV2",
                ++sCacheMissCounter);
        PlatformServiceBridge.getInstance()
                .getMediaIntegrityProvider2(
                        cloudProjectNumber,
                        /* requestMode= */ apiStatus,
                        new ValueOrErrorCallback<>() {
                            @Override
                            public void onResult(MediaIntegrityProvider provider) {
                                ThreadUtils.assertOnUiThread();
                                Objects.requireNonNull(provider);
                                RecordHistogram.recordCount100Histogram(
                                        "Android.WebView.MediaIntegrity"
                                                + ".TokenProviderCreatedCumulativeV2",
                                        ++sProviderCreatedCounter);
                                final WebViewMediaIntegrityProvider integrityProvider =
                                        new WebViewMediaIntegrityProviderImpl(
                                                provider, key, awBrowserContext);
                                WebViewMediaIntegrityProvider.MANAGER.bind(
                                        integrityProvider, providerRequest);
                                callback.call(/* error= */ null);
                                awBrowserContext.putMediaIntegrityProviderInCache(key, provider);
                            }

                            @Override
                            public void onError(MediaIntegrityErrorWrapper error) {
                                ThreadUtils.assertOnUiThread();
                                Objects.requireNonNull(error);
                                callback.call(errorCodeToMojomErrorCode(error.value));
                            }
                        });
    }

    private @MediaIntegrityApiStatus int getMediaIntegrityApiStatus(
            @NonNull String sourceOriginString, @NonNull AwSettings awSettings) {
        // An empty origin will be produced for many (but not all) non-http/non-https schemes.
        // We disallow this in the caller.
        assert !"".equals(sourceOriginString);

        @MediaIntegrityApiStatus
        int apiStatus =
                awSettings.getWebViewIntegrityApiStatusForUri(Uri.parse(sourceOriginString));
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.MediaIntegrity.ApiStatusV2",
                apiStatus,
                MediaIntegrityApiStatus.COUNT);
        return apiStatus;
    }

    @Lifetime.WebView
    private static class WebViewMediaIntegrityProviderImpl
            implements WebViewMediaIntegrityProvider {
        @NonNull private final MediaIntegrityProvider mProvider;
        @NonNull private final MediaIntegrityProviderKey mCacheKey;
        @NonNull private final WeakReference<AwBrowserContext> mAwBrowserContext;
        private int mRequestCounter;

        public WebViewMediaIntegrityProviderImpl(
                @NonNull MediaIntegrityProvider provider,
                @NonNull MediaIntegrityProviderKey cacheKey,
                @NonNull AwBrowserContext awBrowserContext) {
            mProvider = provider;
            mCacheKey = cacheKey;
            mAwBrowserContext = new WeakReference<>(awBrowserContext);
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
            mProvider.requestToken2(
                    contentBinding,
                    new ValueOrErrorCallback<>() {
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
                        public void onError(MediaIntegrityErrorWrapper error) {
                            ThreadUtils.assertOnUiThread();
                            Objects.requireNonNull(error);
                            if (error.value == MediaIntegrityErrorCode.TOKEN_PROVIDER_INVALID) {
                                // This callback could take an arbitrary amount of time. We use a
                                // weak reference to avoid making assumptions about AwBrowserContext
                                // lifetimes.
                                final AwBrowserContext awBrowserContext = mAwBrowserContext.get();
                                if (awBrowserContext != null) {
                                    awBrowserContext.invalidateCachedMediaIntegrityProvider(
                                            mCacheKey, mProvider);
                                }
                            }
                            final WebViewMediaIntegrityTokenResponse response =
                                    new WebViewMediaIntegrityTokenResponse();
                            response.setErrorCode(errorCodeToMojomErrorCode(error.value));
                            callback.call(response);
                        }
                    });
        }
    }
}
