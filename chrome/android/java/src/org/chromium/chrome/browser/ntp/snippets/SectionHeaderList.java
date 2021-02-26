// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Represents a list of SectionHeaders.
 */
public class SectionHeaderList extends PropertyModel {
    public static final PropertyModel.WritableBooleanPropertyKey IS_SECTION_ENABLED_KEY =
            new PropertyModel.WritableBooleanPropertyKey();

    // Private writable version because cannot set initial values on the super model with Readable
    // keys.
    private static final PropertyModel
            .WritableObjectPropertyKey<PropertyListModel<SectionHeader, PropertyKey>>
                    SECTION_HEADERS_KEY = new PropertyModel.WritableObjectPropertyKey<>();

    public SectionHeaderList() {
        super(IS_SECTION_ENABLED_KEY, SECTION_HEADERS_KEY);
        PropertyListModel<SectionHeader, PropertyKey> headerList = new PropertyListModel<>();
        set(SECTION_HEADERS_KEY, headerList);
    }

    public void addHeader(SectionHeader header) {
        get(SECTION_HEADERS_KEY).add(header);
    }

    public PropertyListModel<SectionHeader, PropertyKey> getHeaders() {
        return get(SECTION_HEADERS_KEY);
    }

    /**
     * @return Whether or not the contents below this header is shown or not.
     */
    public boolean isSectionEnabled() {
        return get(IS_SECTION_ENABLED_KEY);
    }

    /**
     * Toggle the expanded state of the header.
     * Toggling to off state should collapse the entire section.
     */
    public void toggleSection() {
        set(IS_SECTION_ENABLED_KEY, !get(IS_SECTION_ENABLED_KEY));
    }
}
