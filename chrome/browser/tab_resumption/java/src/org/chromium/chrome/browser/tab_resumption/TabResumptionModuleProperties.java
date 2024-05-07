// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallbacks;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

interface TabResumptionModuleProperties {
    WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();
    WritableBooleanPropertyKey USE_SALIENT_IMAGE = new WritableBooleanPropertyKey();

    WritableObjectPropertyKey<UrlImageProvider> URL_IMAGE_PROVIDER =
            new WritableObjectPropertyKey();
    WritableObjectPropertyKey<ThumbnailProvider> THUMBNAIL_PROVIDER =
            new WritableObjectPropertyKey();
    WritableObjectPropertyKey<Runnable> SEE_MORE_LINK_CLICK_CALLBACK =
            new WritableObjectPropertyKey();
    WritableObjectPropertyKey<SuggestionClickCallbacks> CLICK_CALLBACK =
            new WritableObjectPropertyKey();
    WritableObjectPropertyKey<SuggestionBundle> SUGGESTION_BUNDLE = new WritableObjectPropertyKey();
    WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey();

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                IS_VISIBLE,
                USE_SALIENT_IMAGE,
                URL_IMAGE_PROVIDER,
                THUMBNAIL_PROVIDER,
                SEE_MORE_LINK_CLICK_CALLBACK,
                CLICK_CALLBACK,
                SUGGESTION_BUNDLE,
                TITLE,
            };
}
