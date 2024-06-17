// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.graphics.Rect;

import androidx.annotation.VisibleForTesting;

import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.pdf.PdfInfo;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.content_public.browser.WebContents;

/**
 * Minimalistic implementation of a {@link TabDelegateFactory} that creates trivial,
 * non-interactable tabs.
 */
class SearchActivityTabDelegateFactory implements TabDelegateFactory {
    @VisibleForTesting
    /* package */ static class WebContentsDelegate extends TabWebContentsDelegateAndroid {
        @Override
        public int getDisplayMode() {
            return DisplayMode.BROWSER;
        }

        @Override
        @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
        public boolean shouldResumeRequestsForCreatedWindow() {
            return false;
        }

        @Override
        @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
        public boolean addNewContents(
                WebContents sourceWebContents,
                WebContents webContents,
                int disposition,
                Rect initialPosition,
                boolean userGesture) {
            return false;
        }

        @Override
        @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
        public void setOverlayMode(boolean useOverlayMode) {}

        @Override
        public boolean canShowAppBanners() {
            return false;
        }
    }

    @Override
    public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
        return new WebContentsDelegate();
    }

    @Override
    public ExternalNavigationHandler createExternalNavigationHandler(Tab tab) {
        return null;
    }

    @Override
    public ContextMenuPopulatorFactory createContextMenuPopulatorFactory(Tab tab) {
        return null;
    }

    @Override
    public BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(Tab tab) {
        return null;
    }

    @Override
    public NativePage createNativePage(
            String url, NativePage candidatePage, Tab tab, PdfInfo pdfInfo) {
        return null;
    }
}
