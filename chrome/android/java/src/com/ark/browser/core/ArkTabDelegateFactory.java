// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser.core;

import com.ark.browser.tab.ArkExternalNavigationDelegateImpl;
import com.ark.browser.tab.ArkTabStateBrowserControlsVisibilityDelegate;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulatorFactory;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.util.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.components.external_intents.ExternalNavigationHandler;

/**
 * {@link TabDelegateFactory} class to be used in all {@link Tab} instances owned by a
 * {@link ChromeTabbedActivity}.
 */
public class ArkTabDelegateFactory implements TabDelegateFactory {

    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final FullscreenManager mFullscreenManager;
    private final Supplier<ArkCompositorViewHolder> mCompositorViewHolderSupplier;

    public ArkTabDelegateFactory(BrowserControlsStateProvider browserControlsStateProvider,
                                 FullscreenManager fullscreenManager,
                                 Supplier<ArkCompositorViewHolder> compositorViewHolderSupplier) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mFullscreenManager = fullscreenManager;
        mCompositorViewHolderSupplier = compositorViewHolderSupplier;
    }

    @Override
    public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
        return new ArkTabWebContentsDelegateAndroid(tab,
                mBrowserControlsStateProvider, mFullscreenManager,
                mCompositorViewHolderSupplier);
    }

    @Override
    public ExternalNavigationHandler createExternalNavigationHandler(Tab tab) {
        return new ExternalNavigationHandler(new ArkExternalNavigationDelegateImpl(tab));
    }

    @Override
    public ContextMenuPopulatorFactory createContextMenuPopulatorFactory(Tab tab) {
        return new ChromeContextMenuPopulatorFactory(
                new ArkTabContextMenuItemDelegate(tab),
                ChromeContextMenuPopulator.ContextMenuMode.NORMAL);
    }

    @Override
    public BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(Tab tab) {
        return new ComposedBrowserControlsVisibilityDelegate(
                new ArkTabStateBrowserControlsVisibilityDelegate(tab));
    }

    /** Destroy and unhook objects at destruction. */
    public void destroy() {
    }
}
