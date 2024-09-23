// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import android.content.Context;

import org.chromium.chrome.browser.quick_delete.QuickDeleteDelegate.DomainVisitsData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The class responsible for specifying the various quick delete MVC properties. */
class QuickDeleteProperties {
    public static final PropertyModel.WritableIntPropertyKey TIME_PERIOD =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<DomainVisitsData>
            DOMAIN_VISITED_DATA = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableIntPropertyKey CLOSED_TABS_COUNT =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_SIGNED_IN =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_SYNCING_HISTORY =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_DOMAIN_VISITED_DATA_PENDING =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.ReadableObjectPropertyKey<Context> CONTEXT =
            new PropertyModel.ReadableObjectPropertyKey();
    public static final PropertyModel.ReadableBooleanPropertyKey HAS_MULTI_WINDOWS =
            new PropertyModel.ReadableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        TIME_PERIOD,
        DOMAIN_VISITED_DATA,
        CLOSED_TABS_COUNT,
        IS_SIGNED_IN,
        IS_SYNCING_HISTORY,
        IS_DOMAIN_VISITED_DATA_PENDING,
        CONTEXT,
        HAS_MULTI_WINDOWS
    };
}
