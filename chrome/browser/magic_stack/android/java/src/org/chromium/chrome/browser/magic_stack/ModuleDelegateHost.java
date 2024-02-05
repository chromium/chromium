// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import android.graphics.Point;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.BrowserUiUtils.HostSurface;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.url.GURL;

/** The interface of the host surface which owns the magic stack. */
public interface ModuleDelegateHost {

    /** Gets the type of the host surface. */
    @HostSurface
    int getHostSurfaceType();

    /** Gets the starting position of the context menu which is shown by long clicking a module. */
    @NonNull
    Point getContextMenuStartPoint();

    /** Gets the instance of {@link UiConfig} of the host surface. */
    @Nullable
    UiConfig getUiConfig();

    /**
     * Called when the user clicks a module to open a URL.
     *
     * @param gurl The URL to open.
     */
    void onUrlClicked(GURL gurl);

    /**
     * Called when the user clicks a module to select a Tab.
     *
     * @param tabId The id of the Tab to select.
     * @param moduleType The type of the module on which the Tab is selected.
     */
    void onTabSelected(int tabId, @ModuleType int moduleType);

    /** Called when the capture thumbnail status changed. */
    default void onCaptureThumbnailStatusChanged() {}

    /** Opens the settings to customize home modules. */
    void customizeSettings();

    /** Returns whether the home surface has scrollable MV tiles. */
    boolean showScrollableMvt();

    /**
     * Returns the start margin of the magic stack in pixel. It is used to calculate the scrolling
     * offset of the recyclerview item.
     */
    int getStartMargin();

    /**
     * Returns the tab that the home surface is tracking. It is non-null on NTP home surface only.
     */
    @Nullable
    default Tab getTrackingTab() {
        return null;
    }
}
