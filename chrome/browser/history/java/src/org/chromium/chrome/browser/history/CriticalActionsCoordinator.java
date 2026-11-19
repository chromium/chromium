// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.animation.ValueAnimator;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.history.CriticalActionsProperties.ItemType;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.ViewGroupAdapter;

import java.util.List;

/**
 * Coordinator managing the inline "Gemini actions" expandable panel and chevron for a history item
 * row. Lazily inflates the panel and {@link ViewGroupAdapter} only when an item with critical
 * actions is bound while the feature is enabled.
 */
@NullMarked
public class CriticalActionsCoordinator implements Destroyable {
    private final Context mContext;
    private final ViewGroup mPanelContainer;
    private final PropertyModel mModel;
    private final ModelList mModelList;
    private final CriticalActionsMediator mMediator;
    private final CriticalActionsExpansionAnimator mExpansionAnimator;
    private final PropertyModelChangeProcessor<
                    PropertyModel, CriticalActionsViewBinder.ViewHolder, PropertyKey>
            mPropertyModelChangeProcessor;

    private @Nullable ViewGroupAdapter mViewGroupAdapter;

    /**
     * @param context Android {@link Context} for inflating views and resolving strings.
     * @param expandIcon Expand/collapse chevron button in the row. Clicking it toggles the panel.
     * @param panelContainer Empty container below the row that hosts the critical actions panel.
     */
    public CriticalActionsCoordinator(
            Context context, ImageView expandIcon, ViewGroup panelContainer) {
        mContext = context;
        mPanelContainer = panelContainer;
        mModel = new PropertyModel(CriticalActionsProperties.ALL_KEYS);
        mModelList = new ModelList();
        mExpansionAnimator = new CriticalActionsExpansionAnimator(expandIcon, panelContainer);
        CriticalActionsViewBinder.ViewHolder viewHolder =
                new CriticalActionsViewBinder.ViewHolder(expandIcon, mExpansionAnimator);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, viewHolder, CriticalActionsViewBinder::bindRow);
        mMediator = new CriticalActionsMediator(context, mModel, mModelList);
    }

    /**
     * Shows the critical actions of the history item now displayed in the row.
     *
     * @param actionLabels Localized labels of the item's critical actions, or empty if none.
     * @param expanded Whether the panel is currently expanded for this item.
     * @param selectionActive Whether list selection mode is currently active.
     * @param onExpansionChanged Callback invoked when the user expands or collapses this item.
     */
    public void attachToHistoryItem(
            List<String> actionLabels,
            boolean expanded,
            boolean selectionActive,
            @Nullable Callback<Boolean> onExpansionChanged) {
        if (CriticalActionsMediator.shouldShowCriticalActions(actionLabels)) {
            ensurePanelInflated();
        }
        // Finish expanding or collapsing the previously attached item, as the row is being reused.
        mExpansionAnimator.endAnimation();
        mMediator.attachToHistoryItem(actionLabels, expanded, selectionActive, onExpansionChanged);
    }

    /** Updates whether list selection mode is active. */
    public void setSelectionActive(boolean selectionActive) {
        mMediator.setSelectionActive(selectionActive);
    }

    @Override
    public void destroy() {
        mExpansionAnimator.destroy();
        // Clear the chevron's click handler and label while the binder is still attached.
        mMediator.destroy();
        mPropertyModelChangeProcessor.destroy();
        if (mViewGroupAdapter != null) {
            mViewGroupAdapter.destroy();
            mViewGroupAdapter = null;
        }
    }

    private void ensurePanelInflated() {
        if (mViewGroupAdapter != null) return;

        View panel =
                LayoutInflater.from(mContext)
                        .inflate(
                                R.layout.history_item_critical_actions,
                                mPanelContainer,
                                /* attachToRoot= */ false);
        mPanelContainer.addView(panel);

        ViewGroup listView = panel.findViewById(R.id.critical_actions_list);
        mViewGroupAdapter =
                new ViewGroupAdapter.Builder(listView, mModelList)
                        .registerType(
                                ItemType.ACTION,
                                parent ->
                                        LayoutInflater.from(parent.getContext())
                                                .inflate(
                                                        R.layout.history_item_critical_action,
                                                        parent,
                                                        /* attachToRoot= */ false),
                                CriticalActionsViewBinder::bindActionItem)
                        .build();
    }

    /** Returns the running expand/collapse animation, if any. */
    public @Nullable ValueAnimator getExpansionAnimatorForTesting() {
        return mExpansionAnimator.getAnimatorForTesting();
    }
}
