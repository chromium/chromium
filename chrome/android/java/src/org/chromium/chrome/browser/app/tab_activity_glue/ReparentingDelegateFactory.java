// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/** Constructs delegates needed for reparenting tabs. */
public class ReparentingDelegateFactory {
    /**
     * @return Creates an implementation of {@link ReparentingTask.Delegate} that supplies
     *         dependencies for {@link ReparentingTask} to reparent a Tab.
     */
    public static ReparentingTask.Delegate createReparentingTaskDelegate(
            final CompositorViewHolder compositorViewHolder,
            final WindowAndroid windowAndroid,
            TabDelegateFactory tabDelegateFactory) {
        return new ReparentingTask.Delegate() {
            @Override
            public CompositorViewHolder getCompositorViewHolder() {
                return compositorViewHolder;
            }

            @Override
            public WindowAndroid getWindowAndroid() {
                return windowAndroid;
            }

            @Override
            public TabDelegateFactory getTabDelegateFactory() {
                return tabDelegateFactory;
            }
        };
    }

    /**
     * @return Creates an implementation of {@link TabReparentingController.Delegate} that
     *         supplies dependencies to {@link TabReparentingController}.
     */
    public static TabReparentingController.Delegate createReparentingControllerDelegate(
            final TabModelSelector tabModelSelector) {
        return new TabReparentingController.Delegate() {
            @Override
            public TabModelSelector getTabModelSelector() {
                return tabModelSelector;
            }

            @Override
            public boolean isNtpUrl(GURL url) {
                return UrlUtilities.isNtpUrl(url);
            }
        };
    }
}
