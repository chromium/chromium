// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.app.tabmodel;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.pdf.PdfInfo;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.external_intents.ExternalNavigationHandler;

/** A fake/stub implementation since none of these dependencies exist in headless mode. */
public class HeadlessTabDelegateFactory implements TabDelegateFactory {
    @Override
    public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
        return null;
    }

    @Override
    public ExternalNavigationHandler createExternalNavigationHandler(Tab tab) {
        return null;
    }

    @Nullable
    @Override
    public ContextMenuPopulatorFactory createContextMenuPopulatorFactory(Tab tab) {
        return null;
    }

    @Override
    public BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(Tab tab) {
        return null;
    }

    @Nullable
    @Override
    public NativePage createNativePage(
            String url, NativePage candidatePage, Tab tab, PdfInfo pdfInfo) {
        return null;
    }
}
