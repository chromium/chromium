// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateAttributes.DirtinessState;
import org.chromium.chrome.browser.tab.TabStateStorageService;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabRegistrationObserver;

/** Orchestrates saving of tabs to the {@link TabStateStorageService}. */
@NullMarked
public class TabStateStore {
    private final TabStateStorageService mTabStateStorageService;
    private final TabStateAttributes.Observer mAttributesObserver =
            this::onTabStateDirtinessChanged;

    private final TabModelSelectorTabRegistrationObserver mTabRegistrationObserver;

    /**
     * @param tabStateStorageService The {@link TabStateStorageService} to save to.
     * @param tabModelSelector The {@link TabModelSelector} to observe changes in.
     */
    public TabStateStore(
            TabStateStorageService tabStateStorageService, TabModelSelector tabModelSelector) {
        mTabStateStorageService = tabStateStorageService;
        mTabRegistrationObserver = new TabModelSelectorTabRegistrationObserver(tabModelSelector);
        mTabRegistrationObserver.addObserverAndNotifyExistingTabRegistration(
                new TabModelSelectorTabRegistrationObserver.Observer() {
                    @Override
                    public void onTabRegistered(Tab tab) {
                        TabStateStore.this.onTabRegistered(tab);
                    }

                    @Override
                    public void onTabUnregistered(Tab tab) {
                        TabStateStore.this.onTabUnregistered(tab);
                    }
                });
    }

    /** Cleans up observation. */
    public void destroy() {
        mTabRegistrationObserver.destroy();
    }

    private void onTabStateDirtinessChanged(Tab tab, @DirtinessState int dirtiness) {
        if (dirtiness == DirtinessState.DIRTY && !tab.isDestroyed()) {
            saveTab(tab);
        }
    }

    private void saveTab(Tab tab) {
        WebContentsState state = tab.getWebContentsState();
        mTabStateStorageService.saveTabData(
                tab.getId(),
                // TODO(https://crbug.com/427254267): Provide a parentCollectionId.
                0,
                // TODO(https://crbug.com/427254267): Provide a position.
                "",
                tab.getParentId(),
                tab.getRootId(),
                tab.getTimestampMillis(),
                state == null ? null : state.buffer(),
                assumeNonNull(TabAssociatedApp.getAppId(tab)),
                tab.getThemeColor(),
                tab.getTabLaunchTypeAtCreation(),
                tab.getUserAgent(),
                tab.getLastNavigationCommittedTimestampMillis(),
                tab.getTabGroupId(),
                tab.getTabHasSensitiveContent(),
                tab.getIsPinned());
    }

    private void onTabRegistered(Tab tab) {
        TabStateAttributes attributes = TabStateAttributes.from(tab);
        assumeNonNull(attributes);
        if (attributes.addObserver(mAttributesObserver) == DirtinessState.DIRTY) {
            saveTab(tab);
        }
    }

    private void onTabUnregistered(Tab tab) {
        assumeNonNull(TabStateAttributes.from(tab)).removeObserver(mAttributesObserver);
        // TODO(https://crbug.com/430996004): Delete the tab record.
    }
}
