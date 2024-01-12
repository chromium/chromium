// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

interface TabResumptionModuleProperties {
    WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();

    PropertyKey[] ALL_KEYS = new PropertyKey[] {IS_VISIBLE};
}
