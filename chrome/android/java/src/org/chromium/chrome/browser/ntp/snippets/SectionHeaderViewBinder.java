// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * View binder for {@link SectionHeaderList}, {@link SectionHeader} and {@link SectionHeaderView}.
 */
public class SectionHeaderViewBinder
        implements PropertyModelChangeProcessor
                           .ViewBinder<SectionHeaderList, SectionHeaderView, PropertyKey> {
    @Override
    public void bind(SectionHeaderList model, SectionHeaderView view, PropertyKey key) {
        if (key == SectionHeaderList.IS_SECTION_ENABLED_KEY) {
            boolean isExpanding = model.get(SectionHeaderList.IS_SECTION_ENABLED_KEY);
            if (isExpanding) {
                view.expandHeader();
            } else {
                view.collapseHeader();
            }
        }
    }

    void onItemsInserted(SectionHeaderList model, SectionHeaderView view, int index, int count) {
        PropertyListModel<SectionHeader, PropertyKey> headers = model.getHeaders();

        // TODO(chili): Make this support more than 1 header.
        SectionHeader header = headers.get(0);
        view.setHeaderText(header.get(SectionHeader.HEADER_TEXT_KEY));
        view.setMenuDelegate(header.get(SectionHeader.MENU_MODEL_LIST_KEY),
                header.get(SectionHeader.MENU_DELEGATE_KEY));
    }

    void onItemsRemoved(SectionHeaderList model, SectionHeaderView view, int index, int count) {
        // TODO(chili): Implement.
    }

    void onItemsChanged(SectionHeaderList model, SectionHeaderView view, int index, int count,
            PropertyKey payload) {
        PropertyListModel<SectionHeader, PropertyKey> headers = model.getHeaders();

        // TODO(chili): Make this support more than 1 header.
        SectionHeader header = headers.get(0);
        if (payload == SectionHeader.HEADER_TEXT_KEY) {
            view.setHeaderText(header.get(SectionHeader.HEADER_TEXT_KEY));
        } else if (payload == SectionHeader.MENU_MODEL_LIST_KEY
                || payload == SectionHeader.MENU_DELEGATE_KEY) {
            view.setMenuDelegate(header.get(SectionHeader.MENU_MODEL_LIST_KEY),
                    header.get(SectionHeader.MENU_DELEGATE_KEY));
        }
    }
}
