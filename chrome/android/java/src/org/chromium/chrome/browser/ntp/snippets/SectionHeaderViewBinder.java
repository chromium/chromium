// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * View binder for {@link SectionHeaderListProperties}, {@link SectionHeaderProperties} and {@link
 * SectionHeaderView}.
 */
public class SectionHeaderViewBinder
        implements PropertyModelChangeProcessor
                           .ViewBinder<PropertyModel, SectionHeaderView, PropertyKey>,
                   ListModelChangeProcessor
                           .ViewBinder<PropertyListModel<PropertyModel, PropertyKey>,
                                   SectionHeaderView, PropertyKey> {
    @Override
    public void bind(PropertyModel model, SectionHeaderView view, PropertyKey key) {
        if (key == SectionHeaderListProperties.IS_SECTION_ENABLED_KEY) {
            boolean isExpanding = model.get(SectionHeaderListProperties.IS_SECTION_ENABLED_KEY);
            if (isExpanding) {
                view.expandHeader();
                setActiveTab(model, view);
            } else {
                view.collapseHeader();
            }
        } else if (key == SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY) {
            setActiveTab(model, view);
        } else if (key == SectionHeaderListProperties.ON_TAB_SELECTED_CALLBACK_KEY) {
            view.setTabChangeListener(
                    model.get(SectionHeaderListProperties.ON_TAB_SELECTED_CALLBACK_KEY));
        } else if (key == SectionHeaderListProperties.MENU_DELEGATE_KEY
                || key == SectionHeaderListProperties.MENU_MODEL_LIST_KEY) {
            view.setMenuDelegate(model.get(SectionHeaderListProperties.MENU_MODEL_LIST_KEY),
                    model.get(SectionHeaderListProperties.MENU_DELEGATE_KEY));
        }
    }

    private void setActiveTab(PropertyModel model, SectionHeaderView view) {
        int index = model.get(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY);
        // TODO(chili): Figure out whether check needed in scroll state restore.
        if (index <= model.get(SectionHeaderListProperties.SECTION_HEADERS_KEY).size()) {
            view.setActiveTab(model.get(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY));
        }
    }

    @Override
    public void onItemsInserted(PropertyListModel<PropertyModel, PropertyKey> headers,
            SectionHeaderView view, int index, int count) {
        for (int i = index; i < count + index; i++) {
            view.addTab();
        }
        onItemsChanged(headers, view, index, count, null);
    }

    @Override
    public void onItemsRemoved(PropertyListModel<PropertyModel, PropertyKey> model,
            SectionHeaderView view, int index, int count) {
        // Do nothing. We don't expect to remove tabs.
        assert false;
    }

    @Override
    public void onItemsChanged(PropertyListModel<PropertyModel, PropertyKey> headers,
            SectionHeaderView view, int index, int count, PropertyKey payload) {
        if (payload == null || payload == SectionHeaderProperties.HEADER_TEXT_KEY) {
            PropertyModel header = headers.get(0);
            // Only use 1st tab for legacy headerText;
            view.setHeaderText(header.get(SectionHeaderProperties.HEADER_TEXT_KEY));

            // Update header text properly.
            for (int i = index; i < index + count; i++) {
                view.setHeaderTextAt(
                        headers.get(i).get(SectionHeaderProperties.HEADER_TEXT_KEY), i);
            }
        }
    }
}
