// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.device_reauth;

import android.text.SpannableString;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

public class BiometricErrorDialogProperties {
    private BiometricErrorDialogProperties() {}

    static final PropertyModel.ReadableObjectPropertyKey<String> TITLE =
            new ReadableObjectPropertyKey<>("dialog title");
    static final PropertyModel.ReadableObjectPropertyKey<String> DESCRIPTION =
            new ReadableObjectPropertyKey<>("description");
    static final PropertyModel.ReadableObjectPropertyKey<SpannableString> MORE_DETAILS =
            new ReadableObjectPropertyKey("more details");

    static final PropertyKey[] ALL_KEYS = {TITLE, DESCRIPTION, MORE_DETAILS};
}
