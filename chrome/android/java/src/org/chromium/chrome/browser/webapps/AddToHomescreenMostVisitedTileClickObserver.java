// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPage.MostVisitedTileClickObserver;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.TileSource;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

/**
 * This class contains the glue logic necessary for triggering the add to home screen IPH in
 * relation to NTP. It observes the currently shown NTP and forwards the most visited tile click
 * events to the {@link AddToHomescreenIPHController} after the page has been loaded.
 */
public class AddToHomescreenMostVisitedTileClickObserver implements MostVisitedTileClickObserver {
    private CurrentTabObserver mCurrentTabObserver;
    private GURL mLastClickedMostVisitedTileUrl;

    /**
     * Constructor.
     * @param tabSupplier The current tab supplier.
     * @param addToHomescreenIPHController The {@link AddToHomescreenIPHController} responsible for
     *         showing the IPH messages.
     */
    public AddToHomescreenMostVisitedTileClickObserver(
            ObservableSupplier<Tab> tabSupplier,
            AddToHomescreenIPHController addToHomescreenIPHController) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ADD_TO_HOMESCREEN_IPH)) return;

        mCurrentTabObserver =
                new CurrentTabObserver(
                        tabSupplier,
                        new EmptyTabObserver() {
                            @Override
                            public void onPageLoadFinished(Tab tab, GURL url) {
                                if (isNtp(tab)) {
                                    // If we are on NTP, add ourselves as an observer for most
                                    // visited tiles.
                                    NewTabPage ntp = (NewTabPage) tab.getNativePage();
                                    ntp.addMostVisitedTileClickObserver(
                                            AddToHomescreenMostVisitedTileClickObserver.this);
                                } else {
                                    // If it is a regular web page, and started from a most visited
                                    // tile, show IPH.
                                    if (url.getOrigin().equals(mLastClickedMostVisitedTileUrl)) {
                                        addToHomescreenIPHController.showAddToHomescreenIPH(tab);
                                    }
                                    removeObserver(tab);
                                }
                                mLastClickedMostVisitedTileUrl = null;
                            }

                            @Override
                            public void onContentChanged(Tab tab) {
                                removeObserver(tab);
                            }
                        },
                        null);
    }

    @Override
    public void onMostVisitedTileClicked(Tile tile, Tab tab) {
        if (tile.getSource() != TileSource.TOP_SITES) return;
        mLastClickedMostVisitedTileUrl = tab.getUrl().getOrigin();
    }

    private void removeObserver(Tab tab) {
        if (!isNtp(tab)) return;

        NewTabPage ntp = (NewTabPage) tab.getNativePage();
        ntp.removeMostVisitedTileClickObserver(this);
    }

    private boolean isNtp(Tab tab) {
        return tab != null && tab.getNativePage() instanceof NewTabPage;
    }
}
