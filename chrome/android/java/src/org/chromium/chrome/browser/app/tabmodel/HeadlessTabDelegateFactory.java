// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.app.tabmodel;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.pdf.PdfInfo;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelType;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.external_intents.ExternalNavigationHandler;

/** A fake/stub implementation since none of these dependencies exist in headless mode. */
@NullMarked
public class HeadlessTabDelegateFactory implements TabDelegateFactory {
    private final @TabModelType int mTabModelType;

    /**
     * @param tabModelType The {@link TabModelType} for this headless delegate factory (must be
     *     either {@link TabModelType#HEADLESS} or {@link TabModelType#ARCHIVED}).
     */
    public HeadlessTabDelegateFactory(@TabModelType int tabModelType) {
        assert TabModel.isDormantTabModel(tabModelType)
                : "HeadlessTabDelegateFactory only supports HEADLESS or ARCHIVED tab model types.";
        mTabModelType = tabModelType;
    }

    @Override
    public @TabModelType int getTabModelType() {
        return mTabModelType;
    }

    @Override
    public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
        throw new AssertionError(
                "Headless tabs must never create a WebContents or WebContentsDelegate.");
    }

    @Override
    public @Nullable ExternalNavigationHandler createExternalNavigationHandler(Tab tab) {
        return null;
    }

    @Override
    public @Nullable ContextMenuPopulatorFactory createContextMenuPopulatorFactory(Tab tab) {
        return null;
    }

    @Override
    public @Nullable BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(
            Tab tab) {
        return null;
    }

    @Override
    public @Nullable NativePage createNativePage(
            String url, @Nullable NativePage candidatePage, Tab tab, @Nullable PdfInfo pdfInfo) {
        return null;
    }

    @Override
    public boolean isCustomTab() {
        return false;
    }

    @Override
    public boolean isTabInPwa() {
        return false;
    }

    @Override
    public boolean isTabInBrowser() {
        return true;
    }

    @Override
    public boolean isTabInPopup() {
        return false;
    }
}
