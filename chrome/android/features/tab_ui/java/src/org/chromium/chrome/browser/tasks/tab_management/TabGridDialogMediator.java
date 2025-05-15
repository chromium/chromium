// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.DATA_SHARING;

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
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.collaboration.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesCoordinator;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tab_ui.TabUiThemeUtils;
import org.chromium.chrome.browser.tabmodel.TabClosureParamsUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.TabGridContextMenuCoordinator.ShowTabListEditor;
import org.chromium.chrome.browser.tasks.tab_management.TabGridItemLongPressOrchestrator.CancelLongPressTabItemEventListener;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.TabGroupCreationCallback;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.TabMovedCallback;
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
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager.AppHeaderObserver;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.list_view.ListViewTouchTracker;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.CollaborationServiceLeaveOrDeleteEntryPoint;
import org.chromium.components.collaboration.CollaborationServiceShareOrManageEntryPoint;
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.MessageUtils;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.MessagingBackendService.PersistentMessageObserver;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.PersistentNotificationType;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.tab_group_sync.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.EmptyTextWatcher;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import java.util.Set;

/**
 * A mediator for the TabGridDialog component, responsible for communicating with the components'
 * coordinator as well as managing the business logic for dialog show/hide.
 */
public class TabGridDialogMediator
        implements SnackbarManager.SnackbarController,
                TabGridDialogView.VisibilityListener,
                TabGridItemLongPressOrchestrator.OnLongPressTabItemEventListener,
                AppHeaderObserver {
    @VisibleForTesting static final String SHOW_SEND_FEEDBACK_PARAM = "show_send_feedback";
    @VisibleForTesting static final String SHARE_FEEDBACK_CATEGORY_SUFFIX = ".tab_group_share";

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

        /**
         * Sets the content sensitivity on the dialog view.
         *
         * <p>Note: The view on which the content sensitivity is set is shared between all tab grid
         * dialog instances.
         *
         * @param contentIsSensitive True if the grid is sensitive.
         */
        void setGridContentSensitivity(boolean contentIsSensitive);
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

    private final ValueChangedCallback<TabGroupModelFilter> mOnTabGroupModelFilterChanged =
            new ValueChangedCallback<>(this::onTabGroupModelFilterChanged);
    private final Callback<String> mOnCollaborationIdChanged = this::onCollaborationIdChanged;
    private final Callback<Integer> mOnGroupSharedStateChanged = this::onGroupSharedStateChanged;
    private final Callback<List<GroupMember>> mOnGroupMembersChanged = this::onGroupMembersChanged;
    private final Activity mActivity;
    private final DialogController mDialogController;
    private final PropertyModel mModel;
    private final ObservableSupplier<TabGroupModelFilter> mCurrentTabGroupModelFilterSupplier;
    private final @Nullable TabSwitcherResetHandler mTabSwitcherResetHandler;
    private final Supplier<RecyclerViewPosition> mRecyclerViewPositionSupplier;
    private final AnimationSourceViewProvider mAnimationSourceViewProvider;
    private final DialogHandler mTabGridDialogHandler;
    private final @Nullable SnackbarManager mSnackbarManager;
    private final @NonNull BottomSheetController mBottomSheetController;
    private final @Nullable SharedImageTilesCoordinator mSharedImageTilesCoordinator;
    private final DataSharingTabManager mDataSharingTabManager;
    private final String mComponentName;
    private final Runnable mShowColorPickerPopupRunnable;
    private final Profile mOriginalProfile;
    private final @Nullable TabGroupSyncService mTabGroupSyncService;
    private final @Nullable DataSharingService mDataSharingService;
    private final @NonNull CollaborationService mCollaborationService;
    private final @Nullable TransitiveSharedGroupObserver mTransitiveSharedGroupObserver;
    private final @Nullable MessagingBackendService mMessagingBackendService;
    private final @Nullable MessagingBackendService.PersistentMessageObserver
            mPersistentMessageObserver;
    private final TabModelObserver mTabModelObserver;
    private final TabGroupModelFilterObserver mTabGroupModelFilterObserver;
    private final Runnable mScrimClickRunnable;
    private final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;
    private final BottomSheetObserver mBottomSheetObserver;

    private @Nullable TabGroupListBottomSheetCoordinator mTabGroupListBottomSheetCoordinator;
    private int mCurrentTabId = Tab.INVALID_TAB_ID;
    private TabGridDialogMenuCoordinator mTabGridDialogMenuCoordinator;
    private Supplier<TabListEditorController> mTabListEditorControllerSupplier;
    private @Nullable TabGridContextMenuCoordinator mTabGridContextMenuCoordinator;
    private boolean mTabListEditorSetup;
    private KeyboardVisibilityDelegate.KeyboardVisibilityListener mKeyboardVisibilityListener;
    private boolean mIsUpdatingTitle;
    private String mCurrentGroupModifiedTitle;
    private @Nullable CollaborationActivityMessageCardViewModel mCollaborationActivityPropertyModel;

    TabGridDialogMediator(
            Activity activity,
            DialogController dialogController,
            PropertyModel model,
            ObservableSupplier<TabGroupModelFilter> currentTabGroupModelFilterSupplier,
            @Nullable TabSwitcherResetHandler tabSwitcherResetHandler,
            Supplier<RecyclerViewPosition> recyclerViewPositionSupplier,
            AnimationSourceViewProvider animationSourceViewProvider,
            @Nullable SnackbarManager snackbarManager,
            @NonNull BottomSheetController bottomSheetController,
            @Nullable SharedImageTilesCoordinator sharedImageTilesCoordinator,
            @NonNull DataSharingTabManager dataSharingTabManager,
            String componentName,
            Runnable showColorPickerPopupRunnable,
            @Nullable ModalDialogManager modalDialogManager,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            ObservableSupplier<TabBookmarker> tabBookmarkerSupplier,
            Supplier<ShareDelegate> shareDelegateSupplier) {
        mActivity = activity;
        mDialogController = dialogController;
        mModel = model;
        mCurrentTabGroupModelFilterSupplier = currentTabGroupModelFilterSupplier;
        mTabSwitcherResetHandler = tabSwitcherResetHandler;
        mRecyclerViewPositionSupplier = recyclerViewPositionSupplier;
        mAnimationSourceViewProvider = animationSourceViewProvider;
        mTabGridDialogHandler = new DialogHandler();
        mSnackbarManager = snackbarManager;
        mBottomSheetController = bottomSheetController;
        mSharedImageTilesCoordinator = sharedImageTilesCoordinator;
        mDataSharingTabManager = dataSharingTabManager;
        mComponentName = componentName;
        mShowColorPickerPopupRunnable = showColorPickerPopupRunnable;
        mOriginalProfile =
                mCurrentTabGroupModelFilterSupplier
                        .get()
                        .getTabModel()
                        .getProfile()
                        .getOriginalProfile();
        mDesktopWindowStateManager = desktopWindowStateManager;
        mTabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(mOriginalProfile);
        mCollaborationService = CollaborationServiceFactory.getForProfile(mOriginalProfile);
        if (mTabGroupSyncService != null
                && mCollaborationService.getServiceStatus().isAllowedToJoin()) {
            mDataSharingService = DataSharingServiceFactory.getForProfile(mOriginalProfile);
            mTransitiveSharedGroupObserver =
                    new TransitiveSharedGroupObserver(
                            mTabGroupSyncService, mDataSharingService, mCollaborationService);
            // This should be the first supplier set as the other suppliers depend on its value.
            mTransitiveSharedGroupObserver
                    .getCollaborationIdSupplier()
                    .addObserver(mOnCollaborationIdChanged);
            mTransitiveSharedGroupObserver
                    .getGroupSharedStateSupplier()
                    .addObserver(mOnGroupSharedStateChanged);
            mTransitiveSharedGroupObserver
                    .getGroupMembersSupplier()
                    .addObserver(mOnGroupMembersChanged);
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

        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        if (!isVisible()) return;

                        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
                        if (filter == null || !filter.isTabModelRestored()) {
                            return;
                        }

                        // For tab group sync or data sharing a tab can be added without needing
                        // to close the tab grid dialog. The UI updates for this event are driven
                        // from TabListMediator's implementation.
                        if (type == TabLaunchType.FROM_SYNC_BACKGROUND
                                || type == TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP) {
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
                        if (currentTabRootIdMatchesRootId(rootId)) {
                            mModel.set(TabGridDialogProperties.TAB_GROUP_COLOR_ID, newColor);
                        }
                    }
                };

        mOnTabGroupModelFilterChanged.onResult(
                mCurrentTabGroupModelFilterSupplier.addObserver(mOnTabGroupModelFilterChanged));

        // Setup ScrimView click Runnable.
        mScrimClickRunnable =
                () -> {
                    hideDialog(true);
                    RecordUserAction.record("TabGridDialog.Exit");
                };
        mModel.set(TabGridDialogProperties.VISIBILITY_LISTENER, this);
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridDialogProperties.IS_CONTENT_SENSITIVE, false);
        mModel.set(
                TabGridDialogProperties.UNGROUP_BAR_STATUS,
                TabGridDialogView.UngroupBarStatus.HIDE);
        if (mDesktopWindowStateManager != null) {
            mDesktopWindowStateManager.addObserver(this);
            if (mDesktopWindowStateManager.getAppHeaderState() != null) {
                onAppHeaderStateChanged(mDesktopWindowStateManager.getAppHeaderState());
            }
        }

        if (ChromeFeatureList.sTabGroupParityBottomSheetAndroid.isEnabled()) {
            TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
            Profile profile = filter.getTabModel().getProfile();
            if (profile != null && modalDialogManager != null) {
                TabGroupCreationDialogManager tabGroupCreationDialogManager =
                        new TabGroupCreationDialogManager(activity, modalDialogManager, null);
                TabGroupCreationCallback tabGroupCreationCallback =
                        groupId -> tabGroupCreationDialogManager.showDialog(groupId, filter);

                // Dismiss the dialog if open. The dialog should be open when the bottom sheet is
                // visible.
                TabMovedCallback tabMovedCallback = () -> hideDialog(true);
                mTabGroupListBottomSheetCoordinator =
                        new TabGroupListBottomSheetCoordinator(
                                activity,
                                profile,
                                tabGroupCreationCallback,
                                tabMovedCallback,
                                filter,
                                bottomSheetController,
                                true,
                                false);

                CollaborationService collaborationService =
                        CollaborationServiceFactory.getForProfile(profile);
                ShowTabListEditor showTabListEditor =
                        tabId -> {
                            setupAndShowTabListEditor(mCurrentTabId);
                            TabListEditorController tabListEditorController =
                                    mTabListEditorControllerSupplier.get();
                            if (tabListEditorController != null) {
                                tabListEditorController.selectTabs(
                                        Set.of(TabListEditorItemSelectionId.createTabId(tabId)));
                            }
                        };
                mTabGridContextMenuCoordinator =
                        new TabGridContextMenuCoordinator(
                                activity,
                                tabBookmarkerSupplier,
                                profile,
                                filter,
                                mTabGroupListBottomSheetCoordinator,
                                tabGroupCreationDialogManager,
                                shareDelegateSupplier,
                                mTabGroupSyncService,
                                collaborationService,
                                showTabListEditor);
            }
        }

        mBottomSheetObserver =
                new BottomSheetObserver() {
                    @Override
                    public void onSheetOpened(int reason) {
                        mModel.set(TabGridDialogProperties.SUPPRESS_ACCESSIBILITY, true);
                    }

                    @Override
                    public void onSheetClosed(int reason) {
                        mModel.set(TabGridDialogProperties.SUPPRESS_ACCESSIBILITY, false);
                    }

                    @Override
                    public void onSheetOffsetChanged(float heightFraction, float offsetPx) {}

                    @Override
                    public void onSheetStateChanged(int newState, int reason) {}

                    @Override
                    public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {}
                };
        mBottomSheetController.addObserver(mBottomSheetObserver);
    }

    public void initWithNative(
            @NonNull Supplier<TabListEditorController> tabListEditorControllerSupplier) {
        mTabListEditorControllerSupplier = tabListEditorControllerSupplier;

        setupToolbarClickHandlers();
        setupToolbarEditText();

        mModel.set(TabGridDialogProperties.MENU_CLICK_LISTENER, getMenuButtonClickListener());

        mModel.set(TabGridDialogProperties.SHARE_BUTTON_CLICK_LISTENER, getShareClickListener());
        mModel.set(
                TabGridDialogProperties.SHARE_IMAGE_TILES_CLICK_LISTENER, getShareClickListener());
        mModel.set(TabGridDialogProperties.SEND_FEEDBACK_RUNNABLE, this::sendFeedback);
    }

    void hideDialog(boolean showAnimation) {
        if (!isVisible()) {
            if (!showAnimation) {
                // Forcibly finish any pending animations.
                mModel.set(TabGridDialogProperties.FORCE_ANIMATION_TO_FINISH, true);
                mModel.set(TabGridDialogProperties.FORCE_ANIMATION_TO_FINISH, false);
            }
            return;
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

    /**
     * Loads the provided list of tabs into the dialog and returns whether the dialog started to
     * show.
     */
    boolean onReset(@Nullable List<Tab> tabs) {
        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        if (tabs == null) {
            mCurrentTabId = Tab.INVALID_TAB_ID;
        } else {
            mCurrentTabId =
                    filter.getRepresentativeTabAt(filter.representativeIndexOf(tabs.get(0)))
                            .getId();
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
            mDialogController.prepareDialog();

            // Do this after the dialog is updated so most attributes are not set with stale values
            // when the binding token is set.
            mModel.set(TabGridDialogProperties.BINDING_TOKEN, hashCode());

            mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

            requestShowBottomSheet();
            return true;
        } else if (isVisible()) {
            mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, false);
        }
        return false;
    }

    /** Destroy any members that needs clean up. */
    public void destroy() {
        removeTabGroupModelFilterObserver(mCurrentTabGroupModelFilterSupplier.get());
        mCurrentTabGroupModelFilterSupplier.removeObserver(mOnTabGroupModelFilterChanged);
        KeyboardVisibilityDelegate.getInstance()
                .removeKeyboardVisibilityListener(mKeyboardVisibilityListener);
        if (mTransitiveSharedGroupObserver != null) {
            mTransitiveSharedGroupObserver
                    .getGroupSharedStateSupplier()
                    .removeObserver(mOnGroupSharedStateChanged);
            mTransitiveSharedGroupObserver
                    .getGroupMembersSupplier()
                    .removeObserver(mOnGroupMembersChanged);
            mTransitiveSharedGroupObserver
                    .getCollaborationIdSupplier()
                    .removeObserver(mOnCollaborationIdChanged);
            mTransitiveSharedGroupObserver.destroy();
        }
        if (mDesktopWindowStateManager != null) {
            mDesktopWindowStateManager.removeObserver(this);
        }
        if (mMessagingBackendService != null && mPersistentMessageObserver != null) {
            mMessagingBackendService.removePersistentMessageObserver(mPersistentMessageObserver);
        }
        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

    boolean isVisible() {
        return Boolean.TRUE.equals(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE));
    }

    void setSelectedTabGroupColor(int selectedColor) {
        mModel.set(TabGridDialogProperties.TAB_GROUP_COLOR_ID, selectedColor);

        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        Tab currentTab = filter.getTabModel().getTabById(mCurrentTabId);

        if (currentTab != null) {
            filter.setTabGroupColor(currentTab.getRootId(), selectedColor);
        }
    }

    private void requestShowBottomSheet() {
        if (mTransitiveSharedGroupObserver != null) {
            @Nullable
            String collaborationId =
                    mTransitiveSharedGroupObserver.getCollaborationIdSupplier().get();
            if (TabShareUtils.isCollaborationIdValid(collaborationId)) {
                TabGroupShareNoticeBottomSheetCoordinator bottomSheetCoordinator =
                        new TabGroupShareNoticeBottomSheetCoordinator(
                                mBottomSheetController, mActivity, mOriginalProfile);
                bottomSheetCoordinator.requestShowContent();
            }
        }
    }

    void setGridContentSensitivity(boolean contentIsSensitive) {
        mModel.set(TabGridDialogProperties.IS_CONTENT_SENSITIVE, contentIsSensitive);
    }

    private void updateGridTabSwitcher() {
        if (!isVisible() || mTabSwitcherResetHandler == null) return;
        mTabSwitcherResetHandler.resetWithListOfTabs(
                mCurrentTabGroupModelFilterSupplier.get().getRepresentativeTabList());
    }

    private void updateDialog() {
        final int tabCount = getRelatedTabs(mCurrentTabId).size();
        if (tabCount == 0) {
            hideDialog(true);
            return;
        }

        updateUngroupBarText(tabCount);

        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        Tab currentTab = filter.getTabModel().getTabById(mCurrentTabId);
        final @TabGroupColorId int color =
                filter.getTabGroupColorWithFallback(currentTab.getRootId());
        mModel.set(TabGridDialogProperties.TAB_GROUP_COLOR_ID, color);
        updateTitle(tabCount);
    }

    private void updateTitle(int tabsCount) {
        Resources res = mActivity.getResources();

        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        Tab currentTab = filter.getTabModel().getTabById(mCurrentTabId);
        String storedTitle = filter.getTabGroupTitle(currentTab.getRootId());
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
        Tab currentTab = mCurrentTabGroupModelFilterSupplier.get().getCurrentRepresentativeTab();
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
                        IconPosition.START));
        actions.add(
                TabListEditorUngroupAction.createAction(
                        mActivity,
                        ShowMode.MENU_ONLY,
                        ButtonType.ICON_AND_TEXT,
                        IconPosition.START));
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
            TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
            TabModel tabModel = filter.getTabModel();
            Tab currentTab = tabModel.getTabById(mCurrentTabId);
            hideDialog(false);

            if (currentTab == null) {
                tabModel.getTabCreator().launchNtp();
                return;
            }

            TabGroupUtils.openUrlInGroup(
                    mCurrentTabGroupModelFilterSupplier.get(),
                    UrlConstants.NTP_URL,
                    currentTab.getId(),
                    TabLaunchType.FROM_TAB_GROUP_UI);
            RecordUserAction.record("MobileNewTabOpened." + mComponentName);
        };
    }

    @VisibleForTesting
    public void onToolbarMenuItemClick(
            int menuId,
            Token tabGroupId,
            @Nullable String collaborationId,
            @Nullable ListViewTouchTracker listViewTouchTracker) {
        // Collaboration IDs will not change without the menu somehow being dismissed. This assert
        // should always hold.
        assert mTransitiveSharedGroupObserver == null
                || Objects.equals(
                        collaborationId,
                        mTransitiveSharedGroupObserver.getCollaborationIdSupplier().get());

        int tabId = mCurrentTabId;
        EitherGroupId eitherId = EitherGroupId.createLocalId(new LocalTabGroupId(tabGroupId));
        if (tabId == Tab.INVALID_TAB_ID) return;

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
            mDataSharingTabManager.createOrManageFlow(
                    eitherId,
                    CollaborationServiceShareOrManageEntryPoint.ANDROID_TAB_GRID_DIALOG_MANAGE,
                    /* createGroupFinishedCallback= */ null);
        } else if (menuId == R.id.recent_activity) {
            RecordUserAction.record("TabGridDialogMenu.RecentActivity");
            mDataSharingTabManager.showRecentActivity(mActivity, collaborationId);
            dismissAllDirtyTabMessagesForCurrentGroup();
        } else if (menuId == R.id.close_tab_group || menuId == R.id.delete_tab_group) {
            boolean hideTabGroups = menuId == R.id.close_tab_group;
            if (hideTabGroups) {
                RecordUserAction.record("TabGridDialogMenu.Close");
            } else {
                RecordUserAction.record("TabGridDialogMenu.Delete");
            }

            boolean allowUndo = TabClosureParamsUtils.shouldAllowUndo(listViewTouchTracker);

            TabUiUtils.closeTabGroup(
                    mCurrentTabGroupModelFilterSupplier.get(),
                    tabId,
                    allowUndo,
                    hideTabGroups,
                    /* didCloseCallback= */ null);
        } else if (menuId == R.id.delete_shared_group) {
            RecordUserAction.record("TabGridDialogMenu.DeleteShared");
            mDataSharingTabManager.leaveOrDeleteFlow(
                    eitherId,
                    CollaborationServiceLeaveOrDeleteEntryPoint.ANDROID_TAB_GRID_DIALOG_DELETE);
        } else if (menuId == R.id.leave_group) {
            RecordUserAction.record("TabGridDialogMenu.LeaveShared");
            mDataSharingTabManager.leaveOrDeleteFlow(
                    eitherId,
                    CollaborationServiceLeaveOrDeleteEntryPoint.ANDROID_TAB_GRID_DIALOG_LEAVE);
        }
    }

    private View.OnClickListener getMenuButtonClickListener() {
        assert mTabListEditorControllerSupplier != null;

        if (mTabGridDialogMenuCoordinator == null) {
            Supplier<Token> tabGroupIdSupplier =
                    () -> {
                        TabModel tabModel = mCurrentTabGroupModelFilterSupplier.get().getTabModel();
                        @Nullable Tab tab = tabModel.getTabById(mCurrentTabId);
                        return tab == null ? null : tab.getTabGroupId();
                    };
            mTabGridDialogMenuCoordinator =
                    new TabGridDialogMenuCoordinator(
                            this::onToolbarMenuItemClick,
                            () -> mCurrentTabGroupModelFilterSupplier.get().getTabModel(),
                            tabGroupIdSupplier,
                            mTabGroupSyncService,
                            mCollaborationService,
                            mActivity);
        }

        return mTabGridDialogMenuCoordinator.getOnClickListener();
    }

    private View.OnClickListener getShareClickListener() {
        return unused -> handleShareClick();
    }

    private void sendFeedback() {
        HelpAndFeedbackLauncher launcher =
                HelpAndFeedbackLauncherFactory.getForProfile(mOriginalProfile);
        String tag = mActivity.getPackageName() + SHARE_FEEDBACK_CATEGORY_SUFFIX;
        launcher.showFeedback(mActivity, /* url= */ null, tag);
    }

    private void handleShareClick() {
        assert mCollaborationService.getServiceStatus().isAllowedToJoin();

        saveCurrentGroupModifiedTitle();
        String tabGroupDisplayName = mModel.get(TabGridDialogProperties.HEADER_TITLE);
        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();

        TabUiUtils.startShareTabGroupFlow(
                mActivity,
                filter,
                mDataSharingTabManager,
                mCurrentTabId,
                tabGroupDisplayName,
                CollaborationServiceShareOrManageEntryPoint.DIALOG_TOOLBAR_BUTTON);
    }

    private void updateTabGroupId() {
        if (mTransitiveSharedGroupObserver == null) return;

        boolean isIncognitoBranded =
                mCurrentTabGroupModelFilterSupplier.get().getTabModel().isIncognitoBranded();
        if (isIncognitoBranded
                || !mCollaborationService.getServiceStatus().isAllowedToJoin()
                || mCurrentTabId == Tab.INVALID_TAB_ID) {
            mTransitiveSharedGroupObserver.setTabGroupId(/* tabGroupId= */ null);
            return;
        }

        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        Tab tab = filter.getTabModel().getTabById(mCurrentTabId);
        mTransitiveSharedGroupObserver.setTabGroupId(tab.getTabGroupId());
    }

    private void onCollaborationIdChanged(@Nullable String collaborationId) {
        if (TabShareUtils.isCollaborationIdValid(collaborationId)) {
            showOrUpdateCollaborationActivityMessageCard();
        } else {
            removeCollaborationActivityMessageCard();
        }
        int tabCount = getRelatedTabs(mCurrentTabId).size();
        updateUngroupBarText(tabCount);
    }

    private boolean shouldShowShareButton() {
        return !mCurrentTabGroupModelFilterSupplier.get().getTabModel().isIncognitoBranded()
                && mCollaborationService.getServiceStatus().isAllowedToCreate();
    }

    private boolean shouldShowSendFeedback() {
        return mCollaborationService.getServiceStatus().isAllowedToJoin()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        DATA_SHARING, SHOW_SEND_FEEDBACK_PARAM, true);
    }

    private void onGroupSharedStateChanged(@Nullable @GroupSharedState Integer groupSharedState) {
        if (groupSharedState == null || groupSharedState == GroupSharedState.NOT_SHARED) {
            mModel.set(
                    TabGridDialogProperties.SHARE_BUTTON_STRING_RES,
                    R.string.tab_grid_share_button_text);
            mModel.set(TabGridDialogProperties.SHOW_SHARE_BUTTON, shouldShowShareButton());
            mModel.set(TabGridDialogProperties.SHOW_IMAGE_TILES, false);
            mModel.set(TabGridDialogProperties.SHOW_SEND_FEEDBACK, false);
        } else if (groupSharedState == GroupSharedState.COLLABORATION_ONLY) {
            mModel.set(
                    TabGridDialogProperties.SHARE_BUTTON_STRING_RES,
                    R.string.tab_grid_manage_button_text);
            mModel.set(TabGridDialogProperties.SHOW_SHARE_BUTTON, shouldShowShareButton());
            mModel.set(TabGridDialogProperties.SHOW_IMAGE_TILES, false);
            mModel.set(TabGridDialogProperties.SHOW_SEND_FEEDBACK, shouldShowSendFeedback());
        } else {
            mModel.set(TabGridDialogProperties.SHOW_SHARE_BUTTON, false);
            mModel.set(TabGridDialogProperties.SHOW_IMAGE_TILES, true);
            mModel.set(TabGridDialogProperties.SHOW_SEND_FEEDBACK, shouldShowSendFeedback());
        }
    }

    private void onGroupMembersChanged(@Nullable List<GroupMember> members) {
        if (mSharedImageTilesCoordinator == null) return;

        @Nullable
        String collaborationId = mTransitiveSharedGroupObserver.getCollaborationIdSupplier().get();
        if (members != null && TabShareUtils.isCollaborationIdValid(collaborationId)) {
            mSharedImageTilesCoordinator.onGroupMembersChanged(collaborationId, members);
        } else {
            mSharedImageTilesCoordinator.onGroupMembersChanged(
                    /* collaborationId= */ null, /* members= */ null);
        }
    }

    private List<Tab> getRelatedTabs(int tabId) {
        return mCurrentTabGroupModelFilterSupplier.get().getRelatedTabList(tabId);
    }

    private void saveCurrentGroupModifiedTitle() {
        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        Tab currentTab = filter.getTabModel().getTabById(mCurrentTabId);
        // When current group no longer exists, skip saving the title.
        if (currentTab == null || !filter.isTabInTabGroup(currentTab)) {
            mCurrentGroupModifiedTitle = null;
        }

        if (mCurrentGroupModifiedTitle == null) {
            return;
        }

        int tabsCount = getRelatedTabs(mCurrentTabId).size();
        int rootId = currentTab.getRootId();
        if (mCurrentGroupModifiedTitle.length() == 0
                || TabGroupTitleUtils.isDefaultTitle(
                        mActivity, mCurrentGroupModifiedTitle, tabsCount)) {
            // When dialog title is empty or was unchanged, delete previously stored title and
            // restore default title.
            filter.deleteTabGroupTitle(rootId);

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
            // Setting the tab group title to null ensures the default title isn't saved, but
            // observers downstream will update to the correct default title.
            filter.setTabGroupTitle(rootId, null);
            mCurrentGroupModifiedTitle = null;
            RecordUserAction.record("TabGridDialog.ResetTabGroupName");
            return;
        }
        filter.setTabGroupTitle(rootId, mCurrentGroupModifiedTitle);
        int relatedTabsCount = getRelatedTabs(mCurrentTabId).size();
        mModel.set(
                TabGridDialogProperties.COLLAPSE_BUTTON_CONTENT_DESCRIPTION,
                mActivity
                        .getResources()
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
            TabModel model = mCurrentTabGroupModelFilterSupplier.get().getTabModel();

            model.cancelTabClosure(tabId);
        } else {
            List<Tab> tabs = (List<Tab>) actionData;
            if (tabs.isEmpty()) return;
            TabModel model = mCurrentTabGroupModelFilterSupplier.get().getTabModel();

            for (Tab tab : tabs) {
                model.cancelTabClosure(tab.getId());
            }
        }
    }

    @Override
    public void onDismissNoAction(Object actionData) {
        if (actionData instanceof Integer) {
            int tabId = (Integer) actionData;
            TabModel model = mCurrentTabGroupModelFilterSupplier.get().getTabModel();

            model.commitTabClosure(tabId);
        } else {
            List<Tab> tabs = (List<Tab>) actionData;
            if (tabs.isEmpty()) return;

            TabModel model = mCurrentTabGroupModelFilterSupplier.get().getTabModel();

            for (Tab tab : tabs) {
                model.commitTabClosure(tab.getId());
            }
        }
    }

    // OnLongPressTabItemEventListener implementation
    @Override
    public @Nullable CancelLongPressTabItemEventListener onLongPressEvent(
            @TabId int tabId, @Nullable View cardView) {
        return onLongPressEvent(tabId, cardView, mTabGridContextMenuCoordinator);
    }

    @VisibleForTesting
    @Nullable
    CancelLongPressTabItemEventListener onLongPressEvent(
            @TabId int tabId,
            @Nullable View cardView,
            @Nullable TabGridContextMenuCoordinator tabGridContextMenuCoordinator) {
        if (tabGridContextMenuCoordinator != null && cardView != null) {
            tabGridContextMenuCoordinator.showMenu(
                    new ViewRectProvider(cardView, TabGridViewRectUpdater::new), tabId);
            return tabGridContextMenuCoordinator::dismiss;
        }
        return null;
    }

    private boolean setupAndShowTabListEditor(int currentTabId) {
        if (mTabListEditorControllerSupplier == null) return false;

        List<Tab> tabs = getRelatedTabs(currentTabId);
        // Setup dialog selection editor.
        mTabListEditorControllerSupplier
                .get()
                .show(
                        tabs,
                        /* tabGroupSyncIds= */ Collections.emptyList(),
                        mRecyclerViewPositionSupplier.get());
        configureTabListEditorMenu();
        return true;
    }

    private void onTabGroupModelFilterChanged(
            @Nullable TabGroupModelFilter newFilter, @Nullable TabGroupModelFilter oldFilter) {
        removeTabGroupModelFilterObserver(oldFilter);

        if (newFilter != null) {
            boolean isIncognito = newFilter.getTabModel().isIncognito();
            updateColorProperties(mActivity, isIncognito);
            newFilter.addObserver(mTabModelObserver);
            newFilter.addTabGroupObserver(mTabGroupModelFilterObserver);
        }
    }

    private void removeTabGroupModelFilterObserver(@Nullable TabGroupModelFilter filter) {
        if (filter != null) {
            filter.removeObserver(mTabModelObserver);
            filter.removeTabGroupObserver(mTabGroupModelFilterObserver);
        }
    }

    private boolean currentTabRootIdMatchesRootId(int rootId) {
        Tab tab = mCurrentTabGroupModelFilterSupplier.get().getTabModel().getTabById(mCurrentTabId);
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
        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        Tab tab = filter.getTabModel().getTabById(mCurrentTabId);
        return tab == null ? null : tab.getTabGroupId();
    }

    private void updateOnMatch(PersistentMessage message) {
        if (message.attribution.tabGroupMetadata == null
                || message.attribution.tabGroupMetadata.localTabGroupId == null) return;
        @Nullable Token token = getCurrentTabGroupId();
        if (Objects.equals(
                token, message.attribution.tabGroupMetadata.localTabGroupId.tabGroupId)) {
            showOrUpdateCollaborationActivityMessageCard();
        }
    }

    private void dismissAllDirtyTabMessagesForCurrentGroup() {
        @Nullable Token tabGroupId = getCurrentTabGroupId();
        @Nullable
        String collaborationId =
                TabShareUtils.getCollaborationIdOrNull(tabGroupId, mTabGroupSyncService);
        if (mMessagingBackendService != null && collaborationId != null) {
            mMessagingBackendService.clearDirtyTabMessagesForGroup(collaborationId);
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
                        eitherGroupId,
                        /* type= */ Optional.of(PersistentNotificationType.DIRTY_TAB));

        Map<Integer, Integer> collaborationEventCounts = new HashMap<>();
        for (PersistentMessage message : messages) {
            collaborationEventCounts.merge(message.collaborationEvent, 1, Integer::sum);
        }
        // Added and updated will both be presented as new changes.
        int tabsAdded =
                collaborationEventCounts.getOrDefault(CollaborationEvent.TAB_ADDED, 0)
                        + collaborationEventCounts.getOrDefault(CollaborationEvent.TAB_UPDATED, 0);

        // Query for tombstoned entries from backend and look for the tab removals.
        List<PersistentMessage> tombstonedMessages =
                mMessagingBackendService.getMessages(
                        Optional.of(PersistentNotificationType.TOMBSTONED));
        int tabsClosed = 0;
        for (PersistentMessage message : tombstonedMessages) {
            if (message.collaborationEvent != CollaborationEvent.TAB_REMOVED) continue;
            if (!currentTabGroupId.equals(MessageUtils.extractTabGroupId(message))) continue;
            tabsClosed++;
        }

        if (tabsAdded == 0 && tabsClosed == 0) {
            removeCollaborationActivityMessageCard();
            return;
        }

        if (mCollaborationActivityPropertyModel == null) {
            mCollaborationActivityPropertyModel =
                    new CollaborationActivityMessageCardViewModel(
                            mActivity,
                            this::showRecentActivityOrDismissActivityMessageCard,
                            (unused) -> {
                                // TODO(crbug.com/391946087): this shouldn't be required once
                                // clearDirtyTabMessagesForCurrentGroup is fixed.
                                removeCollaborationActivityMessageCard();

                                dismissAllDirtyTabMessagesForCurrentGroup();
                            });
        }
        mCollaborationActivityPropertyModel.updateDescriptionText(mActivity, tabsAdded, tabsClosed);

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
            mDataSharingTabManager.showRecentActivity(mActivity, collaborationId);
        } else {
            // TODO(crbug.com/391946087): this shouldn't be required once
            // clearDirtyTabMessagesForCurrentGroup is fixed.
            removeCollaborationActivityMessageCard();
        }
        dismissAllDirtyTabMessagesForCurrentGroup();
    }

    private void updateUngroupBarText(int tabCount) {
        @StringRes int ungroupBarTextId = R.string.remove_tab_from_group;
        if (tabCount == 1) {
            boolean isMember = MemberRole.MEMBER == getMemberRole();
            ungroupBarTextId =
                    isMember
                            ? R.string.remove_last_tab_action_member
                            : R.string.remove_last_tab_action;
        }

        Resources res = mActivity.getResources();
        mModel.set(
                TabGridDialogProperties.DIALOG_UNGROUP_BAR_TEXT, res.getString(ungroupBarTextId));
    }

    private @MemberRole int getMemberRole() {
        if (!mCollaborationService.getServiceStatus().isAllowedToJoin()) return MemberRole.UNKNOWN;

        @Nullable
        String collaborationId = mTransitiveSharedGroupObserver.getCollaborationIdSupplier().get();
        if (!TabShareUtils.isCollaborationIdValid(collaborationId)) return MemberRole.UNKNOWN;

        return mCollaborationService.getCurrentUserRoleForGroup(collaborationId);
    }

    /** AppHeaderObserver implementation */
    @Override
    public void onAppHeaderStateChanged(AppHeaderState state) {
        mModel.set(TabGridDialogProperties.APP_HEADER_HEIGHT, state.getAppHeaderHeight());
    }
}
