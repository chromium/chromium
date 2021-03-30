// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webshare;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegateImpl.ShareOrigin;
import org.chromium.chrome.browser.share.ShareDelegateSupplier;
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
    private Supplier<ShareDelegate> mShareDelegateSupplier;

    public ShareServiceImplementationFactory(WebContents webContents) {
        mWebContents = webContents;
        mShareDelegateSupplier = ShareDelegateSupplier.from(webContents.getTopLevelNativeWindow());
        assert mShareDelegateSupplier != null;
    }

    @Override
    public ShareService createImpl() {
        ShareServiceImpl.WebShareDelegate delegate = new ShareServiceImpl.WebShareDelegate() {
            @Override
            public boolean canShare() {
                return mShareDelegateSupplier.get() != null;
            }

            @Override
            public void share(ShareParams params) {
                assert mShareDelegateSupplier.get() != null;
                mShareDelegateSupplier.get().share(
                        params, new ChromeShareExtras.Builder().build(), ShareOrigin.WEBSHARE_API);
            }
        };

        return new ShareServiceImpl(mWebContents, delegate);
    }
}
