// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;

/** Observer for events that are relevant to TabGroup suggestion triggering or calculation. */
public class SuggestionEventObserver {

    private final @NonNull TabModel mTabModel;
    private final @NonNull TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void didSelectTab(Tab tab, int type, int lastId) {}

                @Override
                public void didAddTab(
                        Tab tab,
                        @TabLaunchType int type,
                        @TabCreationState int creationState,
                        boolean markedForSelection) {}
            };
    ;

    /** Creates the observer. */
    public SuggestionEventObserver(
            @NonNull TabModel tabModel,
            @NonNull ObservableSupplier<Boolean> hubVisibilitySupplier,
            @NonNull ObservableSupplier<Pane> focusedPaneSupplier) {
        mTabModel = tabModel;
        assert !tabModel.isIncognitoBranded();
        tabModel.addObserver(mTabModelObserver);
    }

    /** Destroys the observer. */
    public void destroy() {
        mTabModel.removeObserver(mTabModelObserver);
    }
}
