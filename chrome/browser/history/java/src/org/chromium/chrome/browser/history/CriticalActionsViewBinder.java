// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for the critical actions section of a history row and its action items. */
@NullMarked
class CriticalActionsViewBinder {
    /**
     * Views and setters of the host history row that {@link #bindRow} updates. The setters are
     * callbacks because the row (HistoryItemView) lives in chrome_java, which this target can't
     * depend on.
     */
    static class ViewHolder {
        final ImageView mExpandIcon;
        final ViewGroup mPanelContainer;
        final Callback<@Nullable Runnable> mSetRowClickHandler;
        final Callback<@Nullable CharSequence> mSetRowClickAccessibilityLabel;

        ViewHolder(
                ImageView expandIcon,
                ViewGroup panelContainer,
                Callback<@Nullable Runnable> setRowClickHandler,
                Callback<@Nullable CharSequence> setRowClickAccessibilityLabel) {
            mExpandIcon = expandIcon;
            mPanelContainer = panelContainer;
            mSetRowClickHandler = setRowClickHandler;
            mSetRowClickAccessibilityLabel = setRowClickAccessibilityLabel;
        }
    }

    /** Binds the row-level model to the host row's views. */
    static void bindRow(PropertyModel model, ViewHolder holder, PropertyKey key) {
        if (key == CriticalActionsProperties.EXPAND_ICON_VISIBLE) {
            holder.mExpandIcon.setVisibility(
                    model.get(CriticalActionsProperties.EXPAND_ICON_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (key == CriticalActionsProperties.EXPAND_ICON_CONTENT_DESCRIPTION) {
            holder.mExpandIcon.setContentDescription(
                    model.get(CriticalActionsProperties.EXPAND_ICON_CONTENT_DESCRIPTION));
        } else if (key == CriticalActionsProperties.EXPAND_ICON_ROTATION) {
            holder.mExpandIcon.setRotation(
                    model.get(CriticalActionsProperties.EXPAND_ICON_ROTATION));
        } else if (key == CriticalActionsProperties.PANEL_VISIBLE) {
            holder.mPanelContainer.setVisibility(
                    model.get(CriticalActionsProperties.PANEL_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (key == CriticalActionsProperties.ROW_CLICK_HANDLER) {
            holder.mSetRowClickHandler.onResult(
                    model.get(CriticalActionsProperties.ROW_CLICK_HANDLER));
        } else if (key == CriticalActionsProperties.ROW_CLICK_ACCESSIBILITY_LABEL) {
            holder.mSetRowClickAccessibilityLabel.onResult(
                    model.get(CriticalActionsProperties.ROW_CLICK_ACCESSIBILITY_LABEL));
        }
    }

    /** Binds an action's model to its row in the panel's list. */
    static void bindActionItem(PropertyModel model, View view, PropertyKey key) {
        if (key == CriticalActionsProperties.LABEL) {
            ((TextView) view).setText(model.get(CriticalActionsProperties.LABEL));
        }
    }
}
