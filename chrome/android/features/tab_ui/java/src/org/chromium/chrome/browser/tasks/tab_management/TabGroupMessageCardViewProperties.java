// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import com.google.common.collect.ObjectArrays;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties for displaying a message card related to tab groups. */
@NullMarked
class TabGroupMessageCardViewProperties {

    /** ID used to uniquely identify message retrieved from the backend service. */
    public static final PropertyModel.ReadableObjectPropertyKey<String>
            MESSAGING_BACKEND_SERVICE_ID = new PropertyModel.ReadableObjectPropertyKey<>();

    private static final PropertyKey[] TAB_GROUP_MESSAGE_CARD_SPECIFIC_KEYS =
            new PropertyKey[] {MESSAGING_BACKEND_SERVICE_ID};

    public static final PropertyKey[] ALL_KEYS =
            ObjectArrays.concat(
                    MessageCardViewProperties.ALL_KEYS,
                    TAB_GROUP_MESSAGE_CARD_SPECIFIC_KEYS,
                    PropertyKey.class);
}
