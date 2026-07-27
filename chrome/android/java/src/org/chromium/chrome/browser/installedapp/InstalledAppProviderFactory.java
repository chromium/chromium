// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.installedapp;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.installedapp.InstalledAppProviderImpl;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.installedapp.mojom.InstalledAppProvider;
import org.chromium.services.service_manager.InterfaceFactory;

/** Factory to create instances of the InstalledAppProvider Mojo service. */
@NullMarked
public class InstalledAppProviderFactory
        implements InterfaceFactory<@Nullable InstalledAppProvider> {
    private final RenderFrameHost mRenderFrameHost;

    // BadMessageReason::INSTALLED_APP_PROVIDER_FACTORY_INVALID_FRAME in
    // content/browser/bad_message.h
    private static final int INSTALLED_APP_PROVIDER_FACTORY_INVALID_FRAME = 365;

    public InstalledAppProviderFactory(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
    }

    @Override
    public @Nullable InstalledAppProvider createImpl() {
        if (!mRenderFrameHost.isOutermostMainFrame()) {
            mRenderFrameHost.terminateRendererDueToBadMessage(
                    INSTALLED_APP_PROVIDER_FACTORY_INVALID_FRAME);
            return null;
        }
        Profile profile =
                Profile.fromWebContents(WebContentsStatics.fromRenderFrameHost(mRenderFrameHost));
        assert profile != null;
        return new InstalledAppProviderImpl(profile, mRenderFrameHost);
    }
}
