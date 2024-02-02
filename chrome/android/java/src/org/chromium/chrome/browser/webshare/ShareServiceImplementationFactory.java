// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webshare;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.share.ShareDelegateSupplier;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.webshare.ShareServiceImpl;
import org.chromium.content_public.browser.PermissionsPolicyFeature;
import org.chromium.content_public.browser.WebContents;
import org.chromium.services.service_manager.InterfaceFactory;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.webshare.mojom.ShareService;

/** Factory that creates instances of ShareService. */
public class ShareServiceImplementationFactory implements InterfaceFactory<ShareService> {
    private final WebContents mWebContents;
    private Supplier<ShareDelegate> mShareDelegateSupplier;
    private WindowAndroid mWindowAndroid;

    public ShareServiceImplementationFactory(WebContents webContents) {
        mWebContents = webContents;
        mWindowAndroid = mWebContents.getTopLevelNativeWindow();
        mShareDelegateSupplier = ShareDelegateSupplier.from(mWindowAndroid);
        assert mShareDelegateSupplier != null;
    }

    @Override
    public ShareService createImpl() {
        ShareServiceImpl.WebShareDelegate delegate =
                new ShareServiceImpl.WebShareDelegate() {
                    @Override
                    public boolean canShare() {
                        return getShareDelegate() != null
                                && mWebContents
                                        .getMainFrame()
                                        .isFeatureEnabled(PermissionsPolicyFeature.WEB_SHARE);
                    }

                    @Override
                    public void share(ShareParams params) {
                        getShareDelegate()
                                .share(
                                        params,
                                        new ChromeShareExtras.Builder()
                                                .setDetailedContentType(
                                                        DetailedContentType.WEB_SHARE)
                                                .build(),
                                        ShareOrigin.WEBSHARE_API);
                    }

                    @Override
                    public WindowAndroid getWindowAndroid() {
                        if (mWindowAndroid == null || mWindowAndroid.isDestroyed()) {
                            mWindowAndroid = mWebContents.getTopLevelNativeWindow();
                        }
                        return mWindowAndroid;
                    }

                    /**
                     * Returns the current {@link ShareDelegate}, and updates it when the {@link
                     * WindowAndroid} has changed.
                     *
                     * <p>The {@link WindowAndroid} changes when the theme changes, which
                     * necessitates getting a new ShareDelegate. See https://crbug.com/1322778.
                     */
                    private ShareDelegate getShareDelegate() {
                        if (mWindowAndroid.equals(mWebContents.getTopLevelNativeWindow())) {
                            return mShareDelegateSupplier.get();
                        }
                        mWindowAndroid = getWindowAndroid();
                        mShareDelegateSupplier = ShareDelegateSupplier.from(mWindowAndroid);
                        assert mShareDelegateSupplier != null;
                        return mShareDelegateSupplier.get();
                    }
                };

        return new ShareServiceImpl(delegate);
    }
}
