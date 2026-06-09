// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabwindow;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Holds objects related to a tab and where it lives. CCTs and archived tabs will have an invalid
 * windowId {@link TabWindowManager#INVALID_WINDOW_ID}.
 */
@NullMarked
public class TabWindowInfo {
    /** The different types of selector instances. */
    @IntDef({
        TabWindowType.INVALID,
        TabWindowType.TABBED,
        TabWindowType.HEADLESS,
        TabWindowType.CUSTOM,
        TabWindowType.ARCHIVED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabWindowType {
        int INVALID = -1;
        int TABBED = 0;
        int HEADLESS = 1;
        int CUSTOM = 2;
        int ARCHIVED = 3;
    }

    public final @WindowId int windowId;
    public final TabModelSelector tabModelSelector;
    public final TabModel tabModel;
    public final Tab tab;
    public final @TabWindowType int type;

    public TabWindowInfo(
            @WindowId int windowId, TabModelSelector tabModelSelector, TabModel tabModel, Tab tab) {
        this.tab = tab;
        this.tabModel = tabModel;
        this.tabModelSelector = tabModelSelector;
        this.windowId = windowId;
        switch (tabModel.getTabModelType()) {
            case TabModelType.STANDARD:
                this.type = tab.isCustomTab() ? TabWindowType.CUSTOM : TabWindowType.TABBED;
                break;
            case TabModelType.HEADLESS:
                this.type = TabWindowType.HEADLESS;
                break;
            case TabModelType.ARCHIVED:
                this.type = TabWindowType.ARCHIVED;
                break;
            default:
                this.type = TabWindowType.INVALID;
                break;
        }
    }
}
