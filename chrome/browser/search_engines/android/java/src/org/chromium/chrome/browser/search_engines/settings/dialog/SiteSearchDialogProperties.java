// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.dialog;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

@NullMarked
public class SiteSearchDialogProperties {
    public static final WritableObjectPropertyKey<String> INVALID_KEYWORD_ERROR_MESSAGE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> INVALID_NAME_ERROR_MESSAGE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> INVALID_URL_ERROR_MESSAGE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> KEYWORD =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> NAME = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Callback<String>> ON_KEYWORD_CHANGED =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Callback<String>> ON_NAME_CHANGED =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Callback<String>> ON_URL_CHANGED =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> URL = new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey URL_ENABLED = new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        INVALID_KEYWORD_ERROR_MESSAGE,
        INVALID_NAME_ERROR_MESSAGE,
        INVALID_URL_ERROR_MESSAGE,
        KEYWORD,
        NAME,
        ON_KEYWORD_CHANGED,
        ON_NAME_CHANGED,
        ON_URL_CHANGED,
        URL,
        URL_ENABLED,
    };
}
