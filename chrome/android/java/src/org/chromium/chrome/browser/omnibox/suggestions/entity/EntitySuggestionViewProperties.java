// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.entity;

import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * The properties associated with rendering the entity suggestion view.
 */
class EntitySuggestionViewProperties {
    /** Text content for the first line of text (subject). */
    public static final WritableObjectPropertyKey<String> SUBJECT_TEXT =
            new WritableObjectPropertyKey<>();
    /** Text content for the second line of text (description). */
    public static final WritableObjectPropertyKey<String> DESCRIPTION_TEXT =
            new WritableObjectPropertyKey<>();
    /** Decoration type of the presented Entity suggestion */
    public static final WritableIntPropertyKey DECORATION_TYPE = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_UNIQUE_KEYS =
            new PropertyKey[] {SUBJECT_TEXT, DESCRIPTION_TEXT, DECORATION_TYPE};

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, BaseSuggestionViewProperties.ALL_KEYS);
}
