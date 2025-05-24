// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.graphics.Color;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.util.ColorUtils;

import java.io.File;

/** Object that contains the state of a tab, including its navigation history. */
@NullMarked
public class TabState {
    /** Special value for timestamp related attributes. */
    public static final long TIMESTAMP_NOT_SET = -1;

    /** A theme color that indicates an unspecified state. */
    public static final int UNSPECIFIED_THEME_COLOR = Color.TRANSPARENT;

    /** Navigation history of the WebContents. */
    public @Nullable WebContentsState contentsState;

    public int parentId = Tab.INVALID_TAB_ID;

    /**
     * The legacy tab group ID. This field is replaced by {@link tabGroupId}.
     *
     * @deprecated Use {@link tabGroupId} instead. This field will continue to exist and be updated
     *     in the near term. However, any new code should use {@link tabGroupId} and old code should
     *     migrate off of root id.
     */
    @Deprecated public int rootId;

    /** The tab group ID. */
    public @Nullable Token tabGroupId;

    public long timestampMillis = TIMESTAMP_NOT_SET;
    public @Nullable String openerAppId;

    /**
     * The tab's brand theme color. Set this to {@link #UNSPECIFIED_THEME_COLOR} for an unspecified
     * state.
     */
    public int themeColor = UNSPECIFIED_THEME_COLOR;

    public @TabLaunchType int tabLaunchTypeAtCreation;

    public boolean tabHasSensitiveContent;

    /** Whether this TabState was created from a file containing info about an incognito Tab. */
    public boolean isIncognito;

    /** Tab level Request Desktop Site setting. */
    public @TabUserAgent int userAgent;

    public long lastNavigationCommittedTimestampMillis = TIMESTAMP_NOT_SET;

    // Flag to signal TabState should be migrated to new FlatBuffer format.
    // This field is not persisted on disk.
    public boolean shouldMigrate;

    // Temporary field indicating which legacy TabState file to delete (if
    // applicable). Legacy TabState file should be deleted if the Tab has
    // been migrated onto the new FlatBuffer format.
    public @Nullable File legacyFileToDelete;

    /* Indicates whether the tab is pinned. */
    public boolean isPinned;

    /** Returns true if the tab has a theme color set. */
    public boolean hasThemeColor() {
        return themeColor != UNSPECIFIED_THEME_COLOR
                && !ColorUtils.isThemeColorTooBright(themeColor);
    }
}
