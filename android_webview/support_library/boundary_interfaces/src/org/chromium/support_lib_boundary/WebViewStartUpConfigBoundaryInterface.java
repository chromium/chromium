// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import org.jspecify.annotations.NullMarked;
import org.jspecify.annotations.Nullable;

import java.util.Set;
import java.util.concurrent.Executor;

/** Boundary interface for WebViewStartUpConfig. */
@NullMarked
public interface WebViewStartUpConfigBoundaryInterface {
    Executor getBackgroundExecutor();

    /** Whether to run only parts of startup that doesn't block the UI thread. */
    boolean shouldRunUiThreadStartUpTasks();

    /**
     * Returns the set of profile names to load during startup.
     *
     * @return A set of profile names, which may include the default profile.
     */
    @Nullable Set<String> getProfileNamesToLoad();
}
