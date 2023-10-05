// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.widget.FrameLayout;

/**
 * Root coordinator of the Hub.
 *
 * <p>TODO(crbug/1487315): This is a stub implementation make it display the Hub UI.
 */
public class HubCoordinator {
    private final FrameLayout mContainerView;

    /**
     * Creates the {@link HubCoordinator}.
     *
     * @param containerView The view to attach the Hub to.
     */
    public HubCoordinator(FrameLayout containerView) {
        mContainerView = containerView;
    }

    public void destroy() {}
}
