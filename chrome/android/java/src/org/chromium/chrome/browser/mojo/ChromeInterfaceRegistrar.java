// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.mojo;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.blink.mojom.Authenticator;
import org.chromium.components.webauthn.AuthenticatorFactory;
import org.chromium.content_public.browser.InterfaceRegistrar;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.services.service_manager.InterfaceRegistry;

/** Registers mojo interface implementations exposed to C++ code at the Chrome layer. */
class ChromeInterfaceRegistrar {
    @CalledByNative
    private static void registerMojoInterfaces() {
        InterfaceRegistrar.Registry.addRenderFrameHostRegistrar(
                new ChromeRenderFrameHostInterfaceRegistrar());
    }

    private static class ChromeRenderFrameHostInterfaceRegistrar
            implements InterfaceRegistrar<RenderFrameHost> {
        @Override
        public void registerInterfaces(
                InterfaceRegistry registry, final RenderFrameHost renderFrameHost) {
            registry.addInterface(Authenticator.MANAGER, new AuthenticatorFactory(renderFrameHost));
        }
    }
}
