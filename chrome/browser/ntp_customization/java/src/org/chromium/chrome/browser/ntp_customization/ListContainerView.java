// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import org.chromium.build.annotations.NullMarked;

/** A interface for a UI View that acts as a container for a list of items. */
@NullMarked
public interface ListContainerView {
    /**
     * Renders a complete list of items in the container using data provided by the delegate.
     *
     * @param delegate The delegate that provides the data for the list.
     */
    void renderAllListItems(ListContainerViewDelegate delegate);

    /** Cleans up any resources held by the container to prevent memory leaks. */
    void destroy();
}
