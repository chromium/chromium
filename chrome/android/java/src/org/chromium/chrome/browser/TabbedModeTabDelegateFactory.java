// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.app.tab_activity_glue.ActivityTabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulatorFactory;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.init.ChromeActivityNativeDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabContextMenuItemDelegate;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.util.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * {@link TabDelegateFactory} class to be used in all {@link Tab} instances owned by a
 * {@link ChromeTabbedActivity}.
 */
public class TabbedModeTabDelegateFactory implements TabDelegateFactory {
    private final Activity mActivity;
    private final ChromeActivityNativeDelegate mChromeActivityNativeDelegate;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final FullscreenManager mFullscreenManager;
    private final TabCreatorManager mTabCreatorManager;
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final Supplier<CompositorViewHolder> mCompositorViewHolderSupplier;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final Supplier<SnackbarManager> mSnackbarManagerSupplier;

    public TabbedModeTabDelegateFactory(Activity activity,
            ChromeActivityNativeDelegate chromeActivityNativeDelegate,
            BrowserControlsStateProvider browserControlsStateProvider,
            FullscreenManager fullscreenManager, TabCreatorManager tabCreatorManager,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Supplier<SnackbarManager> snackbarManagerSupplier) {
        mActivity = activity;
        mChromeActivityNativeDelegate = chromeActivityNativeDelegate;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mFullscreenManager = fullscreenManager;
        mTabCreatorManager = tabCreatorManager;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mCompositorViewHolderSupplier = compositorViewHolderSupplier;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
    }

    @Override
    public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
        return new ActivityTabWebContentsDelegateAndroid(tab, mActivity,
                mChromeActivityNativeDelegate,
                mBrowserControlsStateProvider, mFullscreenManager, mTabCreatorManager,
                mTabModelSelectorSupplier, mCompositorViewHolderSupplier,
                mModalDialogManagerSupplier);
    }

    @Override
    public ExternalNavigationHandler createExternalNavigationHandler(Tab tab) {
        return new ExternalNavigationHandler(new ExternalNavigationDelegateImpl(tab));
    }

    @Override
    public ContextMenuPopulatorFactory createContextMenuPopulatorFactory(Tab tab) {
        return new ChromeContextMenuPopulatorFactory(
                new TabContextMenuItemDelegate(tab, mTabModelSelectorSupplier.get(),
                        mSnackbarManagerSupplier),
                ChromeContextMenuPopulator.ContextMenuMode.NORMAL);
    }

    @Override
    public BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(Tab tab) {
        return new ComposedBrowserControlsVisibilityDelegate(
                new TabStateBrowserControlsVisibilityDelegate(tab));
    }

    /** Destroy and unhook objects at destruction. */
    public void destroy() {
    }
}
