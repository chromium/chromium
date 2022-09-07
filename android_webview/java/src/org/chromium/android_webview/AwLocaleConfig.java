// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

/**
 * Simple class that provides access to the array of uncompressed pak locales. See
 * //android_webview/BUILD.gn for more details.
 */
public final class AwLocaleConfig {
    private AwLocaleConfig() {}

    public static String[] getWebViewSupportedPakLocales() {
        return ProductConfig.LOCALES;
    }
}
