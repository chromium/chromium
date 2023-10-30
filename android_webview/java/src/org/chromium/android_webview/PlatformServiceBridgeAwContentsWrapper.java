// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.net.Uri;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.PlatformServiceBridge.JsReplyProxyWrapper;
import org.chromium.android_webview.common.PlatformServiceBridge.ProfileIdentifier;
import org.chromium.android_webview.common.PlatformServiceBridge.WebMessageListenerWrapper;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;

import java.lang.ref.WeakReference;
import java.util.Objects;

/**
 * Wrapper class that exposes specific methods of {@link AwContents} to implementations of {@link
 * org.chromium.android_webview.common.PlatformServiceBridge}.
 */
public class PlatformServiceBridgeAwContentsWrapper
        implements PlatformServiceBridge.AwContentsWrapper {

    @NonNull private final AwContents mAwContents;

    public PlatformServiceBridgeAwContentsWrapper(@NonNull AwContents awContents) {
        mAwContents = awContents;
    }

    @Override
    public void addDocumentStartJavaScript(
            @NonNull String script, @NonNull String[] allowedOriginRules) {
        mAwContents.addDocumentStartJavaScript(script, allowedOriginRules);
    }

    @Override
    public void addWrappedWebMessageListener(
            @NonNull String jsObjectName,
            @NonNull String[] allowedOriginRules,
            @NonNull WebMessageListenerWrapper listener) {
        mAwContents.addWebMessageListener(
                jsObjectName, allowedOriginRules, new WebMessageListenerAdapter(listener));
    }

    @Override
    public ProfileIdentifier getProfileIdentifier() {
        return new ReferencingProfileIdentifier(mAwContents.getBrowserContext());
    }

    private static class WebMessageListenerAdapter implements WebMessageListener {

        @NonNull private final WebMessageListenerWrapper mListener;

        public WebMessageListenerAdapter(@NonNull WebMessageListenerWrapper listener) {
            mListener = listener;
        }

        @Override
        public void onPostMessage(
                MessagePayload payload,
                Uri topLevelOrigin,
                Uri sourceOrigin,
                boolean isMainFrame,
                JsReplyProxy jsReplyProxy,
                MessagePort[] ports) {
            mListener.onPostMessage(
                    payload,
                    topLevelOrigin,
                    sourceOrigin,
                    isMainFrame,
                    new JsReplyProxyWrapperImpl(jsReplyProxy),
                    ports);
        }
    }

    private static class JsReplyProxyWrapperImpl implements JsReplyProxyWrapper {

        @NonNull private final JsReplyProxy mReplyProxy;

        public JsReplyProxyWrapperImpl(@NonNull JsReplyProxy replyProxy) {
            mReplyProxy = replyProxy;
        }

        @Override
        public void postMessage(@NonNull MessagePayload payload) {
            mReplyProxy.postMessage(payload);
        }
    }

    private static class ReferencingProfileIdentifier implements ProfileIdentifier {

        private final WeakReference<AwBrowserContext> mContextRef;

        private ReferencingProfileIdentifier(AwBrowserContext browserContext) {
            mContextRef = new WeakReference<>(browserContext);
        }

        @Override
        public int hashCode() {
            AwBrowserContext context = mContextRef.get();
            if (context != null) {
                return context.hashCode();
            } else {
                return 0;
            }
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            AwBrowserContext context = mContextRef.get();
            if (context == null) {
                return false;
            }
            if (!(obj instanceof ReferencingProfileIdentifier pi)) {
                return false;
            }
            return Objects.equals(context, pi.mContextRef.get());
        }
    }
}
