// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.hooks;

import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.ProcessScopeDependencyProvider;

/**
 * Provides access to internal Feed implementation parts, if they are available.
 */
public interface FeedHooks {
    /** Whether the internal components of the Feed are available.*/
    default boolean isEnabled() {
        return false;
    }
    /** Create a `ProcessScope`. */
    default ProcessScope createProcessScope(ProcessScopeDependencyProvider dependencies) {
        return null;
    }
}
