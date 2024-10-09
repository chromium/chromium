// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.content.res.Resources;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.util.Size;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.Function;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab_ui.OnTabSelectingListener;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.StrictButtonPressController.ButtonClickResult;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListItemSizeChangedObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.NavigationProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorController;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.GridCardOnClickListenerProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.FadingShadow;
import org.chromium.components.browser_ui.widget.FadingShadowView;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

public class ArchivedTabsDialogCoordinator implements SnackbarManager.SnackbarManageable {

    private static final int ANIM_DURATION_MS = 250;

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
                    SettingsNavigationFactory.createSettingsNavigation()
                            .startSettings(mContext, TabArchiveSettingsFragment.class);
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
                    mArchivedTabModel.closeTabs(TabClosureParams.closeTabs(tabs).build());
                    RecordHistogram.recordCount1000Histogram(
                            "Tabs.CloseArchivedTabsMenuItem.TabCount", tabs.size());
                    RecordUserAction.record("Tabs.CloseArchivedTabsMenuItem");
                }
            };

    private final NavigationProvider mNavigationProvider =
            new NavigationProvider() {
                @Override
                public void goBack() {
                    if (mTabActionState == TabActionState.CLOSABLE) {
                        hide(
                                ANIM_DURATION_MS,
                                /* animationFinishCallback= */ CallbackUtils.emptyRunnable());
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
                if (count == 0 && !ArchivedTabsDialogCoordinator.this.mIsOpeningLastTab) {
                    // Post task to allow the last tab to be unregistered.
                    PostTask.postTask(
                            TaskTraits.UI_DEFAULT,
                            () -> hide(ANIM_DURATION_MS, CallbackUtils.emptyRunnable()));
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
                    mIsOpeningLastTab = mArchivedTabModel.getCount() == 1;
                    Tab tab = mArchivedTabModel.getTabById(tabId);
                    mArchivedTabModelOrchestrator
                            .getTabArchiver()
                            .unarchiveAndRestoreTab(mRegularTabCreator, tab);

                    hide(
                            ANIM_DURATION_MS,
                            () -> {
                                // Post task to allow the tab to be unregistered.
                                PostTask.postTask(
                                        TaskTraits.UI_DEFAULT,
                                        () -> mOnTabSelectingListener.onTabSelecting(tab.getId()));
                                RecordUserAction.record("Tabs.RestoreSingleTab");
                            });
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
                    mDialogRecyclerView.removeOnScrollListener(mRecyclerScrollListener);
                    mSnackbarManager.popParentViewFromOverrideStack(mSnackbarOverrideToken);
                    // In case we were hidden by TabListEditor in some other case, force the
                    // animation to finish.
                    animateOut(
                            /* duration= */ 0,
                            /* animationFinishCallback= */ CallbackUtils.emptyRunnable());
                    mRootView.removeView(mDialogView);
                }

                @Override
                public void didHide() {
                    ArchivedTabsDialogCoordinator.this.hideInternal();
                }
            };

    private final RecyclerView.OnScrollListener mRecyclerScrollListener =
            new RecyclerView.OnScrollListener() {
                @Override
                public void onScrollStateChanged(RecyclerView recyclerView, int newState) {}

                @Override
                public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                    mShadowView.setVisibility(
                            recyclerView.canScrollVertically(1) ? View.VISIBLE : View.GONE);
                }
            };

    private final TabListItemSizeChangedObserver mTabListItemSizeChangedObserver =
            new TabListItemSizeChangedObserver() {
                @Override
                public void onSizeChanged(int spanCount, Size cardSize) {
                    if (mIphMessagePropertyModel == null) return;
                    mIphMessagePropertyModel.set(
                            ResizableMessageCardViewProperties.WIDTH,
                            spanCount == 4 ? cardSize.getWidth() * 2 : MATCH_PARENT);
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
    private final @NonNull UndoBarController mUndoBarController;
    private final @NonNull ActionConfirmationDialog mActionConfirmationDialog;
    private final @NonNull ViewGroup mDialogView;
    private final @NonNull ViewGroup mTabSwitcherView;
    private final @NonNull FadingShadowView mShadowView;
    private final @Nullable DesktopWindowStateProvider mDesktopWindowStateProvider;

    private TabListRecyclerView mDialogRecyclerView;
    private WeakReference<TabListRecyclerView> mTabSwitcherRecyclerView;
    private @TabActionState int mTabActionState = TabActionState.CLOSABLE;
    private TabListEditorCoordinator mTabListEditorCoordinator;
    private OnTabSelectingListener mOnTabSelectingListener;
    private PropertyModel mIphMessagePropertyModel;
    private int mSnackbarOverrideToken;
    private boolean mIsOpeningLastTab;
    private boolean mIsShowing;

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
     * @param desktopWindowStateProvider Provider to get desktop window and app header state.
     */
    public ArchivedTabsDialogCoordinator(
            @NonNull Context context,
            @NonNull ArchivedTabModelOrchestrator archivedTabModelOrchestrator,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull TabContentManager tabContentManager,
            @TabListMode int mode,
            @NonNull ViewGroup rootView,
            @NonNull ViewGroup tabSwitcherView,
            @NonNull SnackbarManager snackbarManager,
            @NonNull TabCreator regularTabCreator,
            @NonNull BackPressManager backPressManager,
            @NonNull TabArchiveSettings tabArchiveSettings,
            @NonNull ModalDialogManager modalDialogManager,
            @Nullable DesktopWindowStateProvider desktopWindowStateProvider) {
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
        mDesktopWindowStateProvider = desktopWindowStateProvider;

        mArchivedTabModelOrchestrator = archivedTabModelOrchestrator;
        mArchivedTabModel =
                mArchivedTabModelOrchestrator
                        .getTabModelSelector()
                        .getModel(/* incognito= */ false);
        mUndoBarController =
                new UndoBarController(
                        mContext,
                        mArchivedTabModelOrchestrator.getTabModelSelector(),
                        /* snackbarManageable= */ this,
                        /* dialogVisibilitySupplier= */ null);
        mTabSwitcherView = tabSwitcherView;

        // Inflate the dialog view and hook it up
        mDialogView =
                (ViewGroup)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.archived_tabs_dialog, mRootView, false);
        mDialogView
                .findViewById(R.id.close_all_tabs_button)
                .setOnClickListener(this::onCloseAllInactiveTabsButtonClicked);

        // Initialize the shadow for the "Close all inactive tabs" container.
        mShadowView = mDialogView.findViewById(R.id.close_all_tabs_button_container_shadow);
        mShadowView.init(
                mContext.getColor(R.color.toolbar_shadow_color), FadingShadow.POSITION_BOTTOM);

        // Initialize the confirmation dialog for when the last archived tab is removed.
        mActionConfirmationDialog = new ActionConfirmationDialog(mContext, mModalDialogManager);
    }

    /** Hides the dialog. */
    public void destroy() {
        if (mTabListEditorCoordinator != null
                && mTabListEditorCoordinator.getController().isVisible()) {
            hide(
                    /* animationDuration= */ 0,
                    /* animationFinishCallback= */ CallbackUtils.emptyRunnable());
        }
    }

    /**
     * Shows the dialog.
     *
     * @param onTabSelectingListener Allows a tab to be selected in the main tab switcher.
     */
    public void show(OnTabSelectingListener onTabSelectingListener) {
        if (mIsShowing) return;
        mIsShowing = true;
        mTabSwitcherRecyclerView =
                new WeakReference<>(mTabSwitcherView.findViewById(R.id.tab_list_recycler_view));
        mTabSwitcherRecyclerView.get().setBlockTouchInput(true);

        boolean tabListFirstShown = false;
        if (mTabListEditorCoordinator == null) {
            tabListFirstShown = true;
            createTabListEditorCoordinator();
        }

        mOnTabSelectingListener = onTabSelectingListener;
        mArchivedTabModel.getTabCountSupplier().addObserver(mTabCountObserver);
        mUndoBarController.initialize();

        TabListEditorController controller = mTabListEditorCoordinator.getController();
        controller.setLifecycleObserver(mTabListEditorLifecycleObserver);
        controller.show(TabModelUtils.convertTabListToListOfTabs(mArchivedTabModel), null);
        controller.setNavigationProvider(mNavigationProvider);
        mTabListEditorCoordinator.overrideContentDescriptions(
                R.string.accessibility_archived_tabs_dialog,
                R.string.accessibility_archived_tabs_dialog_back_button);

        mDialogRecyclerView = mDialogView.findViewById(R.id.tab_list_recycler_view);
        mDialogRecyclerView.addOnScrollListener(mRecyclerScrollListener);
        mShadowView.setVisibility(
                mDialogRecyclerView.canScrollVertically(1) ? View.VISIBLE : View.GONE);

        // Register the dialog to handle back press events.
        mBackPressManager.addHandler(controller, BackPressHandler.Type.ARCHIVED_TABS_DIALOG);

        FrameLayout snackbarContainer = mDialogView.findViewById(R.id.snackbar_container);
        mSnackbarOverrideToken = mSnackbarManager.pushParentViewToOverrideStack(snackbarContainer);
        // View is obscured by the TabListEditorCoordinator, so it needs to be brought to the front.
        mDialogView.findViewById(R.id.close_all_tabs_button_container).bringToFront();
        snackbarContainer.bringToFront();

        // Add the IPH to the TabListEditor.
        if (mTabArchiveSettings.shouldShowDialogIph()) {
            if (tabListFirstShown) {
                mTabListEditorCoordinator.registerItemType(
                        TabProperties.UiType.MESSAGE,
                        new LayoutViewBuilder(R.layout.resizable_tab_grid_message_card_item),
                        ResizableMessageCardViewBinder::bind);
            }
            mIphMessagePropertyModel =
                    ArchivedTabsIphMessageCardViewModel.create(
                            mContext, this::onIphReviewClicked, this::onIphDismissClicked);
            updateIphPropertyModel();
            mTabListEditorCoordinator.addSpecialListItem(
                    0, UiType.MESSAGE, mIphMessagePropertyModel);
            mTabListEditorCoordinator.addTabListItemSizeChangedObserver(
                    mTabListItemSizeChangedObserver);
            RecordUserAction.record("Tabs.ArchivedTabsDialogIphShown");
        }
        mTabArchiveSettings.addObserver(mTabArchiveSettingsObserver);

        moveToState(TabActionState.CLOSABLE);
        animateIn(ANIM_DURATION_MS);
    }

    private void animateIn(int duration) {
        mDialogView.setVisibility(View.INVISIBLE);
        mRootView.addView(mDialogView);
        mDialogView.post(
                () -> {
                    mDialogView.setTranslationX(mDialogView.getWidth());
                    mDialogView.setVisibility(View.VISIBLE);
                    // TODO(crbug.com/358430208): Use AnimatorSet here.
                    mDialogView
                            .animate()
                            .translationX(0f)
                            .setDuration(duration)
                            .setInterpolator(Interpolators.ACCELERATE_INTERPOLATOR)
                            .start();
                    mTabSwitcherView
                            .animate()
                            .translationX(-mTabSwitcherView.getWidth())
                            .setDuration(duration)
                            .setInterpolator(Interpolators.ACCELERATE_INTERPOLATOR)
                            .start();

                    RecordUserAction.record("Tabs.ArchivedTabsDialogShown");
                });
    }

    private void animateOut(int duration, Runnable animationFinishCallback) {
        mDialogRecyclerView.setBlockTouchInput(true);
        // TODO(crbug.com/358430208): Use AnimatorSet here.
        mDialogView
                .animate()
                .translationX(mDialogView.getWidth())
                .setDuration(duration)
                .setInterpolator(Interpolators.ACCELERATE_INTERPOLATOR)
                .start();
        mTabSwitcherView
                .animate()
                .translationX(0)
                .setDuration(duration)
                .setInterpolator(Interpolators.ACCELERATE_INTERPOLATOR)
                .setListener(
                        new AnimatorListenerAdapter() {
                            @Override
                            public void onAnimationEnd(@NonNull Animator animator) {
                                animationFinishCallback.run();
                                mDialogRecyclerView.setBlockTouchInput(false);
                            }
                        })
                .start();
    }

    /** Hides the dialog. */
    public void hide(int animationDuration, Runnable animationFinishCallback) {
        animateOut(
                animationDuration,
                () -> {
                    mTabListEditorCoordinator.removeTabListItemSizeChangedObserver(
                            mTabListItemSizeChangedObserver);
                    TabListEditorController controller = mTabListEditorCoordinator.getController();
                    controller.hide();
                    animationFinishCallback.run();
                });
    }

    void hideInternal() {
        TabListEditorController controller = mTabListEditorCoordinator.getController();
        controller.setLifecycleObserver(null);
        mBackPressManager.removeHandler(mTabListEditorCoordinator.getController());
        mTabArchiveSettings.removeObserver(mTabArchiveSettingsObserver);
        mArchivedTabModel.getTabCountSupplier().removeObserver(mTabCountObserver);
        mSnackbarOverrideToken = TokenHolder.INVALID_TOKEN;
        mIsShowing = false;
        mTabSwitcherRecyclerView.get().setBlockTouchInput(false);
        mTabSwitcherRecyclerView.clear();
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
                        /* parentView= */ mDialogView.findViewById(R.id.tab_list_editor_container),
                        mBrowserControlsStateProvider,
                        mArchivedTabModelOrchestrator
                                .getTabModelSelector()
                                .getTabModelFilterProvider()
                                .getCurrentTabModelFilterSupplier(),
                        mTabContentManager,
                        /* clientTabListRecyclerViewPositionSetter= */ null,
                        mMode,
                        /* displayGroups= */ true,
                        mSnackbarManager,
                        /* bottomSheetController= */ null,
                        TabProperties.TabActionState.CLOSABLE,
                        mGridCardOnCLickListenerProvider,
                        mModalDialogManager,
                        mDesktopWindowStateProvider,
                        /* edgeToEdgeSupplier= */ null);
    }

    @VisibleForTesting
    void onCloseAllInactiveTabsButtonClicked(View view) {
        int tabCount = mArchivedTabModel.getCount();
        showCloseAllArchivedTabsConfirmation(
                tabCount,
                () -> {
                    RecordHistogram.recordCount1000Histogram(
                            "Tabs.CloseAllArchivedTabs.TabCount", tabCount);
                    RecordUserAction.record("Tabs.CloseAllArchivedTabsMenuItem");
                });
    }

    /**
     * Shows a confirmation dialog when the close operation cannot be undone.
     *
     * @param onConfirmRunnable A runnable which is run if the dialog is confirmed.
     */
    private void showCloseAllArchivedTabsConfirmation(int tabCount, Runnable onConfirmRunnable) {
        Function<Resources, String> titleResolver =
                (res) -> {
                    return res.getQuantityString(
                            R.plurals.archive_dialog_close_all_inactive_tabs_confirmation_title,
                            tabCount,
                            tabCount);
                };
        Function<Resources, String> descriptionResolver =
                (res) -> {
                    return res.getString(
                            R.string
                                    .archive_dialog_close_all_inactive_tabs_confirmation_description);
                };
        mActionConfirmationDialog.show(
                titleResolver,
                descriptionResolver,
                R.string.archive_dialog_close_all_inactive_tabs_confirmation,
                R.string.cancel,
                /* supportStopShowing= */ false,
                (buttonClickResult, stopShowing) -> {
                    if (buttonClickResult == ButtonClickResult.POSITIVE) {
                        mArchivedTabModel.closeTabs(
                                TabClosureParams.closeTabs(
                                                TabModelUtils.convertTabListToListOfTabs(
                                                        mArchivedTabModel))
                                        .allowUndo(false)
                                        .build());
                        onConfirmRunnable.run();
                    }
                });
    }

    private void restoreArchivedTabs(List<Tab> tabs) {
        for (Tab tab : tabs) {
            mArchivedTabModelOrchestrator
                    .getTabArchiver()
                    .unarchiveAndRestoreTab(mRegularTabCreator, tab);
        }
    }

    private void onIphReviewClicked() {
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(mContext, TabArchiveSettingsFragment.class);
        RecordUserAction.record("Tabs.ArchivedTabsDialogIphClicked");
    }

    private void onIphDismissClicked(@MessageType int messageType) {
        mTabArchiveSettings.markDialogIphDismissed();
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
        // The auto-delete section is blank when the feature param is disabled.
        String autoDeleteTitle =
                mTabArchiveSettings.isAutoDeleteEnabled()
                        ? mContext.getString(
                                R.string.archived_tab_iph_card_subtitle_autodelete_section,
                                autoDeleteTimeDeletaDays)
                        : "";
        String description =
                mContext.getString(
                        R.string.archived_tab_iph_card_subtitle,
                        archiveTimeDeltaDays,
                        autoDeleteTitle,
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

    // SnackbarManageable implementation.

    @Override
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManager;
    }

    // Testing-specific methods.

    void setTabListEditorCoordinatorForTesting(TabListEditorCoordinator tabListEditorCoordinator) {
        mTabListEditorCoordinator = tabListEditorCoordinator;
    }

    ArchiveDelegate getArchiveDelegateForTesting() {
        return mArchiveDelegate;
    }

    TabListEditorCoordinator.LifecycleObserver getTabListEditorLifecycleObserver() {
        return mTabListEditorLifecycleObserver;
    }

    View getViewForTesting() {
        return mDialogView;
    }
}
