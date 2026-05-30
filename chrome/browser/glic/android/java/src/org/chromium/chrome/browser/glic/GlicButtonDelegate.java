// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlicInvocationSource;

/** Delegate interface for handling clicks on the Glic toolbar button. */
@NullMarked
@FunctionalInterface
public interface GlicButtonDelegate {
    /**
     * Called when the button is clicked.
     *
     * @param preventClose True if the panel should not be closed.
     * @param invocationSource The source of the Glic activation.
     */
    void onClick(boolean preventClose, @GlicInvocationSource int invocationSource);
}
