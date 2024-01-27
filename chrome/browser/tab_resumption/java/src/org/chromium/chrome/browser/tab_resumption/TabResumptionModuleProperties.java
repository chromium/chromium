// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

interface TabResumptionModuleProperties {
    WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();
    WritableObjectPropertyKey DATA_PROVIDER = new WritableObjectPropertyKey();
    WritableObjectPropertyKey URL_IMAGE_PROVIDER = new WritableObjectPropertyKey();
    WritableObjectPropertyKey CLICK_CALLBACK = new WritableObjectPropertyKey();
    WritableObjectPropertyKey SUGGESTION_BUNDLE = new WritableObjectPropertyKey();

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                IS_VISIBLE, DATA_PROVIDER, URL_IMAGE_PROVIDER, CLICK_CALLBACK, SUGGESTION_BUNDLE
            };
}
