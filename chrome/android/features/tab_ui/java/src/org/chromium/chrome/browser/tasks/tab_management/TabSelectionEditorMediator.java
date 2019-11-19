// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Rect;
import android.support.annotation.ColorInt;
import android.support.v7.content.res.AppCompatResources;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * This class is the mediator that contains all business logic for TabSelectionEditor component. It
 * is also responsible for resetting the selectable tab grid based on visibility property.
 */
class TabSelectionEditorMediator
        implements TabSelectionEditorCoordinator.TabSelectionEditorController {
    // TODO(977271): Unify similar interfaces in other components that used the TabListCoordinator.
    /**
     * Interface for resetting the selectable tab grid.
     */
    interface ResetHandler {
        /**
         * Handles the reset event.
         * @param tabs List of {@link Tab}s to reset.
         * @param preSelectedCount First {@code preSelectedCount} {@code tabs} are pre-selected.
         */
        void resetWithListOfTabs(@Nullable List<Tab> tabs, int preSelectedCount);
    }

    /**
     * An interface to provide the {@link Rect} used to position the selection editor on screen.
     */
    public interface TabSelectionEditorPositionProvider {
        /**
         * This method fetches the {@link Rect} used to position the selection editor layout.
         * @return The {@link Rect} indicates where to show the selection editor layout. This Rect
         * should never be null.
         */
        @NonNull
        Rect getSelectionEditorPositionRect();
    }

    private final Context mContext;
    private final TabModelSelector mTabModelSelector;
    private final ResetHandler mResetHandler;
    private final PropertyModel mModel;
    private final SelectionDelegate<Integer> mSelectionDelegate;
    private final TabModelSelectorTabModelObserver mTabModelObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final TabSelectionEditorPositionProvider mPositionProvider;
    private TabSelectionEditorActionProvider mActionProvider;

    private final View.OnClickListener mNavigationClickListener = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            RecordUserAction.record("TabMultiSelect.Cancelled");
            hide();
        }
    };

    private final View.OnClickListener mActionButtonOnClickListener = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            List<Tab> selectedTabs = new ArrayList<>();

            for (int tabId : mSelectionDelegate.getSelectedItems()) {
                selectedTabs.add(
                        TabModelUtils.getTabById(mTabModelSelector.getCurrentModel(), tabId));
            }

            if (mActionProvider == null) return;
            mActionProvider.processSelectedTabs(selectedTabs, mTabModelSelector);
        }
    };

    TabSelectionEditorMediator(Context context, TabModelSelector tabModelSelector,
            ResetHandler resetHandler, PropertyModel model,
            SelectionDelegate<Integer> selectionDelegate,
            @Nullable TabSelectionEditorMediator
                    .TabSelectionEditorPositionProvider positionProvider) {
        mContext = context;
        mTabModelSelector = tabModelSelector;
        mResetHandler = resetHandler;
        mModel = model;
        mSelectionDelegate = selectionDelegate;
        mPositionProvider = positionProvider;

        mModel.set(
                TabSelectionEditorProperties.TOOLBAR_NAVIGATION_LISTENER, mNavigationClickListener);
        mModel.set(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_LISTENER,
                mActionButtonOnClickListener);

        mTabModelObserver = new TabModelSelectorTabModelObserver(mTabModelSelector) {
            @Override
            public void didAddTab(Tab tab, int type) {
                // When tab is added due to multi-window close or moving between multiple windows,
                // force hiding the selection editor.
                if (type == TabLaunchType.FROM_RESTORE || type == TabLaunchType.FROM_REPARENTING) {
                    hide();
                }
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                if (isEditorVisible()) hide();
            }
        };

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                // Incognito in both light/dark theme is the same as non-incognito mode in dark
                // theme. Non-incognito mode and incognito in both light/dark themes in dark theme
                // all look dark.
                boolean isIncognito = newModel.isIncognito();
                @ColorInt
                int primaryColor = ApiCompatibilityUtils.getColor(mContext.getResources(),
                        isIncognito ? R.color.dark_primary_color : R.color.modern_primary_color);
                // TODO(995876): Update color modern_blue_300 to active_color_dark when the
                // associated bug is landed.
                @ColorInt
                int toolbarBackgroundColor = ApiCompatibilityUtils.getColor(mContext.getResources(),
                        isIncognito ? R.color.modern_blue_300 : R.color.light_active_color);
                ColorStateList toolbarTintColorList = AppCompatResources.getColorStateList(mContext,
                        isIncognito ? R.color.dark_text_color_list
                                    : R.color.default_text_color_inverse_list);
                int textAppearance = isIncognito ? R.style.TextAppearance_BlackHeadline_Black
                                                 : R.style.TextAppearance_Headline_Inverse;

                mModel.set(TabSelectionEditorProperties.PRIMARY_COLOR, primaryColor);
                mModel.set(TabSelectionEditorProperties.TOOLBAR_BACKGROUND_COLOR,
                        toolbarBackgroundColor);
                mModel.set(TabSelectionEditorProperties.TOOLBAR_GROUP_BUTTON_TINT,
                        toolbarTintColorList);
                mModel.set(TabSelectionEditorProperties.TOOLBAR_TEXT_APPEARANCE, textAppearance);
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        // Default action for action button is to group selected tabs.
        mActionProvider = new TabSelectionEditorActionProvider(
                this, TabSelectionEditorActionProvider.TabSelectionEditorAction.GROUP);

        if (mPositionProvider != null) {
            mModel.set(TabSelectionEditorProperties.SELECTION_EDITOR_GLOBAL_LAYOUT_LISTENER,
                    ()
                            -> mModel.set(
                                    TabSelectionEditorProperties.SELECTION_EDITOR_POSITION_RECT,
                                    mPositionProvider.getSelectionEditorPositionRect()));
        }
    }

    private boolean isEditorVisible() {
        return mModel.get(TabSelectionEditorProperties.IS_VISIBLE);
    }

    /**
     * {@link TabSelectionEditorCoordinator.TabSelectionEditorController} implementation.
     */
    @Override
    public void show(List<Tab> tabs) {
        show(tabs, 0);
    }

    @Override
    public void show(List<Tab> tabs, int preSelectedTabCount) {
        mSelectionDelegate.setSelectionModeEnabledForZeroItems(true);

        if (preSelectedTabCount > 0) {
            assert preSelectedTabCount <= tabs.size();

            Set<Integer> preSelectedTabIds = new HashSet<>();

            for (int i = 0; i < preSelectedTabCount; i++) {
                preSelectedTabIds.add(tabs.get(i).getId());
            }

            mSelectionDelegate.setSelectedItems(preSelectedTabIds);
        }

        mResetHandler.resetWithListOfTabs(tabs, preSelectedTabCount);

        if (mPositionProvider != null) {
            mModel.set(TabSelectionEditorProperties.SELECTION_EDITOR_POSITION_RECT,
                    mPositionProvider.getSelectionEditorPositionRect());
        }
        mModel.set(TabSelectionEditorProperties.IS_VISIBLE, true);
    }

    @Override
    public void configureToolbar(@Nullable String actionButtonText,
            @Nullable TabSelectionEditorActionProvider actionProvider,
            int actionButtonEnablingThreshold,
            @Nullable View.OnClickListener navigationButtonOnClickListener) {
        if (actionButtonText != null) {
            mModel.set(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_TEXT, actionButtonText);
        }
        if (actionProvider != null) {
            mActionProvider = actionProvider;
        }
        if (actionButtonEnablingThreshold != -1) {
            mModel.set(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_ENABLING_THRESHOLD,
                    actionButtonEnablingThreshold);
        }
        if (navigationButtonOnClickListener != null) {
            mModel.set(TabSelectionEditorProperties.TOOLBAR_NAVIGATION_LISTENER,
                    navigationButtonOnClickListener);
        }
    }

    @Override
    public boolean handleBackPressed() {
        if (!isEditorVisible()) return false;
        hide();
        RecordUserAction.record("TabMultiSelect.Cancelled");
        return true;
    }

    @Override
    public void hide() {
        mResetHandler.resetWithListOfTabs(null, 0);
        mModel.set(TabSelectionEditorProperties.IS_VISIBLE, false);
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        mTabModelObserver.destroy();
        if (mTabModelSelector != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        }
    }
}
