// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webshare;

import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ShareDelegateImpl.ShareOrigin;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.webshare.ShareServiceImpl;
import org.chromium.content_public.browser.WebContents;
import org.chromium.services.service_manager.InterfaceFactory;
import org.chromium.webshare.mojom.ShareService;

/**
 * Factory that creates instances of ShareService.
 */
public class ShareServiceImplementationFactory implements InterfaceFactory<ShareService> {
    private final WebContents mWebContents;

    public ShareServiceImplementationFactory(WebContents webContents) {
        mWebContents = webContents;
    }

    @Override
    public ShareService createImpl() {
        ShareServiceImpl.WebShareDelegate delegate = new ShareServiceImpl.WebShareDelegate() {
            @Override
            public boolean canShare() {
                return mWebContents.getTopLevelNativeWindow().getActivity() != null;
            }

            @Override
            public void share(ShareParams params) {
                ChromeActivity<?> activity =
                        (ChromeActivity<?>) params.getWindow().getActivity().get();
                activity.getShareDelegateSupplier().get().share(
                        params, new ChromeShareExtras.Builder().build(), ShareOrigin.WEBSHARE_API);
            }
        };

        return new ShareServiceImpl(mWebContents, delegate);
    }
}
