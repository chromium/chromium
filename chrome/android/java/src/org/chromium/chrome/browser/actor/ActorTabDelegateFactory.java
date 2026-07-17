// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.pdf.PdfInfo;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.util.PictureInPictureWindowOptions;
import org.chromium.chrome.browser.util.WindowFeatures;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** Tab delegate factory for Actor background tabs. */
@NullMarked
class ActorTabDelegateFactory implements TabDelegateFactory {
    @Override
    public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
        return new ActorTabWebContentsDelegate();
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

    private static class ActorTabWebContentsDelegate extends TabWebContentsDelegateAndroid {
        @Override
        public boolean shouldResumeRequestsForCreatedWindow() {
            return true;
        }

        @Override
        public boolean addNewContents(
                WebContents sourceWebContents,
                WebContents webContents,
                GURL targetUrl,
                int disposition,
                WindowFeatures windowFeatures,
                boolean userGesture,
                @Nullable PictureInPictureWindowOptions pictureInPictureWindowOptions) {
            return false;
        }

        @Override
        public void setOverlayMode(boolean useOverlayMode) {}

        @Override
        public void destroy() {}
    }
}
