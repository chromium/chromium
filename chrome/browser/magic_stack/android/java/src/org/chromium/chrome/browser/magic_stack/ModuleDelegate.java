// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The interface for magic stack which owns a list of modules. */
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
        ModuleType.TAB_RESUMPTION,
        ModuleType.SAFETY_HUB,
        ModuleType.EDUCATIONAL_TIP,
        ModuleType.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ModuleType {
        int SINGLE_TAB = 0;
        int PRICE_CHANGE = 1;
        int TAB_RESUMPTION = 2;
        int SAFETY_HUB = 3;
        int EDUCATIONAL_TIP = 4;
        int NUM_ENTRIES = 5;
    }

    /**
     * Called when a module has a PropertyModel ready. This could be called multiple times from the
     * same module.
     */
    void onDataReady(@ModuleType int moduleType, @NonNull PropertyModel propertyModel);

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
    void onUrlClicked(@NonNull GURL gurl, @ModuleType int moduleType);

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
     * @param modulePosition The position of the module clicked.
     */
    void onModuleClicked(@ModuleType int moduleType, int modulePosition);

    /** Gets the instance of the module {@link ModuleProvider} of the given type. */
    ModuleProvider getModuleProvider(@ModuleType int moduleType);

    /** Gets the local Tab that is showing on the magic stack. */
    @Nullable
    Tab getTrackingTab();

    /** Called before build and show modules. */
    void prepareBuildAndShow();
}
