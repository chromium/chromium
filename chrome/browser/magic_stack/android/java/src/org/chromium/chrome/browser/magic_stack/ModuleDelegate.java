// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The interface for magic stack which owns a list of modules. */
@NullMarked
public interface ModuleDelegate {
    /**
     * Module types that are shown in the magic stack on the home surfaces.
     *
     * <p>These values are persisted to logs. Entries should not be renumbered and numeric values
     * should never be reused. See tools/metrics/histograms/enums.xml.
     */
    @IntDef({
        ModuleType.SINGLE_TAB,
        ModuleType.PRICE_CHANGE,
        ModuleType.DEPRECATED_TAB_RESUMPTION,
        ModuleType.SAFETY_HUB,
        ModuleType.DEPRECATED_EDUCATIONAL_TIP,
        ModuleType.AUXILIARY_SEARCH,
        ModuleType.DEFAULT_BROWSER_PROMO,
        ModuleType.TAB_GROUP_PROMO,
        ModuleType.TAB_GROUP_SYNC_PROMO,
        ModuleType.QUICK_DELETE_PROMO,
        ModuleType.HISTORY_SYNC_PROMO,
        ModuleType.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ModuleType {
        int SINGLE_TAB = 0;
        int PRICE_CHANGE = 1;
        int DEPRECATED_TAB_RESUMPTION = 2;
        int SAFETY_HUB = 3;
        int DEPRECATED_EDUCATIONAL_TIP = 4;
        int AUXILIARY_SEARCH = 5;
        int DEFAULT_BROWSER_PROMO = 6;
        int TAB_GROUP_PROMO = 7;
        int TAB_GROUP_SYNC_PROMO = 8;
        int QUICK_DELETE_PROMO = 9;
        int HISTORY_SYNC_PROMO = 10;
        int NUM_ENTRIES = 11;
    }

    /**
     * Called when a module has a PropertyModel ready. This could be called multiple times from the
     * same module.
     */
    void onDataReady(@ModuleType int moduleType, PropertyModel propertyModel);

    /** Called when a module has no data to show. */
    void onDataFetchFailed(@ModuleType int moduleType);

    /** Removes a module from the magic stack. */
    void removeModule(@ModuleType int moduleType);

    /** Removes a module from the magic stack and disable it from the settings. */
    void removeModuleAndDisable(@ModuleType int moduleType);

    /** Called when the user wants to open the settings to customize modules. */
    void customizeSettings();

    /**
     * Called when the user clicks a module to open a URL.
     *
     * @param gurl The URL to open.
     * @param moduleType The type of the module clicked.
     */
    void onUrlClicked(GURL gurl, @ModuleType int moduleType);

    /**
     * Called when the user clicks a module to select a Tab.
     *
     * @param tabId The id of the Tab to select.
     * @param moduleType The type of the module clicked.
     */
    void onTabClicked(int tabId, @ModuleType int moduleType);

    /**
     * Called when the user clicks a module.
     *
     * @param moduleType The type of the module clicked.
     */
    void onModuleClicked(@ModuleType int moduleType);

    /** Gets the instance of the module {@link ModuleProvider} of the given type. */
    ModuleProvider getModuleProvider(@ModuleType int moduleType);

    /** Gets the local Tab that is showing on the magic stack. */
    @Nullable Tab getTrackingTab();

    /** Called before build and show modules. */
    void prepareBuildAndShow();
}
