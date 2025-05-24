// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.build.annotations.NullMarked;

/**
 * An interface for creating an animator for the Empty Tab List illustration. Helps with providing
 * flexibility for swapping animations for phone and tablet illustrations.
 */
@NullMarked
public interface TabListEmptyIllustrationAnimationManager {
    /** Runs an animation for the Empty Tab List Illustration. */
    void animate(long durationMs);

    /** Applies an initial transformation on the Empty Tab List Illustration. */
    void initialTransformation();
}
