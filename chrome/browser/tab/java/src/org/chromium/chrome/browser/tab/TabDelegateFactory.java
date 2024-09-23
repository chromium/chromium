// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.pdf.PdfInfo;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;

/** An interface for factory to create {@link Tab} related delegates. */
public interface TabDelegateFactory {
    /**
     * Creates the {@link WebContentsDelegateAndroid} the tab will be initialized with.
     * @param tab The associated {@link Tab}.
     * @return The {@link WebContentsDelegateAndroid} to be used for this tab.
     */
    TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab);

    /**
     * Creates the {@link ExternalNavigationHandler} the tab will use for its
     * {@link InterceptNavigationDelegate}.
     * @param tab The associated {@link Tab}.
     * @return The {@link ExternalNavigationHandler} to be used for this tab.
     */
    ExternalNavigationHandler createExternalNavigationHandler(Tab tab);

    /**
     * Creates the {@link ContextMenuPopulatorFactory} the tab will be initialized with.
     *
     * @param tab The associated {@link Tab}.
     * @return The {@link ContextMenuPopulatorFactory} to be used for this tab. {@code null} if the
     *     context menu feature is to be disabled.
     */
    @Nullable
    ContextMenuPopulatorFactory createContextMenuPopulatorFactory(Tab tab);

    /**
     * Creates the {@link BrowserControlsVisibilityDelegate} the tab will be initialized with.
     *
     * @param tab The associated {@link Tab}.
     */
    BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(Tab tab);

    /**
     * Returns a NativePage for displaying the given URL if the URL is a valid chrome-native URL, or
     * represents a pdf file. Otherwise returns null. If candidatePage is non-null and corresponds
     * to the URL, it will be returned. Otherwise, a new NativePage will be constructed.
     *
     * @param url The URL to be handled.
     * @param candidatePage A NativePage to be reused if it matches the url, or null.
     * @param tab The Tab that will show the page.
     * @param pdfInfo Information of the pdf, or null if not pdf.
     * @return A NativePage showing the specified url or null.
     */
    @Nullable
    NativePage createNativePage(String url, NativePage candidatePage, Tab tab, PdfInfo pdfInfo);
}
