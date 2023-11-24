// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.sections;

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
        implements PropertyModelChangeProcessor.ViewBinder<
                        PropertyModel, SectionHeaderView, PropertyKey>,
                ListModelChangeProcessor.ViewBinder<
                        PropertyListModel<PropertyModel, PropertyKey>,
                        SectionHeaderView,
                        PropertyKey> {
    @Override
    public void bind(PropertyModel model, SectionHeaderView view, PropertyKey key) {
        if (key == SectionHeaderListProperties.IS_SECTION_ENABLED_KEY) {
            boolean isEnabled = model.get(SectionHeaderListProperties.IS_SECTION_ENABLED_KEY);
            view.setTextsEnabled(isEnabled);
            if (isEnabled) {
                setActiveTab(model, view);
            }
        } else if (key == SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY) {
            setActiveTab(model, view);
        } else if (key == SectionHeaderListProperties.ON_TAB_SELECTED_CALLBACK_KEY) {
            view.setTabChangeListener(
                    model.get(SectionHeaderListProperties.ON_TAB_SELECTED_CALLBACK_KEY));
        } else if (key == SectionHeaderListProperties.MENU_DELEGATE_KEY
                || key == SectionHeaderListProperties.MENU_MODEL_LIST_KEY) {
            view.setMenuDelegate(
                    model.get(SectionHeaderListProperties.MENU_MODEL_LIST_KEY),
                    model.get(SectionHeaderListProperties.MENU_DELEGATE_KEY));
        } else if (key == SectionHeaderListProperties.IS_TAB_MODE_KEY) {
            view.setTabMode(model.get(SectionHeaderListProperties.IS_TAB_MODE_KEY));
        } else if (key == SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY) {
            view.setIndicatorVisibility(
                    model.get(SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY));
        } else if (key == SectionHeaderListProperties.IS_LOGO_KEY) {
            view.setIsLogo(model.get(SectionHeaderListProperties.IS_LOGO_KEY));
        } else if (key == SectionHeaderListProperties.EXPANDING_DRAWER_VIEW_KEY) {
            view.setOptionsPanel(model.get(SectionHeaderListProperties.EXPANDING_DRAWER_VIEW_KEY));
        } else if (key == SectionHeaderListProperties.STICKY_HEADER_EXPANDING_DRAWER_VIEW_KEY) {
            view.setStickyHeaderOptionsPanel(
                    model.get(SectionHeaderListProperties.STICKY_HEADER_EXPANDING_DRAWER_VIEW_KEY));
        } else if (key == SectionHeaderListProperties.TOOLBAR_HEIGHT_PX) {
            view.setToolbarHeight(model.get(SectionHeaderListProperties.TOOLBAR_HEIGHT_PX));
        } else if (key == SectionHeaderListProperties.STICKY_HEADER_VISIBLILITY_KEY) {
            view.setStickyHeaderVisible(
                    model.get(SectionHeaderListProperties.STICKY_HEADER_VISIBLILITY_KEY));
        } else if (key == SectionHeaderListProperties.STICKY_HEADER_MUTABLE_MARGIN_KEY) {
            view.updateStickyHeaderMargin(
                    model.get(SectionHeaderListProperties.STICKY_HEADER_MUTABLE_MARGIN_KEY));
        } else if (key == SectionHeaderListProperties.IS_NARROW_WINDOW_ON_TABLET_KEY) {
            view.updateTabLayoutHeaderWidth(
                    model.get(SectionHeaderListProperties.IS_NARROW_WINDOW_ON_TABLET_KEY));
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
    public void onItemsInserted(
            PropertyListModel<PropertyModel, PropertyKey> headers,
            SectionHeaderView view,
            int index,
            int count) {
        for (int i = index; i < count + index; i++) {
            view.addTab();
        }
        onItemsChanged(headers, view, index, count, null);
    }

    @Override
    public void onItemsRemoved(
            PropertyListModel<PropertyModel, PropertyKey> model,
            SectionHeaderView view,
            int index,
            int count) {
        if (model.size() == 0) {
            // All headers were removed.
            view.removeAllTabs();
            return;
        }
        for (int i = index + count - 1; i >= index; i--) {
            view.removeTabAt(i);
        }
    }

    @Override
    public void onItemsChanged(
            PropertyListModel<PropertyModel, PropertyKey> headers,
            SectionHeaderView view,
            int index,
            int count,
            PropertyKey payload) {
        PropertyModel header = headers.get(0);
        if (payload == null
                || payload == SectionHeaderProperties.HEADER_TEXT_KEY
                || payload == SectionHeaderProperties.UNREAD_CONTENT_KEY
                || payload == SectionHeaderProperties.BADGE_TEXT_KEY) {
            // Only use 1st tab for legacy headerText;
            view.setHeaderText(header.get(SectionHeaderProperties.HEADER_TEXT_KEY));

            // Update headers.
            for (int i = index; i < index + count; i++) {
                PropertyModel tabModel = headers.get(i);
                boolean hasUnreadContent = tabModel.get(SectionHeaderProperties.UNREAD_CONTENT_KEY);

                view.setHeaderAt(
                        tabModel.get(SectionHeaderProperties.HEADER_TEXT_KEY),
                        hasUnreadContent,
                        tabModel.get(SectionHeaderProperties.BADGE_TEXT_KEY),
                        tabModel.get(SectionHeaderProperties.ANIMATION_START_KEY),
                        i);
            }
        }
        if (payload == null || payload == SectionHeaderProperties.ANIMATION_START_KEY) {
            for (int i = index; i < index + count; i++) {
                boolean animationStart =
                        headers.get(i).get(SectionHeaderProperties.ANIMATION_START_KEY);
                if (animationStart) {
                    view.startAnimationForHeader(i);
                }
            }
        }
        if (payload == null
                || payload == SectionHeaderProperties.OPTIONS_INDICATOR_VISIBILITY_KEY) {
            for (int i = index; i < index + count; i++) {
                PropertyModel tabModel = headers.get(i);
                view.setOptionsIndicatorVisibilityForHeader(
                        i, tabModel.get(SectionHeaderProperties.OPTIONS_INDICATOR_VISIBILITY_KEY));
            }
        }
        if (payload == null || payload == SectionHeaderProperties.OPTIONS_INDICATOR_IS_OPEN_KEY) {
            for (int i = index; i < index + count; i++) {
                PropertyModel tabModel = headers.get(i);
                view.updateDrawable(
                        i, tabModel.get(SectionHeaderProperties.OPTIONS_INDICATOR_IS_OPEN_KEY));
            }
        }
    }
}
