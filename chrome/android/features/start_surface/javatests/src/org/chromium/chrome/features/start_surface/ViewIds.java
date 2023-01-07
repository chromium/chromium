// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import org.chromium.chrome.R;

/**
 * This class allows external tests to use these view IDs without depending directly on the internal
 * `java_resources` target.
 */
public class ViewIds {
    public static int PRIMARY_TASKS_SURFACE_VIEW = R.id.primary_tasks_surface_view;
    public static int SECONDARY_TASKS_SURFACE_VIEW = R.id.secondary_tasks_surface_view;
}