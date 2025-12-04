// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import org.chromium.build.annotations.NullMarked;
import org.chromium.cc.input.OffsetTag;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the bookmark bar scene layer. */
@NullMarked
class BookmarkBarSceneLayerProperties {

    /** The resource ID for the current snapshot */
    public static final WritableIntPropertyKey RESOURCE_ID = new WritableIntPropertyKey();

    /** SceneLayer width, which is the full bookmarks bar. */
    public static final WritableIntPropertyKey SCENE_LAYER_WIDTH = new WritableIntPropertyKey();

    /** SceneLayer height, which is the full bookmarks bar. */
    public static final WritableIntPropertyKey SCENE_LAYER_HEIGHT = new WritableIntPropertyKey();

    /** SceneLayer offset, used to offset the entire view based on current scroll position. */
    public static final WritableIntPropertyKey SCENE_LAYER_OFFSET_HEIGHT =
            new WritableIntPropertyKey();

    /** Snapshot offset width to center the snapshot horizontally within the SceneLayer. */
    public static final WritableIntPropertyKey SNAPSHOT_OFFSET_WIDTH = new WritableIntPropertyKey();

    /** Snapshot offset height to center the snapshot vertically within the SceneLayer. */
    public static final WritableIntPropertyKey SNAPSHOT_OFFSET_HEIGHT =
            new WritableIntPropertyKey();

    /** Background for the layer which the SceneLayer is drawn on. */
    public static final WritableIntPropertyKey BACKGROUND_COLOR = new WritableIntPropertyKey();

    /** Height of the hairline at the bottom of the view. */
    public static final WritableIntPropertyKey HAIRLINE_HEIGHT = new WritableIntPropertyKey();

    /** Background for the hairline at the bottom of the view. */
    public static final WritableIntPropertyKey HAIRLINE_BACKGROUND_COLOR =
            new WritableIntPropertyKey();

    /** The OffsetTag indicating that this layer should be moved by viz. */
    public static final WritableObjectPropertyKey<OffsetTag> OFFSET_TAG =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                RESOURCE_ID,
                SCENE_LAYER_WIDTH,
                SCENE_LAYER_HEIGHT,
                SCENE_LAYER_OFFSET_HEIGHT,
                SNAPSHOT_OFFSET_WIDTH,
                SNAPSHOT_OFFSET_HEIGHT,
                BACKGROUND_COLOR,
                HAIRLINE_HEIGHT,
                HAIRLINE_BACKGROUND_COLOR,
                OFFSET_TAG,
            };
}
