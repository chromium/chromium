// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.media_integrity;

import org.chromium.blink.mojom.WebViewMediaIntegrityService;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.services.service_manager.InterfaceFactory;

public class AwMediaIntegrityServiceFactory implements InterfaceFactory {
    private final RenderFrameHost mRenderFrameHost;

    public AwMediaIntegrityServiceFactory(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
    }

    @Override
    public WebViewMediaIntegrityService createImpl() {
        return new AwMediaIntegrityServiceImpl(mRenderFrameHost);
    }
}
