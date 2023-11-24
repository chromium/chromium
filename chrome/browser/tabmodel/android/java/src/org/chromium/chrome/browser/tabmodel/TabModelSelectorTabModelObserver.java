// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;

import java.util.List;

/**
 * Observer for all {@link TabModel}s owned by a {@link TabModelSelector}.
 *
 * <p>
 * This can safely be constructed before native libraries have been initialized as this will
 * register to observe the underlying TabModels as they are created lazily.
 */
public class TabModelSelectorTabModelObserver implements TabModelObserver {
    private final TabModelSelector mTabModelSelector;

    private TabModelSelectorObserver mSelectorObserver;

    /**
     * Constructs an observer that should be notified of changes for all tab models owned
     * by a specified {@link TabModelSelector}.
     *
     * <p>
     * {@link #destroy()} must be called to unregister this observer.
     *
     * @param selector The selector that owns the Tab Models that should notify this observer.
     */
    public TabModelSelectorTabModelObserver(TabModelSelector selector) {
        mTabModelSelector = selector;

        List<TabModel> tabModels = mTabModelSelector.getModels();
        if (tabModels.isEmpty()) {
            mSelectorObserver =
                    new TabModelSelectorObserver() {
                        @Override
                        public void onNewTabCreated(Tab tab, @TabCreationState int creationState) {
                            throw new IllegalStateException(
                                    "onChange should have happened and unregistered this listener.");
                        }

                        @Override
                        public void onChange() {
                            mTabModelSelector.removeObserver(this);
                            mSelectorObserver = null;
                            registerModelObservers();
                        }
                    };
            mTabModelSelector.addObserver(mSelectorObserver);
        } else {
            registerModelObservers();
        }
    }

    private void registerModelObservers() {
        List<TabModel> tabModels = mTabModelSelector.getModels();
        for (int i = 0; i < tabModels.size(); i++) {
            TabModel tabModel = tabModels.get(i);
            tabModel.addObserver(this);
        }

        onRegistrationComplete();
    }

    /** Notifies that the registration of the observers has been completed. */
    protected void onRegistrationComplete() {}

    /** Destroys the observer and removes itself as a listener for Tab updates. */
    public void destroy() {
        if (mSelectorObserver != null) {
            mTabModelSelector.removeObserver(mSelectorObserver);
            mSelectorObserver = null;
        }

        List<TabModel> tabModels = mTabModelSelector.getModels();
        for (int i = 0; i < tabModels.size(); i++) {
            TabModel tabModel = tabModels.get(i);
            tabModel.removeObserver(this);
        }
    }
}
