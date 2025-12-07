// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

import android.text.SpannableString;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Model properties describing the contents of the CSV download dialog. */
@NullMarked
class PasswordCsvDownloadDialogProperties {
    private PasswordCsvDownloadDialogProperties() {}

    static final PropertyModel.ReadableObjectPropertyKey<String> TITLE =
            new PropertyModel.ReadableObjectPropertyKey<>("dialog title");

    static final PropertyModel.ReadableObjectPropertyKey<SpannableString> DETAILS_PARAGRAPH1 =
            new PropertyModel.ReadableObjectPropertyKey<>("first details paragraph");

    static final PropertyModel.ReadableObjectPropertyKey<String> DETAILS_PARAGRAPH2 =
            new PropertyModel.ReadableObjectPropertyKey<>("second details paragraph");

    static final PropertyModel.ReadableObjectPropertyKey<Runnable> EXPORT_BUTTON_CALLBACK =
            new PropertyModel.ReadableObjectPropertyKey<>("export button callback");
    static final PropertyModel.ReadableObjectPropertyKey<Runnable> CLOSE_BUTTON_CALLBACK =
            new PropertyModel.ReadableObjectPropertyKey<>("close button callback");

    static final PropertyKey[] ALL_KEYS = {
        TITLE, DETAILS_PARAGRAPH1, DETAILS_PARAGRAPH2, EXPORT_BUTTON_CALLBACK, CLOSE_BUTTON_CALLBACK
    };
}
