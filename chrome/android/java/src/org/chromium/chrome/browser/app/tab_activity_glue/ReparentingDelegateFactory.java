// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.ui.base.WindowAndroid;

/** Constructs delegates needed for reparenting tabs. */
@NullMarked
public class ReparentingDelegateFactory {
    /**
     * @return Creates an implementation of {@link ReparentingTask.Delegate} that supplies
     *     dependencies for {@link ReparentingTask} to reparent a Tab.
     */
    public static ReparentingTask.Delegate createReparentingTaskDelegate(
            final @Nullable CompositorViewHolder compositorViewHolder,
            final WindowAndroid windowAndroid,
            @Nullable TabDelegateFactory tabDelegateFactory) {
        return new ReparentingTask.Delegate() {
            @Override
            public @Nullable CompositorViewHolder getCompositorViewHolder() {
                return compositorViewHolder;
            }

            @Override
            public WindowAndroid getWindowAndroid() {
                return windowAndroid;
            }

            @Override
            public @Nullable TabDelegateFactory getTabDelegateFactory() {
                return tabDelegateFactory;
            }
        };
    }
}
