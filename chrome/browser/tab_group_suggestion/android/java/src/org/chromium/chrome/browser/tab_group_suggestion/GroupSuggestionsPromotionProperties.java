// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Model properties for a GroupSuggestions promotion UI. */
public class GroupSuggestionsPromotionProperties {
    public static final WritableObjectPropertyKey<String> PROMO_HEADER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> PROMO_CONTENTS =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> SUGGESTED_NAME =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> GROUP_CONTENT_STRING =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> ACCEPT_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> REJECT_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                PROMO_HEADER,
                PROMO_CONTENTS,
                SUGGESTED_NAME,
                GROUP_CONTENT_STRING,
                ACCEPT_BUTTON_TEXT,
                REJECT_BUTTON_TEXT
            };
}
