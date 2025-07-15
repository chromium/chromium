// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion.toolbar;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabwindow.WindowId;

/** Implementation of {@link GroupSuggestionsButtonController} TODO(salg): Add implementation. */
@NullMarked
public class GroupSuggestionsButtonControllerImpl implements GroupSuggestionsButtonController {

    @Override
    public boolean shouldShowButton(Tab tab, @WindowId int windowId) {
        return false;
    }

    @Override
    public void onButtonShown(Tab tab) {}

    @Override
    public void onButtonHidden() {}

    @Override
    public void onButtonClicked(Tab tab, TabGroupModelFilter tabGroupModelFilter) {}
}
