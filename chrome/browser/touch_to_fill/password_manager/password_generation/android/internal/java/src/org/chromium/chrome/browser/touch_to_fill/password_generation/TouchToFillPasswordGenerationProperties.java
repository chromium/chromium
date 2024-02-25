// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.password_generation;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

/**
 *  Properties defined here reflect the visible state of the TouchToFillPasswordGeneration
 * component.
 */
class TouchToFillPasswordGenerationProperties {
    public static final ReadableObjectPropertyKey<String> ACCOUNT_EMAIL =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<String> GENERATED_PASSWORD =
            new ReadableObjectPropertyKey<>();

    public static final ReadableObjectPropertyKey<Callback<String>> PASSWORD_ACCEPTED_CALLBACK =
            new ReadableObjectPropertyKey<>();

    public static final ReadableObjectPropertyKey<Runnable> PASSWORD_REJECTED_CALLBACK =
            new ReadableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ACCOUNT_EMAIL,
                GENERATED_PASSWORD,
                PASSWORD_ACCEPTED_CALLBACK,
                PASSWORD_REJECTED_CALLBACK
            };
}
