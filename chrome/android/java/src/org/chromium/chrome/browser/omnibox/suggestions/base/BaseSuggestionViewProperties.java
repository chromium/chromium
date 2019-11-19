// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The base set of properties for most omnibox suggestions. */
public class BaseSuggestionViewProperties {
    /** SuggestionDrawableState to show as a suggestion icon. */
    public static final WritableObjectPropertyKey<SuggestionDrawableState> ICON =
            new WritableObjectPropertyKey();

    /** SuggestionDrawableState to show as an action icon. */
    public static final WritableObjectPropertyKey<SuggestionDrawableState> ACTION_ICON =
            new WritableObjectPropertyKey();

    /** Delegate receiving user events. */
    public static final WritableObjectPropertyKey<SuggestionViewDelegate> SUGGESTION_DELEGATE =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_UNIQUE_KEYS =
            new PropertyKey[] {ICON, ACTION_ICON, SUGGESTION_DELEGATE};

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, SuggestionCommonProperties.ALL_KEYS);
}
