// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import org.chromium.build.annotations.NullMarked;

/**
 * Implemented by objects that host the settings UI, either an Activity (e.g. {@code
 * SettingsActivity}) or a Fragment (e.g. {@code SettingsHostFragment}).
 *
 * <p>Settings can be displayed either in a standalone activity or inside a browser tab. Which one
 * is used is decided once, when settings is opened, by {@link
 * SettingsInTab#shouldOpenSettingsInTab()} for the app menu or by the {@code chrome://settings} URL
 * handling. That decision must then remain fixed for the lifetime of the settings UI, even if the
 * screen width changes (e.g. folding a foldable device or changing the display density on
 * automotive). See crbug.com/562619494.
 *
 * <p>Code that needs to know how the settings UI it belongs to is hosted must therefore consult
 * this interface (usually via {@link SettingsHostUtil}) rather than re-deriving the answer from the
 * current screen width.
 */
@NullMarked
public interface SettingsHost {
    /**
     * Returns whether this settings UI is hosted inside a browser tab, rather than a standalone
     * settings activity. The value is fixed for the lifetime of the host.
     */
    boolean isShownInTab();
}
