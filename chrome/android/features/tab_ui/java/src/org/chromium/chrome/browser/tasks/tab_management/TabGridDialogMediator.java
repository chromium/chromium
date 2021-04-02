// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMediator.INITIAL_SCROLL_INDEX_OFFSET_GTS;

import android.content.ComponentName;
import android.content.Context;
import android.content.res.ColorStateList;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegateImpl.ShareOrigin;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * A mediator for the TabGridDialog component, responsible for communicating
 * with the components' coordinator as well as managing the business logic
 * for dialog show/hide.
 */
public class TabGridDialogMediator implements SnackbarManager.SnackbarController {
    /**
     * Defines an interface for a {@link TabGridDialogMediator} to control dialog.
     */
    interface DialogController {
        /**
         * Handles a reset event originated from {@link TabGridDialogMediator} and {@link
         * TabSwitcherMediator}.
         *
         * @param tabs List of Tabs to reset.
         */
        void resetWithListOfTabs(@Nullable List<Tab> tabs);

        /**
         * Hide the TabGridDialog
         * @param showAnimation Whether to show an animation when hiding the dialog.
         */
        void hideDialog(boolean showAnimation);

        /**
         * @return Whether or not the TabGridDialog consumed the event.
         */
        boolean handleBackPressed();

        /**
         * @return Whether the TabGridDialog is visible.
         */
        boolean isVisible();
    }

    /**
     * Defines an interface for a {@link TabGridDialogMediator} to get the source {@link View}
     * in order to prepare show/hide animation.
     */
    interface AnimationSourceViewProvider {
        /**
         * Provide {@link View} of the source item to setup the animation.
         *
         * @param tabId The id of the tab whose position is requested.
         * @return The source {@link View} used to setup the animation.
         */
        View getAnimationSourceViewForTab(int tabId);
    }

    private final Context mContext;
    private final PropertyModel mModel;
    private final TabModelSelector mTabModelSelector;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final TabModelObserver mTabModelObserver;
    private final TabCreatorManager mTabCreatorManager;
    private final DialogController mDialogController;
    private final TabSwitcherMediator.ResetHandler mTabSwitcherResetHandler;
    private final AnimationSourceViewProvider mAnimationSourceViewProvider;
    private final DialogHandler mTabGridDialogHandler;
    private final ObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    private final Runnable mScrimClickRunnable;
    private final String mComponentName;

    private TabGroupTitleEditor mTabGroupTitleEditor;
    private TabSelectionEditorCoordinator
            .TabSelectionEditorController mTabSelectionEditorController;
    private KeyboardVisibilityDelegate.KeyboardVisibilityListener mKeyboardVisibilityListener;
    private int mCurrentTabId = Tab.INVALID_TAB_ID;
    private boolean mIsUpdatingTitle;
    private String mCurrentGroupModifiedTitle;
    private Callback<Integer> mToolbarMenuCallback;

    TabGridDialogMediator(Context context, DialogController dialogController, PropertyModel model,
            TabModelSelector tabModelSelector, TabCreatorManager tabCreatorManager,
            TabSwitcherMediator.ResetHandler tabSwitcherResetHandler,
            AnimationSourceViewProvider animationSourceViewProvider,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            SnackbarManager snackbarManager, String componentName) {
        mContext = context;
        mModel = model;
        mTabModelSelector = tabModelSelector;
        mTabCreatorManager = tabCreatorManager;
        mDialogController = dialogController;
        mTabSwitcherResetHandler = tabSwitcherResetHandler;
        mAnimationSourceViewProvider = animationSourceViewProvider;
        mTabGridDialogHandler = new DialogHandler();
        mShareDelegateSupplier = shareDelegateSupplier;
        mComponentName = componentName;

        // Register for tab model.
        mTabModelObserver = new TabModelObserver() {
            @Override
            public void didAddTab(
                    Tab tab, @TabLaunchType int type, @TabCreationState int creationState) {
                if (!mTabModelSelector.isTabStateInitialized()) {
                    return;
                }
                hideDialog(false);
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                updateDialog();
                updateGridTabSwitcher();
                snackbarManager.dismissSnackbars(TabGridDialogMediator.this, tab.getId());
            }

            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                if (type == TabSelectionType.FROM_USER) {
                    // Cancel the zooming into tab grid card animation.
                    hideDialog(false);
                }
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                List<Tab> relatedTabs = getRelatedTabs(tab.getId());
                // If the group is empty, update the animation and hide the dialog.
                if (relatedTabs.size() == 0) {
                    hideDialog(false);
                    return;
                }
                // If current tab is closed and tab group is not empty, hand over ID of the next
                // tab in the group to mCurrentTabId.
                if (tab.getId() == mCurrentTabId) {
                    mCurrentTabId = relatedTabs.get(0).getId();
                }
                updateDialog();
                updateGridTabSwitcher();
            }

            @Override
            public void tabPendingClosure(Tab tab) {
                if (!mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE)) return;
                snackbarManager.showSnackbar(
                        Snackbar.make(tab.getTitle(), TabGridDialogMediator.this,
                                        Snackbar.TYPE_ACTION, Snackbar.UMA_TAB_CLOSE_UNDO)
                                .setTemplateText(
                                        mContext.getString(R.string.undo_bar_close_message))
                                .setAction(mContext.getString(R.string.undo), tab.getId()));
            }

            @Override
            public void tabClosureCommitted(Tab tab) {
                snackbarManager.dismissSnackbars(TabGridDialogMediator.this, tab.getId());
            }
        };

        mTabModelSelectorObserver = new TabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                boolean isIncognito = newModel.isIncognito();
                int dialogBackgroundResource = isIncognito
                        ? R.drawable.tab_grid_dialog_background_incognito
                        : R.drawable.tab_grid_dialog_background;
                ColorStateList tintList = isIncognito
                        ? AppCompatResources.getColorStateList(
                                mContext, R.color.default_icon_color_light_tint_list)
                        : AppCompatResources.getColorStateList(
                                mContext, R.color.default_icon_color_tint_list);
                int ungroupBarBackgroundColorId = isIncognito
                        ? R.color.tab_grid_dialog_background_color_incognito
                        : R.color.tab_grid_dialog_background_color;
                int ungroupBarHoveredBackgroundColorId = isIncognito
                        ? R.color.tab_grid_card_selected_color_incognito
                        : R.color.tab_grid_card_selected_color;
                int ungroupBarTextAppearance = isIncognito
                        ? R.style.TextAppearance_TextMediumThick_Blue_Light
                        : R.style.TextAppearance_TextMediumThick_Blue;

                mModel.set(TabGridPanelProperties.DIALOG_BACKGROUND_RESOURCE_ID,
                        dialogBackgroundResource);
                mModel.set(TabGridPanelProperties.TINT, tintList);
                mModel.set(TabGridPanelProperties.DIALOG_UNGROUP_BAR_BACKGROUND_COLOR_ID,
                        ungroupBarBackgroundColorId);
                mModel.set(TabGridPanelProperties.DIALOG_UNGROUP_BAR_HOVERED_BACKGROUND_COLOR_ID,
                        ungroupBarHoveredBackgroundColorId);
                mModel.set(TabGridPanelProperties.DIALOG_UNGROUP_BAR_TEXT_APPEARANCE,
                        ungroupBarTextAppearance);
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        // Setup ScrimView click Runnable.
        mScrimClickRunnable = () -> {
            if (!TabUiFeatureUtilities.isLaunchPolishEnabled()) {
                mModel.set(TabGridPanelProperties.IS_KEYBOARD_VISIBLE, false);
                mModel.set(TabGridPanelProperties.IS_TITLE_TEXT_FOCUSED, false);
            }
            hideDialog(true);
            RecordUserAction.record("TabGridDialog.Exit");
        };
        mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false);
    }

    public void initWithNative(@Nullable TabSelectionEditorCoordinator
                                       .TabSelectionEditorController tabSelectionEditorController,
            TabGroupTitleEditor tabGroupTitleEditor) {
        mTabSelectionEditorController = tabSelectionEditorController;
        mTabGroupTitleEditor = tabGroupTitleEditor;
        mTabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(mTabModelObserver);

        assert mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter()
                        instanceof TabGroupModelFilter;

        mToolbarMenuCallback = result -> {
            if (result == R.id.ungroup_tab) {
                if (!TabUiFeatureUtilities.isLaunchPolishEnabled()) {
                    mModel.set(TabGridPanelProperties.IS_KEYBOARD_VISIBLE, false);
                }
                mModel.set(TabGridPanelProperties.IS_TITLE_TEXT_FOCUSED, false);
                List<Tab> tabs = getRelatedTabs(mCurrentTabId);
                if (mTabSelectionEditorController != null) {
                    mTabSelectionEditorController.show(tabs);
                }
            } else if (result == R.id.share_tab_group) {
                Tab tab = mTabModelSelector.getTabById(mCurrentTabId);
                ShareParams shareParams =
                        new ShareParams
                                .Builder(tab.getWindowAndroid(),
                                        mModel.get(TabGridPanelProperties.HEADER_TITLE), "")
                                .setText(getTabGroupStringForSharing())
                                .setCallback(new ShareParams.TargetChosenCallback() {
                                    @Override
                                    public void onTargetChosen(ComponentName chosenComponent) {
                                        RecordUserAction.record(
                                                "TabGridDialog.SharedGroupAsTextList");
                                    }

                                    @Override
                                    public void onCancel() {}
                                })
                                .build();
                // TODO(crbug.com/1085078): Sharing hub is suppressed for tab group sharing.
                // Re-enable it when tab group sharing is supported by sharing hub.
                ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder()
                                                              .setSharingTabGroup(true)
                                                              .setSaveLastUsed(true)
                                                              .build();
                mShareDelegateSupplier.get().share(
                        shareParams, chromeShareExtras, ShareOrigin.TAB_GROUP);
            }

            if (TabUiFeatureUtilities.isLaunchPolishEnabled()) {
                if (result == R.id.edit_group_name) {
                    mModel.set(TabGridPanelProperties.IS_TITLE_TEXT_FOCUSED, true);
                }
            }
        };

        // Setup toolbar button click listeners.
        setupToolbarClickHandlers();

        if (TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled()) {
            // Setup toolbar edit text.
            setupToolbarEditText();

            // Setup dialog selection editor.
            setupDialogSelectionEditor();
            mModel.set(TabGridPanelProperties.MENU_CLICK_LISTENER, getMenuButtonClickListener());
        }
    }

    void hideDialog(boolean showAnimation) {
        if (!mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE)) return;
        if (!showAnimation) {
            mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, null);
        } else {
            if (mAnimationSourceViewProvider != null && mCurrentTabId != Tab.INVALID_TAB_ID) {
                mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW,
                        mAnimationSourceViewProvider.getAnimationSourceViewForTab(mCurrentTabId));
            }
        }
        if (mTabSelectionEditorController != null) {
            mTabSelectionEditorController.hide();
        }
        saveCurrentGroupModifiedTitle();
        if (TabUiFeatureUtilities.isLaunchPolishEnabled()) {
            mModel.set(TabGridPanelProperties.IS_TITLE_TEXT_FOCUSED, false);
        }
        mDialogController.resetWithListOfTabs(null);
    }

    void onReset(@Nullable List<Tab> tabs) {
        if (tabs == null) {
            mCurrentTabId = Tab.INVALID_TAB_ID;
        } else {
            TabModelFilter filter =
                    mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
            mCurrentTabId = filter.getTabAt(filter.indexOf(tabs.get(0))).getId();
        }

        if (mCurrentTabId != Tab.INVALID_TAB_ID) {
            if (mAnimationSourceViewProvider != null) {
                mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW,
                        mAnimationSourceViewProvider.getAnimationSourceViewForTab(mCurrentTabId));
            } else {
                mModel.set(TabGridPanelProperties.ANIMATION_SOURCE_VIEW, null);
            }
            updateDialog();
            updateDialogScrollPosition();
            mModel.set(TabGridPanelProperties.SCRIMVIEW_CLICK_RUNNABLE, mScrimClickRunnable);
            mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, true);
        } else {
            mModel.set(TabGridPanelProperties.IS_DIALOG_VISIBLE, false);
        }
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        if (mTabModelObserver != null) {
            mTabModelSelector.getTabModelFilterProvider().removeTabModelFilterObserver(
                    mTabModelObserver);
        }
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        KeyboardVisibilityDelegate.getInstance().removeKeyboardVisibilityListener(
                mKeyboardVisibilityListener);
    }

    boolean isVisible() {
        return mModel.get(TabGridPanelProperties.IS_DIALOG_VISIBLE);
    }

    private void updateGridTabSwitcher() {
        if (!isVisible() || mTabSwitcherResetHandler == null) return;
        mTabSwitcherResetHandler.resetWithTabList(
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter(), false,
                false);
    }

    private void updateDialog() {
        List<Tab> relatedTabs = getRelatedTabs(mCurrentTabId);
        int tabsCount = relatedTabs.size();
        if (tabsCount == 0) {
            hideDialog(true);
            return;
        }
        if (mTabGroupTitleEditor != null) {
            Tab currentTab = mTabModelSelector.getTabById(mCurrentTabId);
            String storedTitle = mTabGroupTitleEditor.getTabGroupTitle(getRootId(currentTab));
            if (storedTitle != null && relatedTabs.size() > 1) {
                if (TabUiFeatureUtilities.isLaunchPolishEnabled()) {
                    mModel.set(TabGridPanelProperties.COLLAPSE_BUTTON_CONTENT_DESCRIPTION,
                            mContext.getResources().getQuantityString(
                                    R.plurals.accessibility_dialog_back_button_with_group_name,
                                    relatedTabs.size(), storedTitle, relatedTabs.size()));
                }
                mModel.set(TabGridPanelProperties.HEADER_TITLE, storedTitle);
                return;
            }
        }
        if (TabUiFeatureUtilities.isLaunchPolishEnabled()) {
            mModel.set(TabGridPanelProperties.COLLAPSE_BUTTON_CONTENT_DESCRIPTION,
                    mContext.getResources().getQuantityString(
                            R.plurals.accessibility_dialog_back_button, relatedTabs.size(),
                            relatedTabs.size()));
        }
        mModel.set(TabGridPanelProperties.HEADER_TITLE,
                mContext.getResources().getQuantityString(
                        R.plurals.bottom_tab_grid_title_placeholder, tabsCount, tabsCount));
    }

    private static int getRootId(Tab tab) {
        return CriticalPersistedTabData.from(tab).getRootId();
    }

    private void updateDialogScrollPosition() {
        // If current selected tab is not within this dialog, always scroll to the top.
        if (mCurrentTabId != mTabModelSelector.getCurrentTabId()) {
            mModel.set(TabGridPanelProperties.INITIAL_SCROLL_INDEX, 0);
            return;
        }
        List<Tab> relatedTabs = getRelatedTabs(mCurrentTabId);
        Tab currentTab = mTabModelSelector.getTabById(mCurrentTabId);
        int initialPosition =
                Math.max(relatedTabs.indexOf(currentTab) - INITIAL_SCROLL_INDEX_OFFSET_GTS, 0);
        mModel.set(TabGridPanelProperties.INITIAL_SCROLL_INDEX, initialPosition);
    }

    private void setupToolbarClickHandlers() {
        mModel.set(
                TabGridPanelProperties.COLLAPSE_CLICK_LISTENER, getCollapseButtonClickListener());
        mModel.set(TabGridPanelProperties.ADD_CLICK_LISTENER, getAddButtonClickListener());
    }

    private void setupDialogSelectionEditor() {
        assert mTabSelectionEditorController != null;
        TabSelectionEditorActionProvider actionProvider =
                new TabSelectionEditorActionProvider(mTabSelectionEditorController,
                        TabSelectionEditorActionProvider.TabSelectionEditorAction.UNGROUP);

        String actionButtonText =
                mContext.getString(R.string.tab_grid_dialog_selection_mode_remove);
        mTabSelectionEditorController.configureToolbar(actionButtonText,
                R.plurals.accessibility_tab_selection_dialog_remove_button, actionProvider, 1,
                null);
    }

    private void setupToolbarEditText() {
        mKeyboardVisibilityListener = isShowing -> {
            mModel.set(TabGridPanelProperties.TITLE_CURSOR_VISIBILITY, isShowing);
            if (!TabUiFeatureUtilities.isLaunchPolishEnabled()) {
                mModel.set(TabGridPanelProperties.IS_TITLE_TEXT_FOCUSED, isShowing);
                mModel.set(TabGridPanelProperties.IS_KEYBOARD_VISIBLE, isShowing);
            } else if (TabUiFeatureUtilities.isLaunchPolishEnabled() && !isShowing) {
                mModel.set(TabGridPanelProperties.IS_TITLE_TEXT_FOCUSED, false);
            }
            if (!isShowing) {
                saveCurrentGroupModifiedTitle();
            }
        };
        KeyboardVisibilityDelegate.getInstance().addKeyboardVisibilityListener(
                mKeyboardVisibilityListener);

        TextWatcher textWatcher = new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override
            public void afterTextChanged(Editable s) {
                if (!mIsUpdatingTitle) return;
                mCurrentGroupModifiedTitle = s.toString();
            }
        };
        mModel.set(TabGridPanelProperties.TITLE_TEXT_WATCHER, textWatcher);

        View.OnFocusChangeListener onFocusChangeListener = (v, hasFocus) -> {
            mIsUpdatingTitle = hasFocus;
            if (!TabUiFeatureUtilities.isLaunchPolishEnabled()) return;
            mModel.set(TabGridPanelProperties.IS_KEYBOARD_VISIBLE, hasFocus);
            mModel.set(TabGridPanelProperties.IS_TITLE_TEXT_FOCUSED, hasFocus);
        };
        mModel.set(TabGridPanelProperties.TITLE_TEXT_ON_FOCUS_LISTENER, onFocusChangeListener);
    }

    private View.OnClickListener getCollapseButtonClickListener() {
        return view -> {
            if (!TabUiFeatureUtilities.isLaunchPolishEnabled()) {
                mModel.set(TabGridPanelProperties.IS_KEYBOARD_VISIBLE, false);
            }
            hideDialog(true);
            RecordUserAction.record("TabGridDialog.Exit");
        };
    }

    private View.OnClickListener getAddButtonClickListener() {
        return view -> {
            // Get the current Tab first since hideDialog causes mCurrentTabId to be
            // Tab.INVALID_TAB_ID.
            Tab currentTab = mTabModelSelector.getTabById(mCurrentTabId);
            hideDialog(false);
            if (currentTab == null) {
                mTabCreatorManager.getTabCreator(mTabModelSelector.isIncognitoSelected())
                        .launchNTP();
                return;
            }
            List<Tab> relatedTabs = getRelatedTabs(currentTab.getId());

            assert relatedTabs.size() > 0;

            Tab parentTabToAttach = relatedTabs.get(relatedTabs.size() - 1);
            mTabCreatorManager.getTabCreator(currentTab.isIncognito())
                    .createNewTab(new LoadUrlParams(UrlConstants.NTP_URL),
                            TabLaunchType.FROM_TAB_GROUP_UI, parentTabToAttach);
            RecordUserAction.record("MobileNewTabOpened." + mComponentName);
        };
    }

    private View.OnClickListener getMenuButtonClickListener() {
        assert mTabSelectionEditorController != null;
        return TabGridDialogMenuCoordinator.getTabGridDialogMenuOnClickListener(
                mToolbarMenuCallback);
    }

    private List<Tab> getRelatedTabs(int tabId) {
        return mTabModelSelector.getTabModelFilterProvider()
                .getCurrentTabModelFilter()
                .getRelatedTabList(tabId);
    }

    private void saveCurrentGroupModifiedTitle() {
        // When current group no longer exists, skip saving the title.
        if (getRelatedTabs(mCurrentTabId).size() < 2) {
            mCurrentGroupModifiedTitle = null;
        }

        if (mCurrentGroupModifiedTitle == null) {
            return;
        }
        assert mTabGroupTitleEditor != null;

        Tab currentTab = mTabModelSelector.getTabById(mCurrentTabId);
        if (mCurrentGroupModifiedTitle.length() == 0) {
            // When dialog title is empty, delete previously stored title and restore default title.
            mTabGroupTitleEditor.deleteTabGroupTitle(getRootId(currentTab));
            int tabsCount = getRelatedTabs(mCurrentTabId).size();
            assert tabsCount >= 2;

            String originalTitle = mContext.getResources().getQuantityString(
                    R.plurals.bottom_tab_grid_title_placeholder, tabsCount, tabsCount);
            if (TabUiFeatureUtilities.isLaunchPolishEnabled()) {
                mModel.set(TabGridPanelProperties.COLLAPSE_BUTTON_CONTENT_DESCRIPTION,
                        mContext.getResources().getQuantityString(
                                R.plurals.accessibility_dialog_back_button, tabsCount, tabsCount));
            }
            mModel.set(TabGridPanelProperties.HEADER_TITLE, originalTitle);
            mTabGroupTitleEditor.updateTabGroupTitle(currentTab, originalTitle);
            return;
        }
        mTabGroupTitleEditor.storeTabGroupTitle(getRootId(currentTab), mCurrentGroupModifiedTitle);
        mTabGroupTitleEditor.updateTabGroupTitle(currentTab, mCurrentGroupModifiedTitle);
        if (TabUiFeatureUtilities.isLaunchPolishEnabled()) {
            int relatedTabsCount = getRelatedTabs(mCurrentTabId).size();
            mModel.set(TabGridPanelProperties.COLLAPSE_BUTTON_CONTENT_DESCRIPTION,
                    mContext.getResources().getQuantityString(
                            R.plurals.accessibility_dialog_back_button_with_group_name,
                            relatedTabsCount, mCurrentGroupModifiedTitle, relatedTabsCount));
        }
        mModel.set(TabGridPanelProperties.HEADER_TITLE, mCurrentGroupModifiedTitle);
        RecordUserAction.record("TabGridDialog.TabGroupNamedInDialog");
        mCurrentGroupModifiedTitle = null;
    }

    TabListMediator.TabGridDialogHandler getTabGridDialogHandler() {
        return mTabGridDialogHandler;
    }

    // SnackbarManager.SnackbarController implementation.
    @Override
    public void onAction(Object actionData) {
        int tabId = (int) actionData;
        TabModel model = mTabModelSelector.getModelForTabId(tabId);
        if (model != null) {
            model.cancelTabClosure(tabId);
        }
    }

    @Override
    public void onDismissNoAction(Object actionData) {
        int tabId = (int) actionData;
        TabModel model = mTabModelSelector.getModelForTabId(tabId);
        if (model != null) {
            model.commitTabClosure(tabId);
        }
    }

    /**
     * A handler that handles TabGridDialog related changes originated from {@link TabListMediator}
     * and {@link TabGridItemTouchHelperCallback}.
     */
    class DialogHandler implements TabListMediator.TabGridDialogHandler {
        @Override
        public void updateUngroupBarStatus(@TabGridDialogView.UngroupBarStatus int status) {
            mModel.set(TabGridPanelProperties.UNGROUP_BAR_STATUS, status);
        }

        @Override
        public void updateDialogContent(int tabId) {
            mCurrentTabId = tabId;
            updateDialog();
        }
    }

    private String getTabGroupStringForSharing() {
        StringBuilder sb = new StringBuilder();
        List<Tab> tabgroup = getRelatedTabs(mCurrentTabId);
        assert tabgroup.size() > 0;
        for (int i = 0; i < tabgroup.size(); i++) {
            sb.append(i + 1).append(". ").append(tabgroup.get(i).getUrlString()).append("\n");
        }
        return sb.toString();
    }

    @VisibleForTesting
    String getTabGroupStringForSharingForTesting() {
        return getTabGroupStringForSharing();
    }

    @VisibleForTesting
    int getCurrentTabIdForTesting() {
        return mCurrentTabId;
    }

    @VisibleForTesting
    void setCurrentTabIdForTesting(int tabId) {
        mCurrentTabId = tabId;
    }

    @VisibleForTesting
    KeyboardVisibilityDelegate.KeyboardVisibilityListener
    getKeyboardVisibilityListenerForTesting() {
        return mKeyboardVisibilityListener;
    }

    @VisibleForTesting
    boolean getIsUpdatingTitleForTesting() {
        return mIsUpdatingTitle;
    }

    @VisibleForTesting
    String getCurrentGroupModifiedTitleForTesting() {
        return mCurrentGroupModifiedTitle;
    }

    @VisibleForTesting
    Callback<Integer> getToolbarMenuCallbackForTesting() {
        return mToolbarMenuCallback;
    }

    @VisibleForTesting
    Runnable getScrimClickRunnableForTesting() {
        return mScrimClickRunnable;
    }
}
