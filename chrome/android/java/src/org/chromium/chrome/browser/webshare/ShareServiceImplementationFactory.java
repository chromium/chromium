// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webshare;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.share.ShareDelegateSupplier;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.webshare.ShareServiceImpl;
import org.chromium.content_public.browser.PermissionsPolicyFeature;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsStatics;
import org.chromium.services.service_manager.InterfaceFactory;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.webshare.mojom.ShareService;

import java.util.function.Supplier;

/** Factory that creates instances of ShareService. */
@NullMarked
public class ShareServiceImplementationFactory implements InterfaceFactory<@Nullable ShareService> {
    private final RenderFrameHost mRenderFrameHost;
    private @Nullable Supplier<@Nullable ShareDelegate> mShareDelegateSupplier;
    private @Nullable WindowAndroid mWindowAndroid;

    public ShareServiceImplementationFactory(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
        WebContents webContents = WebContentsStatics.fromRenderFrameHost(mRenderFrameHost);
        mWindowAndroid =
                (webContents != null && !webContents.isDestroyed())
                        ? webContents.getTopLevelNativeWindow()
                        : null;
        mShareDelegateSupplier = getShareDelegateSupplier(mWindowAndroid);
    }

    private static @Nullable Supplier<@Nullable ShareDelegate> getShareDelegateSupplier(
            @Nullable WindowAndroid windowAndroid) {
        return windowAndroid == null
                ? null
                : (Supplier<@Nullable ShareDelegate>) ShareDelegateSupplier.from(windowAndroid);
    }

    @Override
    public ShareService createImpl() {
        ShareServiceImpl.WebShareDelegate delegate =
                new ShareServiceImpl.WebShareDelegate() {
                    @Override
                    public boolean canShare() {
                        WebContents webContents =
                                WebContentsStatics.fromRenderFrameHost(mRenderFrameHost);
                        if (webContents == null || webContents.isDestroyed()) return false;
                        return getShareDelegate() != null
                                && mRenderFrameHost.isFeatureEnabled(
                                        PermissionsPolicyFeature.WEB_SHARE);
                    }

                    @Override
                    public void share(ShareParams params) {
                        ShareDelegate shareDelegate = getShareDelegate();
                        if (shareDelegate == null) return;
                        shareDelegate.share(
                                params,
                                new ChromeShareExtras.Builder()
                                        .setDetailedContentType(DetailedContentType.WEB_SHARE)
                                        .setRenderFrameHost(mRenderFrameHost)
                                        .build(),
                                ShareOrigin.WEBSHARE_API);
                    }

                    @Override
                    public @Nullable WindowAndroid getWindowAndroid() {
                        WebContents webContents =
                                WebContentsStatics.fromRenderFrameHost(mRenderFrameHost);
                        if (webContents == null || webContents.isDestroyed()) return null;
                        if (mWindowAndroid == null || mWindowAndroid.isDestroyed()) {
                            mWindowAndroid = webContents.getTopLevelNativeWindow();
                        }
                        return mWindowAndroid;
                    }

                    @Override
                    public void terminateRendererDueToBadMessage(int reason) {
                        mRenderFrameHost.terminateRendererDueToBadMessage(reason);
                    }

                    /**
                     * Returns the current {@link ShareDelegate}, and updates it when the {@link
                     * WindowAndroid} has changed.
                     *
                     * <p>The {@link WindowAndroid} changes when the theme changes, which
                     * necessitates getting a new ShareDelegate. See https://crbug.com/40838216.
                     */
                    private @Nullable ShareDelegate getShareDelegate() {
                        WebContents webContents =
                                WebContentsStatics.fromRenderFrameHost(mRenderFrameHost);
                        if (webContents == null || webContents.isDestroyed()) return null;
                        WindowAndroid currentWindow = webContents.getTopLevelNativeWindow();
                        if (mWindowAndroid != null
                                && mWindowAndroid.equals(currentWindow)
                                && mShareDelegateSupplier != null) {
                            return mShareDelegateSupplier.get();
                        }
                        mWindowAndroid = currentWindow;
                        mShareDelegateSupplier = getShareDelegateSupplier(mWindowAndroid);
                        return mShareDelegateSupplier != null ? mShareDelegateSupplier.get() : null;
                    }
                };

        return new ShareServiceImpl(delegate);
    }
}
