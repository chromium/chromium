// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Factory for creating HubBottomToolbarDelegate instances.
 *
 * <p>This factory provides a central point for delegate creation, allowing downstream
 * implementations to override the default behavior while maintaining upstream compatibility.
 *
 * <p>By default, this factory returns null, meaning no bottom toolbar functionality is provided in
 * upstream Chromium. Downstream implementations can override the factory method to provide their
 * own delegate implementations.
 */
@NullMarked
public class HubBottomToolbarDelegateFactory {
    /** Test-only override for the delegate instance. */
    private static @Nullable HubBottomToolbarDelegate sDelegateForTesting;

    /**
     * Creates a HubBottomToolbarDelegate instance.
     *
     * <p>This method returns null by default in upstream Chromium, ensuring no bottom toolbar
     * functionality is active. Downstream implementations can override this method to return their
     * own delegate implementations.
     *
     * @return A HubBottomToolbarDelegate instance, or null if no bottom toolbar functionality
     *     should be provided.
     */
    public static @Nullable HubBottomToolbarDelegate createDelegate() {
        if (sDelegateForTesting != null) {
            return sDelegateForTesting;
        }

        // Upstream default: no bottom toolbar functionality
        // Downstream implementations can override this method to return their delegate
        return null;
    }

    /**
     * Sets a delegate instance for testing purposes only.
     *
     * <p>This method allows tests to provide custom delegate implementations to verify that the
     * integration works correctly without requiring full downstream implementations.
     *
     * @param delegate The delegate to use for testing.
     */
    public static void setDelegateForTesting(@Nullable HubBottomToolbarDelegate delegate) {
        sDelegateForTesting = delegate;
        ResettersForTesting.register(() -> sDelegateForTesting = null);
    }
}
