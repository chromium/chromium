// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

interface TabResumptionModuleProperties {
    WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();
    WritableObjectPropertyKey<UrlImageProvider> URL_IMAGE_PROVIDER =
            new WritableObjectPropertyKey();
    WritableObjectPropertyKey<SuggestionClickCallback> CLICK_CALLBACK =
            new WritableObjectPropertyKey();
    WritableObjectPropertyKey<SuggestionBundle> SUGGESTION_BUNDLE = new WritableObjectPropertyKey();
    WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey();

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                IS_VISIBLE, URL_IMAGE_PROVIDER, CLICK_CALLBACK, SUGGESTION_BUNDLE, TITLE,
            };
}
