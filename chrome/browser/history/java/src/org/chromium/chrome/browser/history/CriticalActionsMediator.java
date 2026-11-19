// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.history.CriticalActionsProperties.Expansion;
import org.chromium.chrome.browser.history.CriticalActionsProperties.ItemType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Mediator driving the critical actions state, chevron, and action list for a history row. */
@NullMarked
class CriticalActionsMediator {
    private final Context mContext;
    private final PropertyModel mModel;
    private final ModelList mModelList;
    private final Runnable mToggleExpandedHandler = this::toggleExpanded;

    private boolean mHasCriticalActions;
    private boolean mExpanded;
    private boolean mSelectionActive;
    private @Nullable Callback<Boolean> mOnExpansionChanged;

    CriticalActionsMediator(Context context, PropertyModel model, ModelList modelList) {
        mContext = context;
        mModel = model;
        mModelList = modelList;
    }

    /** Returns whether critical actions should be shown for the given action labels. */
    static boolean shouldShowCriticalActions(List<String> actionLabels) {
        return !actionLabels.isEmpty() && ChromeFeatureList.sCriticalActionHistory.isEnabled();
    }

    /** Shows the critical actions of the history item now displayed in the row. */
    void attachToHistoryItem(
            List<String> actionLabels,
            boolean expanded,
            boolean selectionActive,
            @Nullable Callback<Boolean> onExpansionChanged) {
        boolean show = shouldShowCriticalActions(actionLabels);
        mHasCriticalActions = show;
        mExpanded = show && expanded;
        mSelectionActive = selectionActive;
        mOnExpansionChanged = show ? onExpansionChanged : null;

        mModelList.clear();
        if (show) {
            for (String label : actionLabels) {
                PropertyModel itemModel =
                        new PropertyModel.Builder(CriticalActionsProperties.ACTION_ITEM_KEYS)
                                .with(CriticalActionsProperties.LABEL, label)
                                .build();
                mModelList.add(new ListItem(ItemType.ACTION, itemModel));
            }
        }

        int count = actionLabels.size();
        mModel.set(
                CriticalActionsProperties.EXPAND_ICON_CONTENT_DESCRIPTION,
                show
                        ? mContext.getResources()
                                .getQuantityString(
                                        R.plurals.history_critical_actions_summary, count, count)
                        : null);
        // Tapping the row always opens the page. Only the chevron toggles the panel.
        mModel.set(
                CriticalActionsProperties.EXPAND_ICON_CLICK_HANDLER,
                show ? mToggleExpandedHandler : null);
        // Attaching the row to an item, e.g. when it's recycled, shows its state right away.
        updateExpansion(/* animate= */ false);
        updateExpandIconVisibility();
        updateClickAccessibilityLabel();
    }

    void setSelectionActive(boolean selectionActive) {
        if (mSelectionActive == selectionActive) return;
        mSelectionActive = selectionActive;
        if (!mHasCriticalActions) return;
        updateExpandIconVisibility();
        updateClickAccessibilityLabel();
    }

    /** Stops the chevron from toggling the critical actions, e.g. when it's no longer used. */
    void destroy() {
        mOnExpansionChanged = null;
        mModel.set(CriticalActionsProperties.EXPAND_ICON_CLICK_HANDLER, null);
        mModel.set(CriticalActionsProperties.EXPAND_ICON_CLICK_ACCESSIBILITY_LABEL, null);
    }

    private void toggleExpanded() {
        // Only installed as the chevron's click handler for items with critical actions.
        assert mHasCriticalActions;
        mExpanded = !mExpanded;
        if (mOnExpansionChanged != null) {
            mOnExpansionChanged.onResult(mExpanded);
        }
        updateExpansion(/* animate= */ true);
        updateClickAccessibilityLabel();
    }

    /** Expands or collapses the panel, optionally with an animation. */
    private void updateExpansion(boolean animate) {
        mModel.set(CriticalActionsProperties.EXPANSION, Expansion.of(mExpanded, animate));
    }

    /** Updates the chevron's visibility. It's hidden in selection mode, where taps select rows. */
    private void updateExpandIconVisibility() {
        mModel.set(CriticalActionsProperties.EXPAND_ICON_VISIBLE, isExpandIconVisible());
    }

    /** Updates the chevron's "Expand section" or "Collapse section" accessibility click label. */
    private void updateClickAccessibilityLabel() {
        mModel.set(
                CriticalActionsProperties.EXPAND_ICON_CLICK_ACCESSIBILITY_LABEL,
                isExpandIconVisible()
                        ? mContext.getString(
                                mExpanded
                                        ? R.string.accessibility_collapse_section
                                        : R.string.accessibility_expand_section)
                        : null);
    }

    private boolean isExpandIconVisible() {
        return mHasCriticalActions && !mSelectionActive;
    }
}
