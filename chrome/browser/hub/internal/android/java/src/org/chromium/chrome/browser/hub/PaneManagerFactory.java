// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

/** Factory for creating {@link PaneManager}. */
public class PaneManagerFactory {
    /**
     * Creates a new instance of {@link PaneManagerImpl}.
     * @param paneListBuilder The {@link PaneListBuilder} which is consumed to build the set of
     *                        {@link Pane}s to manage.
     * @return an instance of {@link PaneManagerImpl}.
     */
    public static PaneManager createPaneManager(PaneListBuilder paneListBuilder) {
        return new PaneManagerImpl(paneListBuilder);
    }
}
