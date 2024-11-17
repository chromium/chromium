// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.core.util.Pair;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesView;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupFaviconCluster.ClusterData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for displaying a single tab group row. */
public class TabGroupRowProperties {
    public static final ReadableObjectPropertyKey<ClusterData> CLUSTER_DATA =
            new WritableObjectPropertyKey();

    // Data Sharing properties.
    public static final WritableBooleanPropertyKey DISPLAY_AS_SHARED =
            new WritableBooleanPropertyKey();

    public static final ReadableIntPropertyKey COLOR_INDEX = new ReadableIntPropertyKey();
    // First is the user title, second is the number of tabs.
    public static final ReadableObjectPropertyKey<Pair<String, Integer>> TITLE_DATA =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Long> CREATION_MILLIS =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Runnable> OPEN_RUNNABLE =
            new ReadableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> DELETE_RUNNABLE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> LEAVE_RUNNABLE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<SharedImageTilesView> SHARED_IMAGE_TILES_VIEW =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Destroyable> DESTROYABLE =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        CLUSTER_DATA,
        DISPLAY_AS_SHARED,
        COLOR_INDEX,
        TITLE_DATA,
        CREATION_MILLIS,
        OPEN_RUNNABLE,
        DELETE_RUNNABLE,
        LEAVE_RUNNABLE,
        SHARED_IMAGE_TILES_VIEW,
        DESTROYABLE
    };
}
