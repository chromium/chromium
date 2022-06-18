// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator.ContextMenuMode;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.externalauth.ExternalAuthUtils;

/**
 * Factory for creating {@link ContextMenuPopulator}s.
 */
public class ChromeContextMenuPopulatorFactory implements ContextMenuPopulatorFactory {
    private final ContextMenuItemDelegate mItemDelegate;
    private final @ContextMenuMode int mContextMenuMode;
    private final ExternalAuthUtils mExternalAuthUtils;

    public ChromeContextMenuPopulatorFactory(@NonNull ContextMenuItemDelegate itemDelegate,
            @ContextMenuMode int contextMenuMode,
            ExternalAuthUtils externalAuthUtils) {
        mItemDelegate = itemDelegate;
        mContextMenuMode = contextMenuMode;
        mExternalAuthUtils = externalAuthUtils;
    }

    @Override
    public void onDestroy() {
        mItemDelegate.onDestroy();
    }

    @Override
    public ContextMenuPopulator createContextMenuPopulator(
            Context context, ContextMenuParams params, ContextMenuNativeDelegate nativeDelegate) {
        return new ChromeContextMenuPopulator(mItemDelegate,
                mContextMenuMode, mExternalAuthUtils, context, params, nativeDelegate);
    }
}
