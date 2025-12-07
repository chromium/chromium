// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static android.view.View.GONE;
import static android.view.View.VISIBLE;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.ui.listmenu.BasicListMenu.buildMenuDivider;
import static org.chromium.ui.listmenu.ListItemType.SUBMENU_HEADER;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.database.DataSetObserver;
import android.text.Editable;
import android.text.TextUtils;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.EditText;
import android.widget.ListAdapter;
import android.widget.ListView;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParamsUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupColorUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadataExtractor;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator.ColorPickerLayoutType;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerType;
import org.chromium.chrome.browser.tasks.tab_management.TabShareUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabStripReorderingHelper;
import org.chromium.chrome.browser.tasks.tab_management.TabUiUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.CollaborationServiceLeaveOrDeleteEntryPoint;
import org.chromium.components.collaboration.CollaborationServiceShareOrManageEntryPoint;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.tab_group_sync.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.EmptyTextWatcher;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;

import java.util.List;
import java.util.function.BiConsumer;
import java.util.function.Supplier;

/**
 * A coordinator for the context menu on the tab strip by long-pressing on the group titles. It is
 * responsible for creating a list of menu items, setting up the menu and displaying the menu.
 */
@NullMarked
public class TabGroupContextMenuCoordinator extends TabStripReorderingHelper<Token> {
    private final Context mContext;
    private @MonotonicNonNull View mContentView;
    private @MonotonicNonNull EditText mGroupTitleEditText;
    private @MonotonicNonNull ColorPickerCoordinator mColorPickerCoordinator;
    private TabGroupModelFilter mTabGroupModelFilter;
    private Token mTabGroupId;

    // Title currently modified by the user through the edit box. This does not include previously
    // updated or default title.
    private @Nullable String mCurrentModifiedTitle;
    private boolean mIsPresetTitleUsed;
    private final WindowAndroid mWindowAndroid;
    private final KeyboardVisibilityDelegate.KeyboardVisibilityListener mKeyboardVisibilityListener;

    @SuppressWarnings("HidingField")
    protected CollaborationService mCollaborationService;

    private final TabGroupModelFilterObserver mTabGroupModelFilterObserver =
            new TabGroupModelFilterObserver() {
                @Override
                public void didChangeTabGroupTitle(Token tabGroupId, @Nullable String newTitle) {
                    if (isMenuShowing() && mTabGroupId.equals(tabGroupId)) {
                        setExistingOrDefaultTitle(newTitle);
                    }
                }

                @Override
                public void didChangeTabGroupColor(
                        Token tabGroupId, @TabGroupColorId int newColor) {
                    if (isMenuShowing() && mTabGroupId.equals(tabGroupId)) {
                        setSelectedColorItem(newColor);
                    }
                }
            };

    private TabGroupContextMenuCoordinator(
            Supplier<TabModel> tabModelSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            MultiInstanceManager multiInstanceManager,
            WindowAndroid windowAndroid,
            @Nullable TabGroupSyncService tabGroupSyncService,
            DataSharingTabManager dataSharingTabManager,
            CollaborationService collaborationService,
            BiConsumer<Token, Boolean> reorderFunction) {
        super(
                R.layout.tab_strip_group_menu_layout,
                R.layout.tab_switcher_action_menu_layout,
                getMenuItemClickedCallback(
                        assumeNonNull(windowAndroid.getActivity().get()),
                        tabModelSupplier,
                        tabGroupModelFilter,
                        multiInstanceManager,
                        dataSharingTabManager),
                tabModelSupplier,
                multiInstanceManager,
                tabGroupSyncService,
                collaborationService,
                assumeNonNull(windowAndroid.getActivity().get()),
                reorderFunction);
        mTabGroupModelFilter = tabGroupModelFilter;
        mWindowAndroid = windowAndroid;
        mContext = windowAndroid.getActivity().get();
        mKeyboardVisibilityListener =
                isShowing -> {
                    if (!isShowing) updateTabGroupTitle();
                };
        mTabGroupModelFilter.addTabGroupObserver(mTabGroupModelFilterObserver);
        mCollaborationService = collaborationService;
    }

    /**
     * Creates the TabGroupContextMenuCoordinator object.
     *
     * @param tabModel The tab model. Should have a {@link Profile}.
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} to act on.
     * @param multiInstanceManager The {@link MultiInstanceManager} that may be used to move the
     *     group to another window.
     * @param windowAndroid The {@link WindowAndroid} current window.
     * @param dataSharingTabManager The {@link} DataSharingTabManager managing communication between
     *     UI and DataSharing services.
     */
    public static TabGroupContextMenuCoordinator createContextMenuCoordinator(
            TabModel tabModel,
            TabGroupModelFilter tabGroupModelFilter,
            MultiInstanceManager multiInstanceManager,
            WindowAndroid windowAndroid,
            DataSharingTabManager dataSharingTabManager,
            BiConsumer<Token, Boolean> reorderFunction) {
        Profile profile = assumeNonNull(tabModel.getProfile());

        @Nullable TabGroupSyncService tabGroupSyncService =
                profile.isOffTheRecord() ? null : TabGroupSyncServiceFactory.getForProfile(profile);

        CollaborationService collaborationService =
                CollaborationServiceFactory.getForProfile(profile);

        return new TabGroupContextMenuCoordinator(
                () -> tabModel,
                tabGroupModelFilter,
                multiInstanceManager,
                windowAndroid,
                tabGroupSyncService,
                dataSharingTabManager,
                collaborationService,
                reorderFunction);
    }

    @VisibleForTesting
    static OnItemClickedCallback<Token> getMenuItemClickedCallback(
            Activity activity,
            Supplier<TabModel> tabModelSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            MultiInstanceManager multiInstanceManager,
            DataSharingTabManager dataSharingTabManager) {
        return (menuId, tabGroupId, collaborationId, listViewTouchTracker) -> {
            int tabId = tabGroupModelFilter.getGroupLastShownTabId(tabGroupId);
            EitherGroupId eitherId = EitherGroupId.createLocalId(new LocalTabGroupId(tabGroupId));

            if (tabId == Tab.INVALID_TAB_ID) return;

            if (menuId == R.id.ungroup_tab) {
                TabUiUtils.ungroupTabGroup(tabGroupModelFilter, tabGroupId);
                RecordUserAction.record("MobileToolbarTabGroupMenu.Ungroup");
            } else if (menuId == R.id.close_tab_group) {
                boolean allowUndo = TabClosureParamsUtils.shouldAllowUndo(listViewTouchTracker);
                TabUiUtils.closeTabGroup(
                        tabGroupModelFilter,
                        tabId,
                        TabClosingSource.TABLET_TAB_STRIP,
                        allowUndo,
                        /* hideTabGroups= */ true,
                        /* didCloseCallback= */ null);
                RecordUserAction.record("MobileToolbarTabGroupMenu.CloseGroup");
            } else if (menuId == R.id.delete_tab_group) {
                boolean allowUndo = TabClosureParamsUtils.shouldAllowUndo(listViewTouchTracker);
                TabUiUtils.closeTabGroup(
                        tabGroupModelFilter,
                        tabId,
                        TabClosingSource.TABLET_TAB_STRIP,
                        allowUndo,
                        /* hideTabGroups= */ false,
                        /* didCloseCallback= */ null);
                RecordUserAction.record("MobileToolbarTabGroupMenu.DeleteGroup");
            } else if (menuId == R.id.open_new_tab_in_group) {
                TabGroupUtils.openUrlInGroup(
                        tabGroupModelFilter,
                        UrlConstants.NTP_URL,
                        tabId,
                        TabLaunchType.FROM_TAB_GROUP_UI);
                RecordUserAction.record("MobileToolbarTabGroupMenu.NewTabInGroup");
            } else if (menuId == R.id.move_to_other_window_menu_id) {
                if (MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ACTIVE)
                        == 1) {
                    RecordUserAction.record("MobileToolbarTabGroupMenu.MoveGroupToNewWindow");
                } else {
                    RecordUserAction.record("MobileToolbarTabGroupMenu.MoveGroupToAnotherWindow");
                }
                TabModel tabModel = tabModelSupplier.get();
                @Nullable TabGroupMetadata tabGroupMetadata =
                        TabGroupMetadataExtractor.extractTabGroupMetadata(
                                tabGroupModelFilter,
                                tabGroupModelFilter.getTabsInGroup(tabGroupId),
                                TabWindowManagerSingleton.getInstance().getIdForWindow(activity),
                                assumeNonNull(tabModel.getTabAt(tabModel.index())).getId(),
                                TabShareUtils.isCollaborationIdValid(collaborationId));
                if (tabGroupMetadata != null) {
                    multiInstanceManager.moveTabGroupToOtherWindow(
                            tabGroupMetadata, NewWindowAppSource.MENU);
                }
            } else if (menuId == R.id.share_group) {
                // Create the group share flow and display the share bottom sheet.
                dataSharingTabManager.createOrManageFlow(
                        eitherId,
                        CollaborationServiceShareOrManageEntryPoint
                                .ANDROID_TAB_GROUP_CONTEXT_MENU_SHARE,
                        /* createGroupFinishedCallback= */ null);
                RecordUserAction.record("MobileToolbarTabGroupMenu.ShareGroup");
            } else if (menuId == R.id.manage_sharing) {
                dataSharingTabManager.createOrManageFlow(
                        eitherId,
                        CollaborationServiceShareOrManageEntryPoint
                                .ANDROID_TAB_GROUP_CONTEXT_MENU_MANAGE,
                        /* createGroupFinishedCallback= */ null);
                RecordUserAction.record("MobileToolbarTabGroupMenu.ManageSharing");
            } else if (menuId == R.id.recent_activity
                    && TabShareUtils.isCollaborationIdValid(collaborationId)) {
                dataSharingTabManager.showRecentActivity(activity, collaborationId);
                RecordUserAction.record("MobileToolbarTabGroupMenu.RecentActivity");
            } else if (menuId == R.id.delete_shared_group) {
                dataSharingTabManager.leaveOrDeleteFlow(
                        eitherId,
                        CollaborationServiceLeaveOrDeleteEntryPoint
                                .ANDROID_TAB_GROUP_CONTEXT_MENU_DELETE);
                RecordUserAction.record("MobileToolbarTabGroupMenu.DeleteSharedGroup");
            } else if (menuId == R.id.leave_group) {
                dataSharingTabManager.leaveOrDeleteFlow(
                        eitherId,
                        CollaborationServiceLeaveOrDeleteEntryPoint
                                .ANDROID_TAB_GROUP_CONTEXT_MENU_LEAVE);
                RecordUserAction.record("MobileToolbarTabGroupMenu.LeaveSharedGroup");
            }
        };
    }

    /**
     * Show the context menu of the tab group.
     *
     * @param anchorViewRectProvider The context menu's anchor view rect provider. These are screen
     *     coordinates.
     * @param tabGroupId The tab group ID of the interacting tab group.
     */
    @Initializer
    public void showMenu(RectProvider anchorViewRectProvider, Token tabGroupId) {
        mTabGroupId = tabGroupId;
        createAndShowMenu(
                anchorViewRectProvider,
                tabGroupId,
                /* horizontalOverlapAnchor= */ true,
                /* verticalOverlapAnchor= */ false,
                /* animStyle= */ Resources.ID_NULL,
                HorizontalOrientation.LAYOUT_DIRECTION,
                assumeNonNull(mWindowAndroid.getActivity().get()));
        RecordUserAction.record("MobileToolbarTabGroupMenu.Shown");
    }

    @Override
    protected void buildCustomView(View contentView, boolean isIncognito) {
        mContentView = contentView;

        buildTitleEditor(mContentView, mContext, isIncognito);

        buildColorEditor(mContentView, mContext, isIncognito);
    }

    @Override
    protected void buildMenuActionItems(ModelList itemList, Token id) {
        boolean isIncognito = mTabModelSupplier.get().isIncognitoBranded();
        @Nullable String collaborationId = getCollaborationIdOrNull(id);
        boolean hasCollaborationData =
                TabShareUtils.isCollaborationIdValid(collaborationId)
                        && mCollaborationService.getServiceStatus().isAllowedToJoin();
        itemList.add(buildMenuDivider(isIncognito));

        itemList.add(
                new ListItemBuilder()
                        .withTitleRes(R.string.open_new_tab_in_group_context_menu_item)
                        .withMenuId(R.id.open_new_tab_in_group)
                        .withIsIncognito(isIncognito)
                        .build());

        if (!hasCollaborationData) {
            itemList.add(
                    new ListItemBuilder()
                            .withTitleRes(R.string.ungroup_tab_group_menu_item)
                            .withMenuId(R.id.ungroup_tab)
                            .withIsIncognito(isIncognito)
                            .build());
        }

        if (!isIncognito
                && mCollaborationService != null
                && mCollaborationService.getServiceStatus().isAllowedToCreate()
                && !hasCollaborationData) {
            itemList.add(
                    new ListItemBuilder()
                            .withTitleRes(R.string.share_tab_group_context_menu_item)
                            .withMenuId(R.id.share_group)
                            .build());
        }

        itemList.add(
                new ListItemBuilder()
                        .withTitleRes(R.string.tab_grid_dialog_toolbar_close_group)
                        .withMenuId(R.id.close_tab_group)
                        .withIsIncognito(isIncognito)
                        .build());

        if (MultiWindowUtils.isMultiInstanceApi31Enabled() && mMultiInstanceManager != null) {
            itemList.add(
                    createMoveToWindowItem(
                            id,
                            isIncognito,
                            R.plurals.move_group_to_another_window_context_menu_item,
                            R.id.move_to_other_window_menu_id));
        }
        List<MVCListAdapter.ListItem> reorderItems =
                createReorderItems(
                        id,
                        assumeNonNull(mContext).getString(R.string.move_tab_group_left),
                        mContext.getString(R.string.move_tab_group_right));
        // Need to check list is non-empty before calling addAll; otherwise we get assertion error.
        if (!reorderItems.isEmpty()) itemList.addAll(reorderItems);

        // Delete does not make sense for incognito since the tab group is not saved to sync.
        if ((mTabGroupSyncService != null) && !isIncognito && !hasCollaborationData) {
            itemList.add(buildMenuDivider(isIncognito));

            itemList.add(
                    new ListItemBuilder()
                            .withTitleRes(R.string.tab_grid_dialog_toolbar_delete_group)
                            .withMenuId(R.id.delete_tab_group)
                            .build());
        }
    }

    @Override
    public void buildCollaborationMenuItems(ModelList itemList, @MemberRole int memberRole) {
        if (memberRole != MemberRole.UNKNOWN) {
            int insertionIndex = getMenuItemIndex(itemList, R.id.close_tab_group);

            itemList.add(
                    insertionIndex++,
                    new ListItemBuilder()
                            .withTitleRes(R.string.tab_grid_dialog_toolbar_manage_sharing)
                            .withMenuId(R.id.manage_sharing)
                            .build());

            itemList.add(
                    insertionIndex++,
                    new ListItemBuilder()
                            .withTitleRes(R.string.tab_grid_dialog_toolbar_recent_activity)
                            .withMenuId(R.id.recent_activity)
                            .build());
        }

        if (memberRole == MemberRole.OWNER) {
            itemList.add(buildMenuDivider(/* isIncognito= */ false));

            itemList.add(
                    new ListItemBuilder()
                            .withTitleRes(R.string.tab_grid_dialog_toolbar_delete_group)
                            .withMenuId(R.id.delete_shared_group)
                            .build());
        } else if (memberRole == MemberRole.MEMBER) {
            itemList.add(buildMenuDivider(/* isIncognito= */ false));

            itemList.add(
                    new ListItemBuilder()
                            .withTitleRes(R.string.tab_grid_dialog_toolbar_leave_group)
                            .withMenuId(R.id.leave_group)
                            .build());
        }
        resizeMenu();
    }

    /**
     * Calculates and sets the ListView height to prevent collapse when nested in a ScrollView. The
     * ListView behaves like a LinearLayout and relies on the ScrollView for proper scrolling to
     * ensure scrolling for the custom views.
     */
    @Override
    protected void afterCreate() {
        assert mContentView != null : "Menu view should not be null";

        ListView listView = mContentView.findViewById(R.id.tab_group_action_menu_list);
        listView.setScrollContainer(false);

        ListAdapter listAdapter = listView.getAdapter();
        if (listAdapter == null) {
            return;
        }

        int totalHeight = 0;
        for (int i = 0; i < listAdapter.getCount(); i++) {
            View listItem = listAdapter.getView(i, null, listView);
            listItem.measure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
            totalHeight += listItem.getMeasuredHeight();
        }

        ViewGroup.LayoutParams params = listView.getLayoutParams();
        params.height = totalHeight + listView.getPaddingTop() + listView.getPaddingBottom();
        listView.setLayoutParams(params);

        listAdapter.registerDataSetObserver(
                new DataSetObserver() {
                    @Override
                    public void onChanged() {
                        boolean shouldShowTitleEditor =
                                (listAdapter.getItemViewType(0) != SUBMENU_HEADER);
                        if (mGroupTitleEditText != null) {
                            mGroupTitleEditText.setVisibility(
                                    shouldShowTitleEditor ? VISIBLE : GONE);
                        }
                        if (mColorPickerCoordinator != null) {
                            mColorPickerCoordinator
                                    .getContainerView()
                                    .setVisibility(shouldShowTitleEditor ? VISIBLE : GONE);
                        }
                    }
                });
    }

    private int getMenuItemIndex(ModelList itemList, int menuItemId) {
        for (int i = 0; i < itemList.size(); i++) {
            PropertyModel model = itemList.get(i).model;
            if (model.containsKeyEqualTo(ListMenuItemProperties.MENU_ITEM_ID, menuItemId)) {
                return i;
            }
        }
        return itemList.size();
    }

    @Override
    protected void onMenuDismissed() {
        // TODO(Crbug.com/360044398) Record user action dismiss without any action taken.
        updateTabGroupTitle();
        mWindowAndroid
                .getKeyboardDelegate()
                .removeKeyboardVisibilityListener(mKeyboardVisibilityListener);
    }

    @Override
    protected int getMenuWidth(int anchorViewWidthPx) {
        return getDimensionPixelSize(R.dimen.tab_strip_group_context_menu_max_width);
    }

    @Override
    protected @Nullable String getCollaborationIdOrNull(Token id) {
        return TabShareUtils.getCollaborationIdOrNull(id, mTabGroupSyncService);
    }

    @Override
    @RequiresNonNull("mMultiInstanceManager")
    protected void moveToNewWindow(Token groupId) {
        @Nullable TabGroupMetadata tabGroupMetadata = getTabGroupMetadata(groupId);
        if (tabGroupMetadata == null) return;
        RecordUserAction.record("MobileToolbarTabGroupMenu.MoveGroupToNewWindow");
        mMultiInstanceManager.moveTabGroupToNewWindow(tabGroupMetadata, NewWindowAppSource.MENU);
    }

    @Override
    @RequiresNonNull("mMultiInstanceManager")
    protected void moveToWindow(InstanceInfo instanceInfo, Token groupId) {
        @Nullable TabGroupMetadata tabGroupMetadata = getTabGroupMetadata(groupId);
        if (tabGroupMetadata == null) return;
        RecordUserAction.record("MobileToolbarTabGroupMenu.MoveGroupToAnotherWindow");
        mMultiInstanceManager.moveTabGroupToWindow(
                instanceInfo, tabGroupMetadata, TabList.INVALID_TAB_INDEX, NewWindowAppSource.MENU);
    }

    @Override
    protected boolean canItemMoveTowardStart(Token groupId) {
        TabModel tabModel = mTabModelSupplier.get();
        Tab firstTab = mTabGroupModelFilter.getTabsInGroup(groupId).get(0);
        int idx = tabModel.indexOf(firstTab);
        return idx > tabModel.findFirstNonPinnedTabIndex();
    }

    @Override
    protected boolean canItemMoveTowardEnd(Token groupId) {
        TabModel tabModel = mTabModelSupplier.get();
        List<Tab> tabs = mTabGroupModelFilter.getTabsInGroup(groupId);
        for (Tab tab : tabs) {
            if (tab.getIsPinned()) return false;
        }
        Tab lastTab = tabs.get(tabs.size() - 1);
        int idx = tabModel.indexOf(lastTab);
        return idx < tabModel.getCount() - 1;
    }

    private @Nullable TabGroupMetadata getTabGroupMetadata(Token groupId) {
        TabModel tabModel = mTabModelSupplier.get();
        @Nullable String collaborationId = getCollaborationIdOrNull(groupId);
        return TabGroupMetadataExtractor.extractTabGroupMetadata(
                mTabGroupModelFilter,
                mTabGroupModelFilter.getTabsInGroup(groupId),
                TabWindowManagerSingleton.getInstance()
                        .getIdForWindow(assumeNonNull(mWindowAndroid.getActivity().get())),
                assumeNonNull(tabModel.getTabAt(tabModel.index())).getId(),
                TabShareUtils.isCollaborationIdValid(collaborationId));
    }

    private void updateTabGroupColor() {
        if (mColorPickerCoordinator == null) return;
        @TabGroupColorId int newColor = mColorPickerCoordinator.getSelectedColorSupplier().get();
        if (TabUiUtils.updateTabGroupColor(mTabGroupModelFilter, mTabGroupId, newColor)) {
            RecordUserAction.record("MobileToolbarTabGroupMenu.ColorChanged");
        }
    }

    private void setSelectedColorItem(@TabGroupColorId int newColor) {
        if (mColorPickerCoordinator == null) return;
        mColorPickerCoordinator.setSelectedColorItem(newColor);
    }

    @VisibleForTesting
    void updateTabGroupTitle() {
        String newTitle = mCurrentModifiedTitle;
        if (newTitle == null) {
            return;
        } else if (TextUtils.isEmpty(newTitle) || newTitle.equals(getDefaultTitle())) {
            mTabGroupModelFilter.deleteTabGroupTitle(mTabGroupId);
            RecordUserAction.record("MobileToolbarTabGroupMenu.TitleReset");
            setExistingOrDefaultTitle(getDefaultTitle());
        } else if (TabUiUtils.updateTabGroupTitle(mTabGroupModelFilter, mTabGroupId, newTitle)) {
            RecordUserAction.record("MobileToolbarTabGroupMenu.TitleChanged");
        }
        mCurrentModifiedTitle = null;
    }

    private void setExistingOrDefaultTitle(@Nullable String s) {
        // Flip `IsPresetTitleUsed`to prevent `TextWatcher` from treating `#setText` as a title
        // update.
        mIsPresetTitleUsed = true;
        if (mGroupTitleEditText != null) mGroupTitleEditText.setText(s);
    }

    private String getDefaultTitle() {
        return TabGroupTitleUtils.getDefaultTitle(
                assumeNonNull(mContext), mTabGroupModelFilter.getTabCountForGroup(mTabGroupId));
    }

    // TODO(crbug.com/358689769): Enable live editing and updating of the group title.
    private void buildTitleEditor(View contentView, Context context, boolean isIncognito) {
        mGroupTitleEditText = contentView.findViewById(R.id.tab_group_title);

        // Set incognito style.
        if (isIncognito) {
            mGroupTitleEditText.setBackgroundTintList(
                    AppCompatResources.getColorStateList(
                            context, R.color.menu_edit_text_bg_tint_list_baseline));
            mGroupTitleEditText.setTextAppearance(
                    R.style.TextAppearance_TextLarge_Primary_Baseline_Light);
        }

        // Listen to title update as user types.
        mGroupTitleEditText.addTextChangedListener(
                new EmptyTextWatcher() {
                    @Override
                    public void afterTextChanged(Editable s) {
                        if (!mIsPresetTitleUsed) {
                            mCurrentModifiedTitle = s.toString();
                        }
                        mIsPresetTitleUsed = false;
                    }
                });

        setExistingOrDefaultTitle(
                TabGroupTitleUtils.getDisplayableTitle(context, mTabGroupModelFilter, mTabGroupId));

        // Add listener to group title EditText to update group title when keyboard starts hiding.
        mWindowAndroid
                .getKeyboardDelegate()
                .addKeyboardVisibilityListener(mKeyboardVisibilityListener);
    }

    private void buildColorEditor(View contentView, Context context, boolean isIncognito) {
        // Set horizontal padding to custom view to match list items.
        int horizontalPadding =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.color_picker_horizontal_padding);

        // TODO(crbug.com/357104424): Consider create ColorPickerCoordinator once during the first
        // call, and reuse it for subsequent calls.
        mColorPickerCoordinator =
                new ColorPickerCoordinator(
                        context,
                        TabGroupColorUtils.getTabGroupColorIdList(),
                        ((ViewStub) contentView.findViewById(R.id.color_picker_stub)).inflate(),
                        ColorPickerType.TAB_GROUP,
                        isIncognito,
                        ColorPickerLayoutType.DYNAMIC,
                        this::updateTabGroupColor);
        mColorPickerCoordinator
                .getContainerView()
                .setPadding(horizontalPadding, 0, horizontalPadding, 0);

        // The color picker should select the current color of the tab group when it is displayed.
        @TabGroupColorId
        int curGroupColor = mTabGroupModelFilter.getTabGroupColorWithFallback(mTabGroupId);
        mColorPickerCoordinator.setSelectedColorItem(curGroupColor);
    }

    @SuppressWarnings("NullAway")
    public void destroy() {
        if (mTabGroupModelFilter != null) {
            mTabGroupModelFilter.removeTabGroupObserver(mTabGroupModelFilterObserver);
            mTabGroupModelFilter = null;
        }
    }

    @Nullable EditText getGroupTitleEditTextForTesting() {
        return mGroupTitleEditText;
    }

    @Nullable ColorPickerCoordinator getColorPickerCoordinatorForTesting() {
        return mColorPickerCoordinator;
    }

    KeyboardVisibilityDelegate.KeyboardVisibilityListener
            getKeyboardVisibilityListenerForTesting() {
        return mKeyboardVisibilityListener;
    }

    void setGroupDataForTesting(Token tabGroupId) {
        mTabGroupId = tabGroupId;
    }

    void setTabGroupSyncServiceForTesting(TabGroupSyncService tabGroupSyncService) {
        mTabGroupSyncService = tabGroupSyncService;
    }
}
