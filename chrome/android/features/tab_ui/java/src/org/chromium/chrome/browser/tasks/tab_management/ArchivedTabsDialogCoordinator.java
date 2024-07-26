// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab_ui.OnTabSelectingListener;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.NavigationProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorController;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.GridCardOnClickListenerProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

public class ArchivedTabsDialogCoordinator {

    /** Interface exposing functionality to the menu items for the archived tabs dialog */
    public interface ArchiveDelegate {
        /** Restore all tabs from the archived tab model. */
        void restoreAllArchivedTabs();

        /** Open the archive settings page. */
        void openArchiveSettings();

        /** Start tab selection process. */
        void startTabSelection();

        /** Restore the given list of tabs. */
        void restoreArchivedTabs(List<Tab> tabs);

        /** Close the given list of tabs. */
        void closeArchivedTabs(List<Tab> tabs);
    }

    private final ArchiveDelegate mArchiveDelegate =
            new ArchiveDelegate() {
                @Override
                public void restoreAllArchivedTabs() {
                    List<Tab> tabs = TabModelUtils.convertTabListToListOfTabs(mArchivedTabModel);
                    int tabCount = tabs.size();
                    ArchivedTabsDialogCoordinator.this.restoreArchivedTabs(tabs);
                    RecordHistogram.recordCount1000Histogram(
                            "Tabs.RestoreAllArchivedTabsMenuItem.TabCount", tabCount);
                    RecordUserAction.record("Tabs.RestoreAllArchivedTabsMenuItem");
                }

                @Override
                public void openArchiveSettings() {
                    new SettingsLauncherImpl()
                            .launchSettingsActivity(mContext, TabArchiveSettingsFragment.class);
                    RecordUserAction.record("Tabs.OpenArchivedTabsSettingsMenuItem");
                }

                @Override
                public void startTabSelection() {
                    moveToState(TabActionState.SELECTABLE);
                    RecordUserAction.record("Tabs.SelectArchivedTabsMenuItem");
                }

                @Override
                public void restoreArchivedTabs(List<Tab> tabs) {
                    int tabCount = tabs.size();
                    ArchivedTabsDialogCoordinator.this.restoreArchivedTabs(tabs);
                    moveToState(TabActionState.CLOSABLE);
                    RecordHistogram.recordCount1000Histogram(
                            "Tabs.RestoreArchivedTabsMenuItem.TabCount", tabCount);
                    RecordUserAction.record("Tabs.RestoreArchivedTabsMenuItem");
                }

                @Override
                public void closeArchivedTabs(List<Tab> tabs) {
                    int tabCount = tabs.size();
                    ArchivedTabsDialogCoordinator.this.closeArchivedTabs(tabs);
                    RecordHistogram.recordCount1000Histogram(
                            "Tabs.CloseArchivedTabsMenuItem.TabCount", tabCount);
                    RecordUserAction.record("Tabs.CloseArchivedTabsMenuItem");
                }
            };

    private final NavigationProvider mNavigationProvider =
            new NavigationProvider() {
                @Override
                public void goBack() {
                    if (mTabActionState == TabActionState.CLOSABLE) {
                        hide();
                    } else {
                        moveToState(TabActionState.CLOSABLE);
                    }
                }
            };

    /**
     * Observes the tab count in the archived tab model to (1) update the title and (2) hide the
     * dialog when no archived tabs remain.
     */
    private final Callback<Integer> mTabCountObserver =
            (count) -> {
                if (count <= 0) {
                    hide();
                    return;
                }
                updateTitle();
            };

    /** Used to override the default tab click behavior to restore/open the tab. */
    private final GridCardOnClickListenerProvider mGridCardOnCLickListenerProvider =
            new GridCardOnClickListenerProvider() {
                @Nullable
                @Override
                public TabActionListener openTabGridDialog(@NonNull Tab tab) {
                    return null;
                }

                @Override
                public void onTabSelecting(int tabId, boolean fromActionButton) {
                    hide();

                    Tab tab = mArchivedTabModel.getTabById(tabId);
                    mArchivedTabModelOrchestrator
                            .getTabArchiver()
                            .unarchiveAndRestoreTab(mRegularTabCreator, tab);

                    // Post task to allow the tab to be unregistered.
                    PostTask.postTask(
                            TaskTraits.UI_DEFAULT,
                            () -> mOnTabSelectingListener.onTabSelecting(tab.getId()));
                    RecordUserAction.record("Tabs.RestoreSingleTab");
                }
            };

    private final TabArchiveSettings.Observer mTabArchiveSettingsObserver =
            new TabArchiveSettings.Observer() {
                @Override
                public void onSettingChanged() {
                    updateIphPropertyModel();
                }
            };

    /**
     * Observes the TabListEditor lifecycle to remove the view and hide the dialog. This is useful
     * for when (1) the TabListEditor is expecting the embedding view to be removed from the
     * hierarchy prior to hide completion. (2) If the TabListEditor hides itself outside of the
     * dialog control flow, we want to know about it in order to hide the embedding UI.
     */
    private final TabListEditorCoordinator.LifecycleObserver mTabListEditorLifecycleObserver =
            new TabListEditorCoordinator.LifecycleObserver() {
                @Override
                public void willHide() {
                    mRootView.removeView(mView);
                }

                @Override
                public void didHide() {
                    ArchivedTabsDialogCoordinator.this.hideInternal();
                }
            };

    private final @NonNull Context mContext;
    private final @NonNull ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    private final @NonNull TabModel mArchivedTabModel;
    private final @NonNull BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final @NonNull TabContentManager mTabContentManager;
    private final @TabListMode int mMode;
    private final @NonNull ViewGroup mRootView;
    private final @NonNull SnackbarManager mSnackbarManager;
    private final @NonNull TabCreator mRegularTabCreator;
    private final @NonNull BackPressManager mBackPressManager;
    private final @NonNull TabArchiveSettings mTabArchiveSettings;
    private final @NonNull ModalDialogManager mModalDialogManager;

    private ViewGroup mView;
    private @TabActionState int mTabActionState = TabActionState.CLOSABLE;
    private TabListEditorCoordinator mTabListEditorCoordinator;
    private OnTabSelectingListener mOnTabSelectingListener;
    private PropertyModel mIphMessagePropertyModel;

    /**
     * @param context The android context.
     * @param archivedTabModelOrchestrator The TabModelOrchestrator for archived tabs.
     * @param browserControlsStateProvider Used as a dependency to TabListEditorCoordiantor.
     * @param tabContentManager Used as a dependency to TabListEditorCoordiantor.
     * @param mode Used as a dependency to TabListEditorCoordiantor.
     * @param rootView Used as a dependency to TabListEditorCoordiantor.
     * @param snackbarManager Manages snackbars shown in the app.
     * @param regularTabCreator Handles the creation of regular tabs.
     * @param backPressManager Manages the different back press handlers throughout the app.
     * @param tabArchiveSettings The settings manager for tab archive.
     * @param modalDialogManager Used for managing the modal dialogs.
     */
    public ArchivedTabsDialogCoordinator(
            @NonNull Context context,
            @NonNull ArchivedTabModelOrchestrator archivedTabModelOrchestrator,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull TabContentManager tabContentManager,
            @TabListMode int mode,
            @NonNull ViewGroup rootView,
            @NonNull SnackbarManager snackbarManager,
            @NonNull TabCreator regularTabCreator,
            @NonNull BackPressManager backPressManager,
            @NonNull TabArchiveSettings tabArchiveSettings,
            @NonNull ModalDialogManager modalDialogManager) {
        mContext = context;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mTabContentManager = tabContentManager;
        mMode = mode;
        mRootView = rootView;
        mSnackbarManager = snackbarManager;
        mRegularTabCreator = regularTabCreator;
        mBackPressManager = backPressManager;
        mTabArchiveSettings = tabArchiveSettings;
        mModalDialogManager = modalDialogManager;

        mArchivedTabModelOrchestrator = archivedTabModelOrchestrator;
        mArchivedTabModel =
                mArchivedTabModelOrchestrator
                        .getTabModelSelector()
                        .getModel(/* incognito= */ false);
        mView =
                (ViewGroup)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.archived_tabs_dialog, mRootView, false);
        mView.findViewById(R.id.close_all_tabs_button)
                .setOnClickListener(this::closeAllInactiveTabs);
    }

    /**
     * Shows the dialog.
     *
     * @param onTabSelectingListener Allows a tab to be selected in the main tab switcher.
     */
    public void show(OnTabSelectingListener onTabSelectingListener) {
        boolean tabListFirstShown = false;
        if (mTabListEditorCoordinator == null) {
            tabListFirstShown = true;
            createTabListEditorCoordinator();
        }

        mOnTabSelectingListener = onTabSelectingListener;
        mArchivedTabModel.getTabCountSupplier().addObserver(mTabCountObserver);

        TabListEditorController controller = mTabListEditorCoordinator.getController();
        controller.setLifecycleObserver(mTabListEditorLifecycleObserver);
        controller.show(TabModelUtils.convertTabListToListOfTabs(mArchivedTabModel), 0, null);
        controller.setNavigationProvider(mNavigationProvider);

        // Register the dialog to handle back press events.
        mBackPressManager.addHandler(controller, BackPressHandler.Type.ARCHIVED_TABS_DIALOG);

        // Add the dialog view.
        mRootView.addView(mView);
        // View is obscured by the TabListEditorCoordinator, so it needs to be brought to the front.
        mView.findViewById(R.id.close_all_tabs_button_container).bringToFront();

        // Add the IPH to the TabListEditor.
        if (mTabArchiveSettings.shouldShowDialogIph()) {
            if (tabListFirstShown) {
                mTabListEditorCoordinator.registerItemType(
                        TabProperties.UiType.MESSAGE,
                        new LayoutViewBuilder(R.layout.tab_grid_message_card_item),
                        MessageCardViewBinder::bind);
            }
            mIphMessagePropertyModel =
                    ArchivedTabsIphMessageCardViewModel.create(
                            mContext, this::onIphReviewClicked, this::onIphDismissClicked);
            updateIphPropertyModel();
            mTabListEditorCoordinator.addSpecialListItem(
                    0, UiType.MESSAGE, mIphMessagePropertyModel);
            RecordUserAction.record("Tabs.ArchivedTabsDialogIphShown");
        }
        mTabArchiveSettings.addObserver(mTabArchiveSettingsObserver);

        moveToState(TabActionState.CLOSABLE);
    }

    /** Hides the dialog. */
    public void hide() {
        mRootView.removeView(mView);
        hideInternal();
    }

    void hideInternal() {
        TabListEditorController controller = mTabListEditorCoordinator.getController();
        controller.hide();
        controller.setLifecycleObserver(null);
        mBackPressManager.removeHandler(mTabListEditorCoordinator.getController());
        mArchivedTabModel.getTabCountSupplier().removeObserver(mTabCountObserver);
        mTabArchiveSettings.removeObserver(mTabArchiveSettingsObserver);
    }

    void moveToState(@TabActionState int tabActionState) {
        mTabActionState = tabActionState;
        mTabListEditorCoordinator.getController().setTabActionState(mTabActionState);
        updateTitle();

        List<TabListEditorAction> actions = new ArrayList<>();
        if (mTabActionState == TabActionState.CLOSABLE) {
            actions.add(
                    TabListEditorRestoreAllArchivedTabsAction.createAction(
                            mContext, mArchiveDelegate));
            actions.add(
                    TabListEditorSelectArchivedTabsAction.createAction(mContext, mArchiveDelegate));
            actions.add(
                    TabListEditorArchiveSettingsAction.createAction(mContext, mArchiveDelegate));
        } else if (mTabActionState == TabActionState.SELECTABLE) {
            actions.add(
                    TabListEditorRestoreArchivedTabsAction.createAction(
                            mContext, mArchiveDelegate));
            actions.add(
                    TabListEditorCloseArchivedTabsAction.createAction(mContext, mArchiveDelegate));
        }

        mTabListEditorCoordinator.getController().configureToolbarWithMenuItems(actions);
    }

    @VisibleForTesting
    void updateTitle() {
        int numInactiveTabs = mArchivedTabModel.getCount();
        String title =
                mContext.getResources()
                        .getQuantityString(
                                R.plurals.archived_tabs_dialog_title,
                                numInactiveTabs,
                                numInactiveTabs);
        mTabListEditorCoordinator.getController().setToolbarTitle(title);
    }

    private void createTabListEditorCoordinator() {
        mTabListEditorCoordinator =
                new TabListEditorCoordinator(
                        mContext,
                        mRootView,
                        /* parentView= */ mView,
                        mBrowserControlsStateProvider,
                        mArchivedTabModelOrchestrator
                                .getTabModelSelector()
                                .getTabModelFilterProvider()
                                .getCurrentTabModelFilterSupplier(),
                        mTabContentManager,
                        /* clientTabListRecyclerViewPositionSetter= */ null,
                        mMode,
                        /* displayGroups= */ false,
                        mSnackbarManager,
                        /* bottomSheetController= */ null,
                        TabProperties.TabActionState.CLOSABLE,
                        mGridCardOnCLickListenerProvider,
                        mModalDialogManager);
    }

    @VisibleForTesting
    void closeAllInactiveTabs(View view) {
        int tabCount = mArchivedTabModel.getCount();
        mArchivedTabModel.closeAllTabs(false);
        RecordHistogram.recordCount1000Histogram("Tabs.CloseAllArchivedTabs.TabCount", tabCount);
        RecordUserAction.record("Tabs.CloseAllArchivedTabsMenuItem");
    }

    private void closeArchivedTabs(List<Tab> tabs) {
        mArchivedTabModel.closeMultipleTabs(tabs, /* canUndo= */ true);
    }

    private void restoreArchivedTabs(List<Tab> tabs) {
        for (Tab tab : tabs) {
            mArchivedTabModelOrchestrator
                    .getTabArchiver()
                    .unarchiveAndRestoreTab(mRegularTabCreator, tab);
        }
    }

    private void onIphReviewClicked() {
        new SettingsLauncherImpl()
                .launchSettingsActivity(mContext, TabArchiveSettingsFragment.class);
        RecordUserAction.record("Tabs.ArchivedTabsDialogIphClicked");
    }

    private void onIphDismissClicked(@MessageType int messageType) {
        mTabArchiveSettings.setShouldShowDialogIph(false);
        mTabListEditorCoordinator.removeSpecialListItem(
                UiType.MESSAGE, MessageService.MessageType.ARCHIVED_TABS_IPH_MESSAGE);
        RecordUserAction.record("Tabs.ArchivedTabsDialogIphDismissed");
    }

    private void updateIphPropertyModel() {
        if (mIphMessagePropertyModel == null) return;

        int archiveTimeDeltaDays = mTabArchiveSettings.getArchiveTimeDeltaDays();
        int autoDeleteTimeDeletaDays = mTabArchiveSettings.getAutoDeleteTimeDeltaDays();
        String settingsTitle =
                mContext.getString(R.string.archived_tab_iph_card_subtitle_settings_title);
        String description =
                mContext.getString(
                        R.string.archived_tab_iph_card_subtitle,
                        archiveTimeDeltaDays,
                        autoDeleteTimeDeletaDays,
                        settingsTitle);
        SpannableString ss = new SpannableString(description);
        ForegroundColorSpan fcs =
                new ForegroundColorSpan(SemanticColorUtils.getDefaultTextColorAccent1(mContext));
        ss.setSpan(
                fcs,
                description.indexOf(settingsTitle),
                description.indexOf(settingsTitle) + settingsTitle.length(),
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);

        mIphMessagePropertyModel.set(MessageCardViewProperties.DESCRIPTION_TEXT, ss);
    }

    private boolean shouldShowIph() {
        return true;
    }

    // Testing-specific methods

    void setTabListEditorCoordinatorForTesting(TabListEditorCoordinator tabListEditorCoordinator) {
        mTabListEditorCoordinator = tabListEditorCoordinator;
    }

    ArchiveDelegate getArchiveDelegateForTesting() {
        return mArchiveDelegate;
    }

    TabListEditorCoordinator.LifecycleObserver getTabListEditorLifecycleObserver() {
        return mTabListEditorLifecycleObserver;
    }
}
