// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;

import androidx.browser.customtabs.CustomContentAction;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator.ContextMenuMode;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.TabContextMenuItemDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuNativeDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulator;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;

import java.util.List;
import java.util.function.Supplier;

/** Factory for creating {@link ContextMenuPopulator}s. */
@NullMarked
public class ChromeContextMenuPopulatorFactory implements ContextMenuPopulatorFactory {
    private static @Nullable ShareDelegate sShareDelegateForTesting;
    private final TabContextMenuItemDelegate mItemDelegate;
    private final Supplier<@Nullable ShareDelegate> mShareDelegateSupplier;
    private final @ContextMenuMode int mContextMenuMode;
    private final List<CustomContentAction> mCustomContentActions;

    public ChromeContextMenuPopulatorFactory(
            TabContextMenuItemDelegate itemDelegate,
            Supplier<@Nullable ShareDelegate> shareDelegate,
            @ContextMenuMode int contextMenuMode,
            List<CustomContentAction> customContentActions) {
        mItemDelegate = itemDelegate;
        mShareDelegateSupplier = shareDelegate;
        mContextMenuMode = contextMenuMode;
        if (ChromeFeatureList.sCctContextualMenuItems.isEnabled()) {
            mCustomContentActions = customContentActions;
        } else {
            mCustomContentActions = List.of();
        }
    }

    @Override
    public void onDestroy() {
        mItemDelegate.onDestroy();
    }

    public static void setShareDelegateForTesting(ShareDelegate shareDelegate) {
        sShareDelegateForTesting = shareDelegate;
        ResettersForTesting.register(() -> sShareDelegateForTesting = null);
    }

    @Override
    public ContextMenuPopulator createContextMenuPopulator(
            Context context, ContextMenuParams params, ContextMenuNativeDelegate nativeDelegate) {
        return new ChromeContextMenuPopulator(
                mItemDelegate,
                sShareDelegateForTesting != null
                        ? (Supplier<@Nullable ShareDelegate>)
                                SupplierUtils.of(sShareDelegateForTesting)
                        : mShareDelegateSupplier,
                mCustomContentActions,
                mContextMenuMode,
                context,
                params,
                nativeDelegate);
    }
}
