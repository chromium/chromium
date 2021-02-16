// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.installedapp;

import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.installedapp.mojom.InstalledAppProvider;
import org.chromium.services.service_manager.InterfaceFactory;
import org.chromium.url.GURL;

/** Factory to create instances of the InstalledAppProvider Mojo service. */
public class InstalledAppProviderFactory implements InterfaceFactory<InstalledAppProvider> {
    private final FrameUrlDelegateImpl mFrameUrlDelegate;

    private static final class FrameUrlDelegateImpl
            implements InstalledAppProviderImpl.FrameUrlDelegate {
        private final RenderFrameHost mRenderFrameHost;

        public FrameUrlDelegateImpl(RenderFrameHost renderFrameHost) {
            mRenderFrameHost = renderFrameHost;
        }

        @Override
        public GURL getUrl() {
            GURL url = mRenderFrameHost.getLastCommittedURL();
            return (url != null) ? url : GURL.emptyGURL();
        }

        @Override
        public boolean isIncognito() {
            return mRenderFrameHost.isIncognito();
        }
    }

    public InstalledAppProviderFactory(RenderFrameHost renderFrameHost) {
        mFrameUrlDelegate = new FrameUrlDelegateImpl(renderFrameHost);
    }

    @Override
    public InstalledAppProvider createImpl() {
        return new InstalledAppProviderImpl(
                mFrameUrlDelegate, new PackageManagerDelegate(), InstantAppsHandler.getInstance());
    }
}
