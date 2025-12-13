// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** TabListEditor action for pinning and unpinning tabs. */
@NullMarked
public class TabListEditorPinAction extends TabListEditorAction {
    /**
     * The state of the pin action. The action can be to pin, unpin, or unsupported if the selection
     * contains a mix of pinned and unpinned tabs.
     */
    @IntDef({State.UNSUPPORTED, State.PIN, State.UNPIN})
    @Retention(RetentionPolicy.SOURCE)
    private @interface State {
        /** The selection contains a mix of pinned and unpinned tabs, or contains tab groups. */
        int UNSUPPORTED = 2;

        /** The selection contains only unpinned tabs. */
        int PIN = 0;

        /** The selection contains only pinned tabs. */
        int UNPIN = 1;
    }

    private @State int mState;

    /**
     * Create an action for pinning/unpinning tabs.
     *
     * @param context for loading resources.
     * @param showMode the ShowMode for the action.
     * @param buttonType the ButtonType for the action.
     * @param iconPosition the IconPosition for the action.
     */
    public static TabListEditorAction createAction(
            Context context,
            @ShowMode int showMode,
            @ButtonType int buttonType,
            @IconPosition int iconPosition) {
        Drawable drawable = AppCompatResources.getDrawable(context, R.drawable.ic_keep_24dp);
        return new TabListEditorPinAction(showMode, buttonType, iconPosition, drawable);
    }

    private TabListEditorPinAction(
            @ShowMode int showMode,
            @ButtonType int buttonType,
            @IconPosition int iconPosition,
            Drawable drawable) {
        super(
                R.id.tab_list_editor_pin_menu_item,
                showMode,
                buttonType,
                iconPosition,
                R.plurals.tab_selection_editor_pin_tabs,
                R.plurals.accessibility_tab_selection_editor_pin_tabs,
                drawable);
    }

    @Override
    public void onSelectionStateChange(List<TabListEditorItemSelectionId> itemIds) {
        List<Tab> selectedTabs = getTabsOrTabsAndRelatedTabsFromSelection();
        int size = selectedTabs.size();

        if (size == 0) {
            setEnabledAndItemCount(false, size);
            return;
        }

        updateState(selectedTabs);
        setEnabledAndItemCount(mState != State.UNSUPPORTED, selectedTabs.size());
    }

    private void setIcon(Context context) {
        if (mState == State.UNSUPPORTED) return;

        int iconId = mState == State.PIN ? R.drawable.ic_keep_24dp : R.drawable.ic_keep_off_24dp;
        Drawable icon = AppCompatResources.getDrawable(context, iconId);
        getPropertyModel().set(TabListEditorActionProperties.ICON, icon);
    }

    @Override
    public boolean performAction(
            List<Tab> tabs,
            List<String> tabGroupSyncIds,
            @Nullable MotionEventInfo triggeringMotion) {
        assert mState != State.UNSUPPORTED;

        boolean shouldPin = mState == State.PIN;
        TabModel tabModel = getTabGroupModelFilter().getTabModel();
        for (Tab tab : tabs) {
            if (shouldPin) {
                tabModel.pinTab(tab.getId(), /* showUngroupDialog= */ false);
                TabUiMetricsHelper.recordSelectionEditorActionMetrics(
                        TabUiMetricsHelper.TabListEditorActionMetricGroups.PIN_TABS);
            } else {
                tabModel.unpinTab(tab.getId());
                TabUiMetricsHelper.recordSelectionEditorActionMetrics(
                        TabUiMetricsHelper.TabListEditorActionMetricGroups.UNPIN_TABS);
            }
        }

        updateState(tabs);
        return true;
    }

    @Override
    public boolean shouldHideEditorAfterAction() {
        return true;
    }

    private void updateState(List<Tab> selectedTabs) {
        if (selectedTabs.isEmpty()) {
            mState = State.UNSUPPORTED;
            return;
        }

        boolean allPinned = true;
        boolean allUnpinned = true;
        boolean hasGroups = false;

        for (Tab tab : selectedTabs) {
            if (tab == null) continue;
            if (tab.getTabGroupId() != null) {
                hasGroups = true;
                break;
            }

            if (tab.getIsPinned()) {
                allUnpinned = false;
            } else {
                allPinned = false;
            }
        }

        if (hasGroups) {
            mState = State.UNSUPPORTED;
        } else if (allPinned) {
            mState = State.UNPIN;
            setActionText(
                    R.plurals.tab_selection_editor_unpin_tabs,
                    R.plurals.accessibility_tab_selection_editor_unpin_tabs);
            setIcon(selectedTabs.get(0).getContext());
        } else if (allUnpinned) {
            mState = State.PIN;
            setActionText(
                    R.plurals.tab_selection_editor_pin_tabs,
                    R.plurals.accessibility_tab_selection_editor_pin_tabs);
            setIcon(selectedTabs.get(0).getContext());
        } else {
            mState = State.UNSUPPORTED;
        }
    }
}
