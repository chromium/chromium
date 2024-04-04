// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallbacks;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

interface TabResumptionModuleProperties {
    WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();
    WritableObjectPropertyKey<UrlImageProvider> URL_IMAGE_PROVIDER =
            new WritableObjectPropertyKey();
    WritableObjectPropertyKey<TabListFaviconProvider> FAVICON_PROVIDER =
            new WritableObjectPropertyKey();
    WritableObjectPropertyKey<ThumbnailProvider> THUMBNAIL_PROVIDER =
            new WritableObjectPropertyKey();
    WritableObjectPropertyKey<SuggestionClickCallbacks> CLICK_CALLBACK =
            new WritableObjectPropertyKey();
    WritableObjectPropertyKey<SuggestionBundle> SUGGESTION_BUNDLE = new WritableObjectPropertyKey();
    WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey();

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                IS_VISIBLE,
                URL_IMAGE_PROVIDER,
                FAVICON_PROVIDER,
                THUMBNAIL_PROVIDER,
                CLICK_CALLBACK,
                SUGGESTION_BUNDLE,
                TITLE,
            };
}
