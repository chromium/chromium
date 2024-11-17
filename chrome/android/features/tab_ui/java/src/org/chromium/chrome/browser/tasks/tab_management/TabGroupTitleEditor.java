// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class hosts logic related to edit tab group title. Implementations need to specify the title
 * storage/fetching implementation as well as handle {@link PropertyModel} update.
 */
public interface TabGroupTitleEditor {
    /**
     * This method uses {@code title} to update the {@link PropertyModel} of the group which
     * contains {@code tab}. Concrete class need to specify how to update title in {@link
     * PropertyModel}.
     *
     * @param tab The {@link Tab} whose relevant tab group's title will be updated.
     * @param title The tab group title to update.
     */
    void updateTabGroupTitle(Tab tab, String title);

    /**
     * This method uses tab group root ID as a reference to store tab group title.
     *
     * @param tabRootId The tab root ID of the tab group whose title will be stored.
     * @param title The tab group title to store.
     */
    void storeTabGroupTitle(int tabRootId, String title);

    /**
     * This method uses tab group root ID as a reference to delete specific stored tab group title.
     *
     * @param tabRootId The tab root ID whose related tab group title will be deleted.
     */
    void deleteTabGroupTitle(int tabRootId);

    /**
     * This method uses tab group root ID to fetch stored tab group title.
     *
     * @param tabRootId The tab root ID whose related tab group title will be fetched.
     * @return The stored title of the related group.
     */
    String getTabGroupTitle(int tabRootId);
}
