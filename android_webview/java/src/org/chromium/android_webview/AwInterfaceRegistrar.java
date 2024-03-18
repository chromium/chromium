// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.CalledByNative;

import org.chromium.android_webview.media_integrity.AwMediaIntegrityServiceFactory;
import org.chromium.blink.mojom.Authenticator;
import org.chromium.blink.mojom.WebViewMediaIntegrityService;
import org.chromium.components.webauthn.AuthenticatorFactory;
import org.chromium.content_public.browser.InterfaceRegistrar;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.services.service_manager.InterfaceRegistry;

/** Registers mojo interface implementations exposed to C++ code at the Android Webview layer. */
class AwInterfaceRegistrar {
    @CalledByNative
    private static void registerMojoInterfaces() {
        InterfaceRegistrar.Registry.addRenderFrameHostRegistrar(
                new AndroidWebviewRenderFrameHostInterfaceRegistrar());
    }

    private static class AndroidWebviewRenderFrameHostInterfaceRegistrar
            implements InterfaceRegistrar<RenderFrameHost> {
        @Override
        public void registerInterfaces(
                InterfaceRegistry registry, final RenderFrameHost renderFrameHost) {
            registry.addInterface(
                    Authenticator.MANAGER,
                    new AuthenticatorFactory(renderFrameHost, /* confirmationFactory= */ null));
            registry.addInterface(
                    WebViewMediaIntegrityService.MANAGER,
                    new AwMediaIntegrityServiceFactory(renderFrameHost));
        }
    }
}
