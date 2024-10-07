// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.ContextCompat;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.Token;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_group_sync.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tab_ui.TabUiThemeUtils;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupTitleUtils;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ShowMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorController;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabGroupColorChangeActionType;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabListEditorOpenMetricGroups;
import org.chromium.chrome.browser.tinker_tank.TinkerTankDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider.AppHeaderObserver;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.messaging.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.messaging.MessagingBackendService;
import org.chromium.components.tab_group_sync.messaging.MessagingBackendService.PersistentMessageObserver;
import org.chromium.components.tab_group_sync.messaging.PersistentMessage;
import org.chromium.components.tab_group_sync.messaging.UserAction;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.EmptyTextWatcher;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;

/**
 * A mediator for the TabGridDialog component, responsible for communicating with the components'
 * coordinator as well as managing the business logic for dialog show/hide.
 */
public class TabGridDialogMediator
        implements SnackbarManager.SnackbarController,
                TabGridDialogView.VisibilityListener,
                TabGridItemTouchHelperCallback.OnLongPressTabItemEventListener,
                AppHeaderObserver {

    /** Defines an interface for a {@link TabGridDialogMediator} to control dialog. */
    interface DialogController extends BackPressHandler {
        /**
         * Handles a reset event originated from {@link TabGridDialogMediator} and {@link
         * TabSwitcherPaneMediator}.
         *
         * @param tabs List of Tabs to reset.
         */
        void resetWithListOfTabs(@Nullable List<Tab> tabs);

        /**
         * Hide the TabGridDialog
         * @param showAnimation Whether to show an animation when hiding the dialog.
         */
        void hideDialog(boolean showAnimation);

        /** Prepare the TabGridDialog before show. */
        void prepareDialog();

        /** Cleanup post hiding dialog. */
        void postHiding();

        /**
         * @return Whether or not the TabGridDialog consumed the event.
         */
        boolean handleBackPressed();

        /**
         * @return Whether the TabGridDialog is visible.
         */
        boolean isVisible();

        /** A supplier that returns if the dialog is currently showing or animating. */
        ObservableSupplier<Boolean> getShowingOrAnimationSupplier();

        /**
         * Adds a message card to the UI.
         *
         * @param position The index to insert the card at.
         * @param messageCardModel The {@link PropertyModel} using {@link MessageCardViewProperties}
         *     keys.
         */
        void addMessageCardItem(int position, PropertyModel messageCardModel);

        /**
         * Removes a message card from the UI.
         *
         * @param messageType The type of message to remove.
         */
        void removeMessageCardItem(@MessageType int messageType);

        /**
         * Checks whether a message card exists.
         *
         * @param messageType The type of message to look for.
         */
        boolean messageCardExists(@MessageType int messageType);
    }

    /**
     * Defines an interface for a {@link TabGridDialogMediator} to get the source {@link View} in
     * order to prepare show/hide animation.
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

    private final ValueChangedCallback<TabModelFilter> mOnTabModelFilterChanged =
            new ValueChangedCallback<>(this::onTabModelFilterChanged);
    private final Callback<Integer> mOnGroupSharedStateChanged = this::onGroupSharedStateChanged;
    private final Callback<String> mOnCollaborationIdChanged = this::onCollaborationIdChanged;
    private final Activity mActivity;
    private final DialogController mDialogController;
    private final PropertyModel mModel;
    private final ObservableSupplier<TabModelFilter> mCurrentTabModelFilterSupplier;
    private final TabCreatorManager mTabCreatorManager;
    private final @Nullable TabSwitcherResetHandler mTabSwitcherResetHandler;
    private final Supplier<RecyclerViewPosition> mRecyclerViewPositionSupplier;
    private final AnimationSourceViewProvider mAnimationSourceViewProvider;
    private final DialogHandler mTabGridDialogHandler;
    private final @Nullable SnackbarManager mSnackbarManager;
    private final @Nullable SharedImageTilesCoordinator mSharedImageTilesCoordinator;
    private final DataSharingTabManager mDataSharingTabManager;
    private final String mComponentName;
    private final Runnable mShowColorPickerPopupRunnable;
    private final ActionConfirmationManager mActionConfirmationManager;
    private final ModalDialogManager mModalDialogManager;
    private final Profile mOriginalProfile;
    private final @Nullable TabGroupSyncService mTabGroupSyncService;
    private final @Nullable DataSharingService mDataSharingService;
    private final @Nullable TransitiveSharedGroupObserver mTransitiveSharedGroupObserver;
    private final @Nullable MessagingBackendService mMessagingBackendService;
    private final @Nullable MessagingBackendService.PersistentMessageObserver
            mPersistentMessageObserver;
    private final TabModelObserver mTabModelObserver;
    private final TabGroupModelFilterObserver mTabGroupModelFilterObserver;
    private final Runnable mScrimClickRunnable;
    private final @Nullable DesktopWindowStateProvider mDesktopWindowStateProvider;

    private int mCurrentTabId = Tab.INVALID_TAB_ID;
    private TabGridDialogMenuCoordinator mTabGridDialogMenuCoordinator;
    private TabGroupTitleEditor mTabGroupTitleEditor;
    private Supplier<TabListEditorController> mTabListEditorControllerSupplier;
    private boolean mTabListEditorSetup;
    private KeyboardVisibilityDelegate.KeyboardVisibilityListener mKeyboardVisibilityListener;
    private boolean mIsUpdatingTitle;
    private String mCurrentGroupModifiedTitle;
    private @Nullable CollaborationActivityMessageCardViewModel mCollaborationActivityPropertyModel;

    TabGridDialogMediator(
            Activity activity,
            DialogController dialogController,
            PropertyModel model,
            ObservableSupplier<TabModelFilter> currentTabModelFilterSupplier,
            TabCreatorManager tabCreatorManager,
            @Nullable TabSwitcherResetHandler tabSwitcherResetHandler,
            Supplier<RecyclerViewPosition> recyclerViewPositionSupplier,
            AnimationSourceViewProvider animationSourceViewProvider,
            @Nullable SnackbarManager snackbarManager,
            @Nullable SharedImageTilesCoordinator sharedImageTilesCoordinator,
            @NonNull DataSharingTabManager dataSharingTabManager,
            String componentName,
            Runnable showColorPickerPopupRunnable,
            @Nullable ActionConfirmationManager actionConfirmationManager,
            @Nullable ModalDialogManager modalDialogManager,
            @Nullable DesktopWindowStateProvider desktopWindowStateProvider) {
        mActivity = activity;
        mDialogController = dialogController;
        mModel = model;
        mCurrentTabModelFilterSupplier = currentTabModelFilterSupplier;
        mTabCreatorManager = tabCreatorManager;
        mTabSwitcherResetHandler = tabSwitcherResetHandler;
        mRecyclerViewPositionSupplier = recyclerViewPositionSupplier;
        mAnimationSourceViewProvider = animationSourceViewProvider;
        mTabGridDialogHandler = new DialogHandler();
        mSnackbarManager = snackbarManager;
        mSharedImageTilesCoordinator = sharedImageTilesCoordinator;
        mDataSharingTabManager = dataSharingTabManager;
        mComponentName = componentName;
        mShowColorPickerPopupRunnable = showColorPickerPopupRunnable;
        mActionConfirmationManager = actionConfirmationManager;
        mModalDialogManager = modalDialogManager;
        mOriginalProfile =
                mCurrentTabModelFilterSupplier
                        .get()
                        .getTabModel()
                        .getProfile()
                        .getOriginalProfile();
        mDesktopWindowStateProvider = desktopWindowStateProvider;
        if (TabGroupSyncFeatures.isTabGroupSyncEnabled(mOriginalProfile)) {
            mTabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(mOriginalProfile);
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING)) {
                mDataSharingService = DataSharingServiceFactory.getForProfile(mOriginalProfile);
                mTransitiveSharedGroupObserver =
                        new TransitiveSharedGroupObserver(
                                mTabGroupSyncService, mDataSharingService);
                mTransitiveSharedGroupObserver
                        .getGroupSharedStateSupplier()
                        .addObserver(mOnGroupSharedStateChanged);
                mTransitiveSharedGroupObserver
                        .getCollaborationIdSupplier()
                        .addObserver(mOnCollaborationIdChanged);
                mMessagingBackendService =
                        MessagingBackendServiceFactory.getForProfile(mOriginalProfile);
                mPersistentMessageObserver =
                        new PersistentMessageObserver() {
                            @Override
                            public void displayPersistentMessage(PersistentMessage message) {
                                updateOnMatch(message);
                            }

                            @Override
                            public void hidePersistentMessage(PersistentMessage message) {
                                updateOnMatch(message);
                            }
                        };
                mMessagingBackendService.addPersistentMessageObserver(mPersistentMessageObserver);
            } else {
                mDataSharingService = null;
                mTransitiveSharedGroupObserver = null;
                mMessagingBackendService = null;
                mPersistentMessageObserver = null;
            }
        } else {
            mTabGroupSyncService = null;
            mDataSharingService = null;
            mTransitiveSharedGroupObserver = null;
            mMessagingBackendService = null;
            mPersistentMessageObserver = null;
        }

        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        if (!isVisible()) return;

                        TabModelFilter filter = mCurrentTabModelFilterSupplier.get();
                        if (filter == null || !filter.isTabModelRestored()) {
                            return;
                        }

                        // For tab group sync a tab can be added without needing to close the tab
                        // grid dialog. The UI updates are driven from TabListMediator's
                        // TabGroupModelFilterObserver's didMergeTabToGroup implementation.
                        if (type == TabLaunchType.FROM_SYNC_BACKGROUND) {
                            return;
                        }
                        hideDialog(false);
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        // Allow this to update when invisible so the undo bar is handled correctly.
                        updateDialog();
                        updateGridTabSwitcher();
                        dismissSingleTabSnackbar(tab.getId());
                    }

                    @Override
                    public void didSelectTab(Tab tab, int type, int lastId) {
                        if (!isVisible()) return;

                        // When this grid dialog is opened via the tab switcher there is a
                        // `mTabSwitcherResetHandler`.
                        boolean isTabSwitcherContext = mTabSwitcherResetHandler != null;
                        if (type == TabSelectionType.FROM_USER && !isTabSwitcherContext) {
                            // Hide the dialog from the strip context only.
                            hideDialog(false);
                        } else if (getRelatedTabs(mCurrentTabId).contains(tab)) {
                            mCurrentTabId = tab.getId();
                        }
                    }

                    @Override
                    public void willCloseTab(Tab tab, boolean didCloseAlone) {
                        if (!isVisible()) return;

                        // Ignore updates to tabs in other tab groups.
                        boolean closingTabIsCurrentTab = tab.getId() == mCurrentTabId;
                        if (!closingTabIsCurrentTab
                                && !currentTabRootIdMatchesRootId(tab.getRootId())) {
                            return;
                        }

                        List<Tab> relatedTabs = getRelatedTabs(tab.getId());
                        // If the group is empty, update the animation and hide the dialog.
                        if (relatedTabs.size() == 0) {
                            hideDialog(false);
                            return;
                        }
                        // If current tab is closed and tab group is not empty, hand over ID of the
                        // next tab in the group to mCurrentTabId.
                        if (closingTabIsCurrentTab) {
                            mCurrentTabId = relatedTabs.get(0).getId();
                        }
                        updateDialog();
                        updateGridTabSwitcher();
                    }

                    @Override
                    public void tabPendingClosure(Tab tab) {
                        if (!isVisible()) return;

                        // TODO(b/338447134): This shouldn't show a snackbar if the tab isn't in
                        // this group. However, background closures are currently not-undoable so
                        // this is fine for now...
                        showSingleTabClosureSnackbar(tab);
                    }

                    @Override
                    public void multipleTabsPendingClosure(
                            List<Tab> closedTabs, boolean isAllTabs) {
                        if (!isVisible() || mSnackbarManager == null) return;

                        // TODO(b/338447134): This shouldn't show a snackbar if the tabs aren't in
                        // this group. However, background closures are currently not-undoable so
                        // this is fine for now...
                        if (closedTabs.size() == 1) {
                            showSingleTabClosureSnackbar(closedTabs.get(0));
                            return;
                        }

                        assert !isAllTabs;
                        String content =
                                String.format(Locale.getDefault(), "%d", closedTabs.size());
                        mSnackbarManager.showSnackbar(
                                Snackbar.make(
                                                content,
                                                TabGridDialogMediator.this,
                                                Snackbar.TYPE_ACTION,
                                                Snackbar.UMA_TAB_CLOSE_MULTIPLE_UNDO)
                                        .setTemplateText(
                                                mActivity.getString(
                                                        R.string.undo_bar_close_all_message))
                                        .setAction(mActivity.getString(R.string.undo), closedTabs));
                    }

                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        // Allow this to update while invisible so the snackbar updates correctly.
                        dismissSingleTabSnackbar(tab.getId());
                    }

                    @Override
                    public void onFinishingMultipleTabClosure(List<Tab> tabs, boolean canRestore) {
                        // Allow this to update while invisible so the snackbar updates correctly.
                        if (tabs.size() == 1) {
                            dismissSingleTabSnackbar(tabs.get(0).getId());
                            return;
                        }
                        dismissMultipleTabSnackbar(tabs);
                    }

                    @Override
                    public void allTabsClosureCommitted(boolean isIncognito) {
                        // Allow this to update while invisible so the snackbar updates correctly.
                        dismissAllSnackbars();
                    }

                    private void showSingleTabClosureSnackbar(Tab tab) {
                        if (mSnackbarManager == null) return;
                        mSnackbarManager.showSnackbar(
                                Snackbar.make(
                                                tab.getTitle(),
                                                TabGridDialogMediator.this,
                                                Snackbar.TYPE_ACTION,
                                                Snackbar.UMA_TAB_CLOSE_UNDO)
                                        .setTemplateText(
                                                mActivity.getString(
                                                        R.string.undo_bar_close_message))
                                        .setAction(
                                                mActivity.getString(R.string.undo), tab.getId()));
                    }

                    private void dismissMultipleTabSnackbar(List<Tab> tabs) {
                        if (mSnackbarManager == null) return;
                        PostTask.postTask(
                                TaskTraits.UI_DEFAULT,
                                () -> {
                                    mSnackbarManager.dismissSnackbars(
                                            TabGridDialogMediator.this, tabs);
                                });
                    }

                    private void dismissSingleTabSnackbar(int tabId) {
                        if (mSnackbarManager == null) return;
                        PostTask.postTask(
                                TaskTraits.UI_DEFAULT,
                                () -> {
                                    mSnackbarManager.dismissSnackbars(
                                            TabGridDialogMediator.this, tabId);
                                });
                    }

                    private void dismissAllSnackbars() {
                        if (mSnackbarManager == null) return;
                        PostTask.postTask(
                                TaskTraits.UI_DEFAULT,
                                () -> {
                                    mSnackbarManager.dismissSnackbars(TabGridDialogMediator.this);
                                });
                    }
                };

        mTabGroupModelFilterObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void didChangeTabGroupTitle(int rootId, String newTitle) {
                        if (currentTabRootIdMatchesRootId(rootId)
                                && !Objects.equals(
                                        mModel.get(TabGridDialogProperties.HEADER_TITLE),
                                        newTitle)) {
                            int tabsCount = getRelatedTabs(mCurrentTabId).size();
                            updateTitle(tabsCount);
                        }
                    }

                    @Override
                    public void didChangeTabGroupColor(int rootId, @TabGroupColorId int newColor) {
                        if (!ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) return;

                        if (currentTabRootIdMatchesRootId(rootId)) {
                            mModel.set(TabGridDialogProperties.TAB_GROUP_COLOR_ID, newColor);
                        }
                    }
                };

        mOnTabModelFilterChanged.onResult(
                mCurrentTabModelFilterSupplier.addObserver(mOnTabModelFilterChanged));

        // Setup ScrimView click Runnable.
        mScrimClickRunnable =
                () -> {
                    hideDialog(true);
                    RecordUserAction.record("TabGridDialog.Exit");
                };
        mModel.set(TabGridDialogProperties.VISIBILITY_LISTENER, this);
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(
                TabGridDialogProperties.UNGROUP_BAR_STATUS,
                TabGridDialogView.UngroupBarStatus.HIDE);
        if (mDesktopWindowStateProvider != null) {
            mDesktopWindowStateProvider.addObserver(this);
            if (mDesktopWindowStateProvider.getAppHeaderState() != null) {
                onAppHeaderStateChanged(mDesktopWindowStateProvider.getAppHeaderState());
            }
        }
    }

    public void initWithNative(
            @NonNull Supplier<TabListEditorController> tabListEditorControllerSupplier,
            TabGroupTitleEditor tabGroupTitleEditor) {
        mTabListEditorControllerSupplier = tabListEditorControllerSupplier;
        mTabGroupTitleEditor = tabGroupTitleEditor;

        assert mCurrentTabModelFilterSupplier.get() instanceof TabGroupModelFilter;

        setupToolbarClickHandlers();
        setupToolbarEditText();

        mModel.set(TabGridDialogProperties.MENU_CLICK_LISTENER, getMenuButtonClickListener());

        // TODO(b/325082444): Only a subset should be visible at a time. Only set the listeners that
        // can be seen and used.
        mModel.set(TabGridDialogProperties.SHARE_BUTTON_CLICK_LISTENER, getShareBarClickListener());
        mModel.set(
                TabGridDialogProperties.SHARE_IMAGE_TILES_CLICK_LISTENER,
                getShareBarClickListener());
    }

    void hideDialog(boolean showAnimation) {
        if (!mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE)) {
            if (!showAnimation) {
                // Forcibly finish any pending animations.
                mModel.set(TabGridDialogProperties.FORCE_ANIMATION_TO_FINISH, true);
                mModel.set(TabGridDialogProperties.FORCE_ANIMATION_TO_FINISH, false);
            }
            return;
        }

        if (mModel.get(TabGridDialogProperties.IS_SHARE_SHEET_VISIBLE)) {
            // TODO(b/333776074): Close the ShareSheet without causing a crash at accessibility
            // important restoration.
        }

        if (mSnackbarManager != null) {
            mSnackbarManager.dismissSnackbars(TabGridDialogMediator.this);
        }

        // Save the title first so that the animation has the correct title.
        saveCurrentGroupModifiedTitle();
        mModel.set(TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED, false);

        if (!showAnimation) {
            mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, null);
        } else {
            if (mAnimationSourceViewProvider != null && mCurrentTabId != Tab.INVALID_TAB_ID) {
                mModel.set(
                        TabGridDialogProperties.ANIMATION_SOURCE_VIEW,
                        mAnimationSourceViewProvider.getAnimationSourceViewForTab(mCurrentTabId));
            }
        }
        if (mTabListEditorControllerSupplier != null
                && mTabListEditorControllerSupplier.hasValue()) {
            mTabListEditorControllerSupplier.get().hide();
        }
        // Hide view first. Listener will reset tabs on #finishedHiding.
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, false);
    }

    /**
     * @return a boolean indicating if the result of handling the backpress was successful.
     */
    public boolean handleBackPress() {
        if (mTabListEditorControllerSupplier != null
                && mTabListEditorControllerSupplier.hasValue()
                && mTabListEditorControllerSupplier.get().isVisible()) {
            mTabListEditorControllerSupplier.get().hide();
            return !mTabListEditorControllerSupplier.get().isVisible();
        }
        hideDialog(true);
        RecordUserAction.record("TabGridDialog.Exit");
        return !isVisible();
    }

    // @TabGridDialogView.VisibilityListener
    @Override
    public void finishedHidingDialogView() {
        mDialogController.resetWithListOfTabs(null);
        mDialogController.postHiding();
        // Purge the bitmap reference in the animation.
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, null);
        mModel.set(TabGridDialogProperties.BINDING_TOKEN, null);
    }

    void onReset(@Nullable List<Tab> tabs) {
        TabModelFilter filter = mCurrentTabModelFilterSupplier.get();
        if (tabs == null) {
            mCurrentTabId = Tab.INVALID_TAB_ID;
        } else {
            mCurrentTabId = filter.getTabAt(filter.indexOf(tabs.get(0))).getId();
        }

        updateTabGroupId();
        if (mCurrentTabId != Tab.INVALID_TAB_ID) {
            if (mAnimationSourceViewProvider != null) {
                mModel.set(
                        TabGridDialogProperties.ANIMATION_SOURCE_VIEW,
                        mAnimationSourceViewProvider.getAnimationSourceViewForTab(mCurrentTabId));
            } else {
                mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, null);
            }
            updateDialog();
            mModel.set(TabGridDialogProperties.SCRIMVIEW_CLICK_RUNNABLE, mScrimClickRunnable);
            updateDialogScrollPosition();

            // Do this after the dialog is updated so most attributes are not set with stale values
            // when the binding token is set.
            mModel.set(TabGridDialogProperties.BINDING_TOKEN, hashCode());

            mDialogController.prepareDialog();
            mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);
        } else if (isVisible()) {
            mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, false);
        }
    }

    /** Destroy any members that needs clean up. */
    public void destroy() {
        removeTabModelFilterObserver(mCurrentTabModelFilterSupplier.get());
        mCurrentTabModelFilterSupplier.removeObserver(mOnTabModelFilterChanged);
        KeyboardVisibilityDelegate.getInstance()
                .removeKeyboardVisibilityListener(mKeyboardVisibilityListener);
        if (mTransitiveSharedGroupObserver != null) {
            mTransitiveSharedGroupObserver
                    .getGroupSharedStateSupplier()
                    .removeObserver(mOnGroupSharedStateChanged);
            mTransitiveSharedGroupObserver
                    .getCollaborationIdSupplier()
                    .removeObserver(mOnCollaborationIdChanged);
            mTransitiveSharedGroupObserver.destroy();
        }
        if (mDesktopWindowStateProvider != null) {
            mDesktopWindowStateProvider.removeObserver(this);
        }
        if (mMessagingBackendService != null && mPersistentMessageObserver != null) {
            mMessagingBackendService.removePersistentMessageObserver(mPersistentMessageObserver);
        }
    }

    boolean isVisible() {
        return mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE);
    }

    void setSelectedTabGroupColor(int selectedColor) {
        mModel.set(TabGridDialogProperties.TAB_GROUP_COLOR_ID, selectedColor);

        TabGroupModelFilter filter = (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();
        Tab currentTab = filter.getTabModel().getTabById(mCurrentTabId);

        if (currentTab != null) {
            filter.setTabGroupColor(currentTab.getRootId(), selectedColor);
        }
    }

    private void updateGridTabSwitcher() {
        if (!isVisible() || mTabSwitcherResetHandler == null) return;
        mTabSwitcherResetHandler.resetWithTabList(mCurrentTabModelFilterSupplier.get(), false);
    }

    private void updateDialog() {
        final int tabsCount = getRelatedTabs(mCurrentTabId).size();
        if (tabsCount == 0) {
            hideDialog(true);
            return;
        }

        Resources res = mActivity.getResources();
        // Change the ungroup bar text if the tab being ungrouped is the last tab in the group.
        final @StringRes int ungroupBarTextId =
                tabsCount == 1
                        ? R.string.remove_last_tab_action
                        : R.string.tab_grid_dialog_remove_from_group;
        mModel.set(
                TabGridDialogProperties.DIALOG_UNGROUP_BAR_TEXT, res.getString(ungroupBarTextId));

        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
            TabGroupModelFilter filter = (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();
            Tab currentTab = filter.getTabModel().getTabById(mCurrentTabId);
            final @TabGroupColorId int color =
                    filter.getTabGroupColorWithFallback(currentTab.getRootId());
            mModel.set(TabGridDialogProperties.TAB_GROUP_COLOR_ID, color);
        }
        updateTitle(tabsCount);
    }

    private void updateTitle(int tabsCount) {
        Resources res = mActivity.getResources();

        TabGroupModelFilter filter = (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();
        Tab currentTab = filter.getTabModel().getTabById(mCurrentTabId);
        if (mTabGroupTitleEditor != null) {
            String storedTitle = mTabGroupTitleEditor.getTabGroupTitle(currentTab.getRootId());
            if (storedTitle != null && filter.isTabInTabGroup(currentTab)) {
                mModel.set(
                        TabGridDialogProperties.COLLAPSE_BUTTON_CONTENT_DESCRIPTION,
                        res.getQuantityString(
                                R.plurals.accessibility_dialog_back_button_with_group_name,
                                tabsCount,
                                storedTitle,
                                tabsCount));
                mModel.set(TabGridDialogProperties.HEADER_TITLE, storedTitle);
                return;
            }
        }

        mModel.set(
                TabGridDialogProperties.COLLAPSE_BUTTON_CONTENT_DESCRIPTION,
                res.getQuantityString(
                        R.plurals.accessibility_dialog_back_button, tabsCount, tabsCount));
        mModel.set(
                TabGridDialogProperties.HEADER_TITLE,
                TabGroupTitleUtils.getDefaultTitle(mActivity, tabsCount));
    }

    private void updateColorProperties(Context context, boolean isIncognito) {
        @ColorInt
        int dialogBackgroundColor =
                TabUiThemeProvider.getTabGridDialogBackgroundColor(context, isIncognito);
        ColorStateList tintList =
                isIncognito
                        ? AppCompatResources.getColorStateList(
                                mActivity, R.color.default_icon_color_light_tint_list)
                        : AppCompatResources.getColorStateList(
                                mActivity, R.color.default_icon_color_tint_list);
        @ColorInt
        int ungroupBarBackgroundColor =
                TabUiThemeProvider.getTabGridDialogUngroupBarBackgroundColor(context, isIncognito);
        @ColorInt
        int ungroupBarHoveredBackgroundColor =
                TabUiThemeProvider.getTabGridDialogUngroupBarHoveredBackgroundColor(
                        context, isIncognito);
        @ColorInt
        int ungroupBarTextColor =
                TabUiThemeProvider.getTabGridDialogUngroupBarTextColor(context, isIncognito);
        @ColorInt
        int ungroupBarHoveredTextColor =
                TabUiThemeProvider.getTabGridDialogUngroupBarHoveredTextColor(context, isIncognito);
        @ColorInt
        int hairlineColor =
                isIncognito
                        ? ContextCompat.getColor(context, R.color.divider_line_bg_color_light)
                        : SemanticColorUtils.getDividerLineBgColor(context);

        mModel.set(TabGridDialogProperties.DIALOG_BACKGROUND_COLOR, dialogBackgroundColor);
        mModel.set(TabGridDialogProperties.HAIRLINE_COLOR, hairlineColor);
        mModel.set(TabGridDialogProperties.TINT, tintList);
        mModel.set(
                TabGridDialogProperties.DIALOG_UNGROUP_BAR_BACKGROUND_COLOR,
                ungroupBarBackgroundColor);
        mModel.set(
                TabGridDialogProperties.DIALOG_UNGROUP_BAR_HOVERED_BACKGROUND_COLOR,
                ungroupBarHoveredBackgroundColor);
        mModel.set(TabGridDialogProperties.DIALOG_UNGROUP_BAR_TEXT_COLOR, ungroupBarTextColor);
        mModel.set(
                TabGridDialogProperties.DIALOG_UNGROUP_BAR_HOVERED_TEXT_COLOR,
                ungroupBarHoveredTextColor);
        mModel.set(TabGridDialogProperties.IS_INCOGNITO, isIncognito);
        if (TabUiFeatureUtilities.shouldUseListMode()) {
            int animationBackgroundColor =
                    TabUiThemeUtils.getCardViewBackgroundColor(
                            mActivity, isIncognito, /* isSelected= */ false);
            mModel.set(
                    TabGridDialogProperties.ANIMATION_BACKGROUND_COLOR, animationBackgroundColor);
        }
    }

    private int getIdForTab(@Nullable Tab tab) {
        return tab == null ? Tab.INVALID_TAB_ID : tab.getId();
    }

    private void updateDialogScrollPosition() {
        // If current selected tab is not within this dialog, always scroll to the top.
        Tab currentTab = TabModelUtils.getCurrentTab(mCurrentTabModelFilterSupplier.get());
        if (mCurrentTabId != getIdForTab(currentTab)) {
            mModel.set(TabGridDialogProperties.INITIAL_SCROLL_INDEX, 0);
            return;
        }
        List<Tab> relatedTabs = getRelatedTabs(mCurrentTabId);
        int initialPosition = relatedTabs.indexOf(currentTab);
        mModel.set(TabGridDialogProperties.INITIAL_SCROLL_INDEX, initialPosition);
    }

    private void setupToolbarClickHandlers() {
        mModel.set(
                TabGridDialogProperties.COLLAPSE_CLICK_LISTENER, getCollapseButtonClickListener());
        mModel.set(TabGridDialogProperties.ADD_CLICK_LISTENER, getAddButtonClickListener());
    }

    private void configureTabListEditorMenu() {
        assert mTabListEditorControllerSupplier != null;

        if (mTabListEditorSetup) {
            return;
        }
        mTabListEditorSetup = true;

        List<TabListEditorAction> actions = new ArrayList<>();
        actions.add(
                TabListEditorSelectionAction.createAction(
                        mActivity, ShowMode.MENU_ONLY, ButtonType.ICON_AND_TEXT, IconPosition.END));
        actions.add(
                TabListEditorCloseAction.createAction(
                        mActivity,
                        ShowMode.MENU_ONLY,
                        ButtonType.ICON_AND_TEXT,
                        IconPosition.START,
                        mActionConfirmationManager));
        actions.add(
                TabListEditorUngroupAction.createAction(
                        mActivity,
                        ShowMode.MENU_ONLY,
                        ButtonType.ICON_AND_TEXT,
                        IconPosition.START,
                        mActionConfirmationManager));
        actions.add(
                TabListEditorBookmarkAction.createAction(
                        mActivity,
                        ShowMode.MENU_ONLY,
                        ButtonType.ICON_AND_TEXT,
                        IconPosition.START));
        if (TinkerTankDelegate.isEnabled()) {
            actions.add(
                    TabListEditorTinkerTankAction.createAction(
                            mActivity,
                            ShowMode.MENU_ONLY,
                            ButtonType.ICON_AND_TEXT,
                            IconPosition.START));
        }
        actions.add(
                TabListEditorShareAction.createAction(
                        mActivity,
                        ShowMode.MENU_ONLY,
                        ButtonType.ICON_AND_TEXT,
                        IconPosition.START));
        mTabListEditorControllerSupplier.get().configureToolbarWithMenuItems(actions);
    }

    private void setupToolbarEditText() {
        mKeyboardVisibilityListener =
                isShowing -> {
                    mModel.set(TabGridDialogProperties.TITLE_CURSOR_VISIBILITY, isShowing);
                    if (!isShowing) {
                        mModel.set(TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED, false);
                        saveCurrentGroupModifiedTitle();
                    }
                };
        KeyboardVisibilityDelegate.getInstance()
                .addKeyboardVisibilityListener(mKeyboardVisibilityListener);

        TextWatcher textWatcher =
                new EmptyTextWatcher() {
                    @Override
                    public void afterTextChanged(Editable s) {
                        if (!mIsUpdatingTitle) return;
                        mCurrentGroupModifiedTitle = s.toString();
                    }
                };
        mModel.set(TabGridDialogProperties.TITLE_TEXT_WATCHER, textWatcher);

        View.OnFocusChangeListener onFocusChangeListener =
                (v, hasFocus) -> {
                    mIsUpdatingTitle = hasFocus;
                    mModel.set(TabGridDialogProperties.IS_KEYBOARD_VISIBLE, hasFocus);
                    mModel.set(TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED, hasFocus);
                };
        mModel.set(TabGridDialogProperties.TITLE_TEXT_ON_FOCUS_LISTENER, onFocusChangeListener);
    }

    private View.OnClickListener getCollapseButtonClickListener() {
        return view -> {
            hideDialog(true);
            RecordUserAction.record("TabGridDialog.Exit");
        };
    }

    private View.OnClickListener getAddButtonClickListener() {
        return view -> {
            // Get the current Tab first since hideDialog causes mCurrentTabId to be
            // Tab.INVALID_TAB_ID.
            TabModelFilter filter = mCurrentTabModelFilterSupplier.get();
            Tab currentTab = filter.getTabModel().getTabById(mCurrentTabId);
            hideDialog(false);

            // Reset the list of tabs so the new tab doesn't appear on the dialog before the
            // animation.
            if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
                mDialogController.resetWithListOfTabs(null);
            }

            if (currentTab == null) {
                mTabCreatorManager.getTabCreator(filter.isIncognito()).launchNtp();
                return;
            }

            TabUiUtils.openNtpInGroup(
                    (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get(),
                    mTabCreatorManager.getTabCreator(filter.isIncognito()),
                    currentTab.getId(),
                    TabLaunchType.FROM_TAB_GROUP_UI);
            RecordUserAction.record("MobileNewTabOpened." + mComponentName);
        };
    }

    @VisibleForTesting
    public void onToolbarMenuItemClick(int menuId, int tabId, String collaborationId) {
        assert tabId == mCurrentTabId;
        assert mTransitiveSharedGroupObserver == null
                || Objects.equals(
                        collaborationId,
                        mTransitiveSharedGroupObserver.getCollaborationIdSupplier().get());
        if (menuId == R.id.ungroup_tab || menuId == R.id.select_tabs) {
            RecordUserAction.record("TabGridDialogMenu.SelectTabs");
            mModel.set(TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED, false);
            if (setupAndShowTabListEditor(tabId)) {
                TabUiMetricsHelper.recordSelectionEditorOpenMetrics(
                        TabListEditorOpenMetricGroups.OPEN_FROM_DIALOG, mActivity);
            }
        } else if (menuId == R.id.edit_group_name) {
            RecordUserAction.record("TabGridDialogMenu.Rename");
            mModel.set(TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED, true);
        } else if (menuId == R.id.edit_group_color) {
            RecordUserAction.record("TabGridDialogMenu.EditColor");
            mShowColorPickerPopupRunnable.run();
            TabUiMetricsHelper.recordTabGroupColorChangeActionMetrics(
                    TabGroupColorChangeActionType.VIA_OVERFLOW_MENU);
        } else if (menuId == R.id.manage_sharing) {
            RecordUserAction.record("TabGridDialogMenu.ManageSharing");
            mDataSharingTabManager.showManageSharing(mActivity, collaborationId);
        } else if (menuId == R.id.recent_activity) {
            RecordUserAction.record("TabGridDialogMenu.RecentActivity");
            mDataSharingTabManager.showRecentActivity(collaborationId);
        } else if (menuId == R.id.close_tab || menuId == R.id.delete_tab) {
            boolean hideTabGroups = menuId == R.id.close_tab;
            if (hideTabGroups) {
                RecordUserAction.record("TabGridDialogMenu.Close");
            } else {
                RecordUserAction.record("TabGridDialogMenu.Delete");
            }
            TabUiUtils.closeTabGroup(
                    (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get(),
                    mActionConfirmationManager,
                    tabId,
                    hideTabGroups,
                    mTabGroupSyncService != null,
                    /* didCloseCallback= */ null);
        } else if (menuId == R.id.delete_shared_group) {
            RecordUserAction.record("TabGridDialogMenu.DeleteShared");
            TabUiUtils.deleteSharedTabGroup(
                    mActivity,
                    (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get(),
                    mActionConfirmationManager,
                    mModalDialogManager,
                    tabId);
        } else if (menuId == R.id.leave_group) {
            RecordUserAction.record("TabGridDialogMenu.LeaveShared");
            TabUiUtils.leaveTabGroup(
                    mActivity,
                    (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get(),
                    mActionConfirmationManager,
                    mModalDialogManager,
                    tabId);
        }
    }

    private View.OnClickListener getMenuButtonClickListener() {
        assert mTabListEditorControllerSupplier != null;
        boolean isTabGroupSyncEnabled = mTabGroupSyncService != null;

        IdentityManager identityManager = null;
        if (isTabGroupSyncEnabled && ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING)) {
            identityManager = IdentityServicesProvider.get().getIdentityManager(mOriginalProfile);
        }
        if (mTabGridDialogMenuCoordinator == null) {
            mTabGridDialogMenuCoordinator =
                    new TabGridDialogMenuCoordinator(
                            this::onToolbarMenuItemClick,
                            () -> mCurrentTabModelFilterSupplier.get().getTabModel(),
                            () -> mCurrentTabId,
                            isTabGroupSyncEnabled,
                            identityManager,
                            mTabGroupSyncService,
                            mDataSharingService);
        }

        return mTabGridDialogMenuCoordinator.getOnClickListener();
    }

    private View.OnClickListener getShareBarClickListener() {
        return view -> {
            handleShareClick();
        };
    }

    private void handleShareClick() {
        assert ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING);

        mModel.set(TabGridDialogProperties.IS_SHARE_SHEET_VISIBLE, true);

        String tabGroupDisplayName = mModel.get(TabGridDialogProperties.HEADER_TITLE);
        TabGroupModelFilter filter = (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();

        TabUiUtils.startShareTabGroupFlow(
                mActivity,
                filter,
                mDataSharingTabManager,
                mCurrentTabId,
                tabGroupDisplayName,
                (groupCreated) -> {
                    mModel.set(TabGridDialogProperties.IS_SHARE_SHEET_VISIBLE, false);
                });
    }

    private void updateTabGroupId() {
        if (mTransitiveSharedGroupObserver == null) return;

        boolean isIncognitoBranded = mCurrentTabModelFilterSupplier.get().isIncognitoBranded();
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING)
                || isIncognitoBranded
                || mCurrentTabId == Tab.INVALID_TAB_ID) {
            mTransitiveSharedGroupObserver.setTabGroupId(/* tabGroupId= */ null);
            return;
        }

        TabGroupModelFilter filter = (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();
        Tab tab = filter.getTabModel().getTabById(mCurrentTabId);
        mTransitiveSharedGroupObserver.setTabGroupId(tab.getTabGroupId());
    }

    private void onCollaborationIdChanged(@Nullable String collaborationId) {
        if (TabShareUtils.isCollaborationIdValid(collaborationId)) {
            showOrUpdateCollaborationActivityMessageCard();

            assert mSharedImageTilesCoordinator != null;
            mSharedImageTilesCoordinator.updateCollaborationId(collaborationId);
        } else {
            if (mSharedImageTilesCoordinator != null) {
                // Remove any images left in the shared image tiles component so they don't waste
                // memory.
                mSharedImageTilesCoordinator.updateCollaborationId(/* collaborationId= */ null);
            }
            removeCollaborationActivityMessageCard();
        }
    }

    private boolean shouldShowShareButton() {
        // TODO(crbug.com/360184707): Check DataSharingService configuration to see whether to show
        // the share button.
        return !mCurrentTabModelFilterSupplier.get().isIncognitoBranded();
    }

    private void onGroupSharedStateChanged(@Nullable @GroupSharedState Integer groupSharedState) {
        if (groupSharedState == null
                || groupSharedState == GroupSharedState.NOT_SHARED
                || groupSharedState == GroupSharedState.COLLABORATION_ONLY) {
            mModel.set(TabGridDialogProperties.SHOW_SHARE_BUTTON, shouldShowShareButton());
            mModel.set(TabGridDialogProperties.SHOW_IMAGE_TILES, false);
        } else {
            mModel.set(TabGridDialogProperties.SHOW_SHARE_BUTTON, false);
            mModel.set(TabGridDialogProperties.SHOW_IMAGE_TILES, true);
        }
    }

    private List<Tab> getRelatedTabs(int tabId) {
        return mCurrentTabModelFilterSupplier.get().getRelatedTabList(tabId);
    }

    private void saveCurrentGroupModifiedTitle() {
        TabModelFilter filter = mCurrentTabModelFilterSupplier.get();
        Tab currentTab = filter.getTabModel().getTabById(mCurrentTabId);
        // When current group no longer exists, skip saving the title.
        if (currentTab == null || !filter.isTabInTabGroup(currentTab)) {
            mCurrentGroupModifiedTitle = null;
        }

        if (mCurrentGroupModifiedTitle == null) {
            return;
        }
        assert mTabGroupTitleEditor != null;

        int tabsCount = getRelatedTabs(mCurrentTabId).size();
        if (mCurrentGroupModifiedTitle.length() == 0
                || TabGroupTitleUtils.isDefaultTitle(
                        mActivity, mCurrentGroupModifiedTitle, tabsCount)) {
            // When dialog title is empty or was unchanged, delete previously stored title and
            // restore default title.
            mTabGroupTitleEditor.deleteTabGroupTitle(currentTab.getRootId());

            String originalTitle = TabGroupTitleUtils.getDefaultTitle(mActivity, tabsCount);
            mModel.set(
                    TabGridDialogProperties.COLLAPSE_BUTTON_CONTENT_DESCRIPTION,
                    mActivity
                            .getResources()
                            .getQuantityString(
                                    R.plurals.accessibility_dialog_back_button,
                                    tabsCount,
                                    tabsCount));
            mModel.set(TabGridDialogProperties.HEADER_TITLE, originalTitle);
            mTabGroupTitleEditor.updateTabGroupTitle(currentTab, originalTitle);
            mCurrentGroupModifiedTitle = null;
            RecordUserAction.record("TabGridDialog.ResetTabGroupName");
            return;
        }
        mTabGroupTitleEditor.storeTabGroupTitle(currentTab.getRootId(), mCurrentGroupModifiedTitle);
        mTabGroupTitleEditor.updateTabGroupTitle(currentTab, mCurrentGroupModifiedTitle);
        int relatedTabsCount = getRelatedTabs(mCurrentTabId).size();
        mModel.set(
                TabGridDialogProperties.COLLAPSE_BUTTON_CONTENT_DESCRIPTION,
                mActivity.getResources()
                        .getQuantityString(
                                R.plurals.accessibility_dialog_back_button_with_group_name,
                                relatedTabsCount,
                                mCurrentGroupModifiedTitle,
                                relatedTabsCount));
        mModel.set(TabGridDialogProperties.HEADER_TITLE, mCurrentGroupModifiedTitle);
        RecordUserAction.record("TabGridDialog.TabGroupNamedInDialog");
        mCurrentGroupModifiedTitle = null;
    }

    TabListMediator.TabGridDialogHandler getTabGridDialogHandler() {
        return mTabGridDialogHandler;
    }

    // SnackbarManager.SnackbarController implementation.
    @Override
    public void onAction(Object actionData) {
        if (actionData instanceof Integer) {
            int tabId = (Integer) actionData;
            TabModel model = mCurrentTabModelFilterSupplier.get().getTabModel();

            model.cancelTabClosure(tabId);
        } else {
            List<Tab> tabs = (List<Tab>) actionData;
            if (tabs.isEmpty()) return;
            TabModel model = mCurrentTabModelFilterSupplier.get().getTabModel();

            for (Tab tab : tabs) {
                model.cancelTabClosure(tab.getId());
            }
        }
    }

    @Override
    public void onDismissNoAction(Object actionData) {
        if (actionData instanceof Integer) {
            int tabId = (Integer) actionData;
            TabModel model = mCurrentTabModelFilterSupplier.get().getTabModel();

            model.commitTabClosure(tabId);
        } else {
            List<Tab> tabs = (List<Tab>) actionData;
            if (tabs.isEmpty()) return;

            TabModel model = mCurrentTabModelFilterSupplier.get().getTabModel();

            for (Tab tab : tabs) {
                model.commitTabClosure(tab.getId());
            }
        }
    }

    // OnLongPressTabItemEventListener implementation
    @Override
    public void onLongPressEvent(int tabId) {
        if (setupAndShowTabListEditor(tabId)) {
            RecordUserAction.record("TabMultiSelectV2.OpenLongPressInDialog");
        }
    }

    private boolean setupAndShowTabListEditor(int currentTabId) {
        if (mTabListEditorControllerSupplier == null) return false;

        List<Tab> tabs = getRelatedTabs(currentTabId);
        // Setup dialog selection editor.
        mTabListEditorControllerSupplier.get().show(tabs, mRecyclerViewPositionSupplier.get());
        configureTabListEditorMenu();
        return true;
    }

    private void onTabModelFilterChanged(
            @Nullable TabModelFilter newFilter, @Nullable TabModelFilter oldFilter) {
        removeTabModelFilterObserver(oldFilter);

        if (newFilter != null) {
            boolean isIncognito = newFilter.isIncognito();
            updateColorProperties(mActivity, isIncognito);
            newFilter.addObserver(mTabModelObserver);
            ((TabGroupModelFilter) newFilter).addTabGroupObserver(mTabGroupModelFilterObserver);
        }
    }

    private void removeTabModelFilterObserver(@Nullable TabModelFilter filter) {
        if (filter != null) {
            filter.removeObserver(mTabModelObserver);
            ((TabGroupModelFilter) filter).removeTabGroupObserver(mTabGroupModelFilterObserver);
        }
    }

    private boolean currentTabRootIdMatchesRootId(int rootId) {
        Tab tab = mCurrentTabModelFilterSupplier.get().getTabModel().getTabById(mCurrentTabId);
        return tab != null && tab.getRootId() == rootId;
    }

    /**
     * A handler that handles TabGridDialog related changes originated from {@link TabListMediator}
     * and {@link TabGridItemTouchHelperCallback}.
     */
    class DialogHandler implements TabListMediator.TabGridDialogHandler {
        @Override
        public void updateUngroupBarStatus(@TabGridDialogView.UngroupBarStatus int status) {
            mModel.set(TabGridDialogProperties.UNGROUP_BAR_STATUS, status);
        }

        @Override
        public void updateDialogContent(int tabId) {
            mCurrentTabId = tabId;
            updateDialog();
        }
    }

    int getCurrentTabIdForTesting() {
        return mCurrentTabId;
    }

    void setCurrentTabIdForTesting(int tabId) {
        var oldValue = mCurrentTabId;
        mCurrentTabId = tabId;
        ResettersForTesting.register(() -> mCurrentTabId = oldValue);
    }

    KeyboardVisibilityDelegate.KeyboardVisibilityListener
            getKeyboardVisibilityListenerForTesting() {
        return mKeyboardVisibilityListener;
    }

    boolean getIsUpdatingTitleForTesting() {
        return mIsUpdatingTitle;
    }

    String getCurrentGroupModifiedTitleForTesting() {
        return mCurrentGroupModifiedTitle;
    }

    Runnable getScrimClickRunnableForTesting() {
        return mScrimClickRunnable;
    }

    private void removeCollaborationActivityMessageCard() {
        mDialogController.removeMessageCardItem(MessageType.COLLABORATION_ACTIVITY);
        mCollaborationActivityPropertyModel = null;
    }

    private @Nullable Token getCurrentTabGroupId() {
        TabGroupModelFilter filter = (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get();
        Tab tab = filter.getTabModel().getTabById(mCurrentTabId);
        return tab == null ? null : tab.getTabGroupId();
    }

    private void updateOnMatch(PersistentMessage message) {
        if (message.attribution.localTabGroupId == null) return;
        @Nullable Token token = getCurrentTabGroupId();
        if (Objects.equals(token, message.attribution.localTabGroupId.tabGroupId)) {
            showOrUpdateCollaborationActivityMessageCard();
        }
    }

    private void showOrUpdateCollaborationActivityMessageCard() {
        @Nullable Token currentTabGroupId = getCurrentTabGroupId();
        if (currentTabGroupId == null) {
            assert mCollaborationActivityPropertyModel == null;
            return;
        }

        EitherGroupId eitherGroupId =
                EitherGroupId.createLocalId(new LocalTabGroupId(currentTabGroupId));
        List<PersistentMessage> messages =
                mMessagingBackendService.getMessagesForGroup(
                        eitherGroupId, /* type= */ Optional.empty());
        Map<Integer, Integer> actionCounts = new HashMap<>();
        for (PersistentMessage message : messages) {
            actionCounts.merge(message.action, 1, Integer::sum);
        }
        int tabsAdded = actionCounts.getOrDefault(UserAction.TAB_ADDED, 0);
        int tabsChanged = actionCounts.getOrDefault(UserAction.TAB_NAVIGATED, 0);
        int tabsClosed = actionCounts.getOrDefault(UserAction.TAB_REMOVED, 0);
        if (tabsAdded == 0 && tabsChanged == 0 && tabsClosed == 0) {
            removeCollaborationActivityMessageCard();
            return;
        }

        if (mCollaborationActivityPropertyModel == null) {
            mCollaborationActivityPropertyModel =
                    new CollaborationActivityMessageCardViewModel(
                            mActivity,
                            this::showRecentActivityOrDismissActivityMessageCard,
                            (unused) -> removeCollaborationActivityMessageCard());
        }
        mCollaborationActivityPropertyModel.updateDescriptionText(
                mActivity, tabsAdded, tabsChanged, tabsClosed);

        if (!mDialogController.messageCardExists(MessageType.COLLABORATION_ACTIVITY)) {
            mDialogController.addMessageCardItem(
                    /* position= */ 0, mCollaborationActivityPropertyModel.getPropertyModel());
        }
    }

    private void showRecentActivityOrDismissActivityMessageCard() {
        assert mTransitiveSharedGroupObserver != null;
        @Nullable
        String collaborationId = mTransitiveSharedGroupObserver.getCollaborationIdSupplier().get();
        if (TabShareUtils.isCollaborationIdValid(collaborationId)) {
            mDataSharingTabManager.showRecentActivity(collaborationId);
        } else {
            removeCollaborationActivityMessageCard();
        }
    }

    /** AppHeaderObserver implementation */
    @Override
    public void onAppHeaderStateChanged(AppHeaderState state) {
        mModel.set(TabGridDialogProperties.APP_HEADER_HEIGHT, state.getAppHeaderHeight());
    }
}
