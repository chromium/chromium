// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallback;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

interface TabResumptionModuleProperties {
    WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();

    WritableObjectPropertyKey<UrlImageProvider> URL_IMAGE_PROVIDER =
            new WritableObjectPropertyKey();
    WritableObjectPropertyKey<Runnable> SEE_MORE_LINK_CLICK_CALLBACK =
            new WritableObjectPropertyKey();
    WritableObjectPropertyKey<SuggestionClickCallback> CLICK_CALLBACK =
            new WritableObjectPropertyKey();
    WritableObjectPropertyKey<SuggestionBundle> SUGGESTION_BUNDLE = new WritableObjectPropertyKey();
    WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey();

    WritableObjectPropertyKey<ObservableSupplier<TabModelSelector>> TAB_MODEL_SELECTOR_SUPPLIER =
            new WritableObjectPropertyKey<>();

    WritableObjectPropertyKey<Tab> TRACKING_TAB = new WritableObjectPropertyKey<>();

    WritableObjectPropertyKey<Callback<Tab>> TAB_OBSERVER_CALLBACK =
            new WritableObjectPropertyKey();

    WritableObjectPropertyKey<Callback<Integer>> ON_MODULE_SHOW_CONFIG_FINALIZED_CALLBACK =
            new WritableObjectPropertyKey<>();

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                IS_VISIBLE,
                URL_IMAGE_PROVIDER,
                SEE_MORE_LINK_CLICK_CALLBACK,
                CLICK_CALLBACK,
                SUGGESTION_BUNDLE,
                TITLE,
                TAB_MODEL_SELECTOR_SUPPLIER,
                TRACKING_TAB,
                TAB_OBSERVER_CALLBACK,
                ON_MODULE_SHOW_CONFIG_FINALIZED_CALLBACK,
            };
}
