// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.ObserverList;

/** A provider that notifies its observers when incognito mode is entered or exited. */
public class IncognitoStateProvider {
    /** An interface to be notified about changes to the incognito state. */
    public interface IncognitoStateObserver {
        /** Called when incognito state changes. */
        void onIncognitoStateChanged(boolean isIncognito);
    }

    /** List of {@link IncognitoStateObserver}s. These are used to broadcast events to listeners. */
    private final ObserverList<IncognitoStateObserver> mIncognitoStateObservers;

    /** A {@link TabModelSelectorObserver} used to know when incognito mode is entered or exited. */
    private final TabModelSelectorObserver mTabModelSelectorObserver;

    /** A {@link TabModelSelector} used to know when incognito mode is entered or exited. */
    private TabModelSelector mTabModelSelector;

    public IncognitoStateProvider() {
        mIncognitoStateObservers = new ObserverList<IncognitoStateObserver>();

        mTabModelSelectorObserver = new TabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                // TODO(jinsukkim): Emit this only if the state is different.
                emitIncognitoStateChanged(newModel.isIncognito());
            }
        };
    }

    /**
     * @return Whether incognito mode is currently selected.
     */
    public boolean isIncognitoSelected() {
        return mTabModelSelector != null ? mTabModelSelector.isIncognitoSelected() : false;
    }

    /**
     * @param observer Add an observer to be notified of incognito state changes. Calls
     *                 #onIncognitoStateChanged() on the added observer.
     */
    public void addIncognitoStateObserverAndTrigger(IncognitoStateObserver observer) {
        mIncognitoStateObservers.addObserver(observer);
        observer.onIncognitoStateChanged(isIncognitoSelected());
    }

    /**
     * @param observer Remove the observer.
     */
    public void removeObserver(IncognitoStateObserver observer) {
        mIncognitoStateObservers.removeObserver(observer);
    }

    /**
     * @param tabModelSelector {@link TabModelSelector} to set.
     */
    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
        emitIncognitoStateChanged(mTabModelSelector.isIncognitoSelected());
    }

    /**
     * Destroy {@link IncognitoStateProvider} object.
     */
    public void destroy() {
        if (mTabModelSelector != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            mTabModelSelector = null;
        }
        mIncognitoStateObservers.clear();
    }

    /**
     * Update incognito-selected state.
     * @param isIncognito Whether incognito mode is selected.
     */
    private void emitIncognitoStateChanged(boolean isIncognito) {
        for (IncognitoStateObserver observer : mIncognitoStateObservers) {
            observer.onIncognitoStateChanged(isIncognito);
        }
    }
}
