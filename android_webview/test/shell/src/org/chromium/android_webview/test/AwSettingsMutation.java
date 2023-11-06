// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import org.chromium.android_webview.AwSettings;

import java.util.function.Consumer;

/**
 * A wrapper class to hold an AwSettings mutation for testing together
 * with a name to be used as variant. Name should either be the string
 * "null" for the base variant, or else be a double-dot-separated key value pair
 */
public class AwSettingsMutation {
    private final Consumer<AwSettings> mMaybeMutateAwSettings;
    private final String mName;

    public AwSettingsMutation(Consumer<AwSettings> maybeMutateAwSettings, String name) {
        this.mMaybeMutateAwSettings = maybeMutateAwSettings;
        this.mName = name;
    }

    public static AwSettingsMutation doNotMutateAwSettings() {
        return new AwSettingsMutation(null, "null");
    }

    public Consumer<AwSettings> getMutation() {
        return mMaybeMutateAwSettings;
    }

    @Override
    public String toString() {
        return mName;
    }
}
