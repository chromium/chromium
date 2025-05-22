// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.ui.listmenu.BasicListMenu.buildMenuDivider;

import android.app.Activity;
import android.content.Context;
import android.text.Editable;
import android.text.TextUtils;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.EditText;
import android.widget.ListAdapter;
import android.widget.ListView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosureParamsUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupColorUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadataExtractor;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator.ColorPickerLayoutType;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerType;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupOverflowMenuCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabShareUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
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
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.EmptyTextWatcher;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;

/**
 * A coordinator for the context menu on the tab strip by long-pressing on the group titles. It is
 * responsible for creating a list of menu items, setting up the menu and displaying the menu.
 */
public class TabGroupContextMenuCoordinator extends TabGroupOverflowMenuCoordinator {
    private View mContentView;
    private EditText mGroupTitleEditText;
    private ColorPickerCoordinator mColorPickerCoordinator;
    private TabGroupModelFilter mTabGroupModelFilter;
    private Token mTabGroupId;
    private int mGroupRootId;
    private Context mContext;

    // Title currently modified by the user through the edit box. This does not include previously
    // updated or default title.
    private String mCurrentModifiedTitle;
    private boolean mIsPresetTitleUsed;
    private final WindowAndroid mWindowAndroid;
    private final KeyboardVisibilityDelegate.KeyboardVisibilityListener mKeyboardVisibilityListener;
    protected CollaborationService mCollaborationService;
    private final TabGroupModelFilterObserver mTabGroupModelFilterObserver =
            new TabGroupModelFilterObserver() {
                @Override
                public void didChangeTabGroupTitle(int rootId, String newTitle) {
                    if (isMenuShowing() && rootId == mGroupRootId) {
                        setExistingOrDefaultTitle(newTitle);
                    }
                }

                @Override
                public void didChangeTabGroupColor(int rootId, @TabGroupColorId int newColor) {
                    if (isMenuShowing() && rootId == mGroupRootId) {
                        setSelectedColorItem(newColor);
                    }
                }
            };

    private TabGroupContextMenuCoordinator(
            Supplier<TabModel> tabModelSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            MultiInstanceManager multiInstanceManager,
            WindowAndroid windowAndroid,
            TabGroupSyncService tabGroupSyncService,
            DataSharingTabManager dataSharingTabManager,
            CollaborationService collaborationService) {
        super(
                R.layout.tab_strip_group_menu_layout,
                getMenuItemClickedCallback(
                        windowAndroid.getActivity().get(),
                        tabModelSupplier,
                        tabGroupModelFilter,
                        multiInstanceManager,
                        dataSharingTabManager),
                tabModelSupplier,
                tabGroupSyncService,
                collaborationService,
                windowAndroid.getActivity().get());
        mTabGroupModelFilter = tabGroupModelFilter;
        mWindowAndroid = windowAndroid;
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
     * @param tabModel The tab model.
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
            DataSharingTabManager dataSharingTabManager) {
        Profile profile = tabModel.getProfile();
        @Nullable
        TabGroupSyncService tabGroupSyncService =
                profile.isOffTheRecord() ? null : TabGroupSyncServiceFactory.getForProfile(profile);
        @NonNull
        CollaborationService collaborationService =
                CollaborationServiceFactory.getForProfile(profile);

        return new TabGroupContextMenuCoordinator(
                () -> tabModel,
                tabGroupModelFilter,
                multiInstanceManager,
                windowAndroid,
                tabGroupSyncService,
                dataSharingTabManager,
                collaborationService);
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

            if (menuId == org.chromium.chrome.R.id.ungroup_tab) {
                TabUiUtils.ungroupTabGroup(tabGroupModelFilter, tabGroupId);
                RecordUserAction.record("MobileToolbarTabGroupMenu.Ungroup");
            } else if (menuId == org.chromium.chrome.R.id.close_tab_group) {
                boolean allowUndo = TabClosureParamsUtils.shouldAllowUndo(listViewTouchTracker);
                TabUiUtils.closeTabGroup(
                        tabGroupModelFilter,
                        tabId,
                        allowUndo,
                        /* hideTabGroups= */ true,
                        /* didCloseCallback= */ null);
                RecordUserAction.record("MobileToolbarTabGroupMenu.CloseGroup");
            } else if (menuId == org.chromium.chrome.R.id.delete_tab_group) {
                boolean allowUndo = TabClosureParamsUtils.shouldAllowUndo(listViewTouchTracker);
                TabUiUtils.closeTabGroup(
                        tabGroupModelFilter,
                        tabId,
                        allowUndo,
                        /* hideTabGroups= */ false,
                        /* didCloseCallback= */ null);
                RecordUserAction.record("MobileToolbarTabGroupMenu.DeleteGroup");
            } else if (menuId == org.chromium.chrome.R.id.open_new_tab_in_group) {
                TabGroupUtils.openUrlInGroup(
                        tabGroupModelFilter,
                        UrlConstants.NTP_URL,
                        tabId,
                        TabLaunchType.FROM_TAB_GROUP_UI);
                RecordUserAction.record("MobileToolbarTabGroupMenu.NewTabInGroup");
            } else if (menuId == R.id.move_to_other_window_menu_id) {
                if (MultiWindowUtils.getInstanceCount() == 1) {
                    RecordUserAction.record("MobileToolbarTabGroupMenu.MoveGroupToNewWindow");
                } else {
                    RecordUserAction.record("MobileToolbarTabGroupMenu.MoveGroupToAnotherWindow");
                }
                TabModel tabModel = tabModelSupplier.get();
                TabGroupMetadata tabGroupMetadata =
                        TabGroupMetadataExtractor.extractTabGroupMetadata(
                                tabGroupModelFilter.getTabsInGroup(tabGroupId),
                                TabWindowManagerSingleton.getInstance().getIdForWindow(activity),
                                tabModel.getTabAt(tabModel.index()).getId(),
                                TabShareUtils.isCollaborationIdValid(collaborationId));
                multiInstanceManager.moveTabGroupToOtherWindow(tabGroupMetadata);
            } else if (menuId == org.chromium.chrome.R.id.share_group) {
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
            } else if (menuId == R.id.recent_activity) {
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
    public void showMenu(RectProvider anchorViewRectProvider, Token tabGroupId) {
        mTabGroupId = tabGroupId;
        mGroupRootId = mTabGroupModelFilter.getRootIdFromTabGroupId(tabGroupId);
        createAndShowMenu(
                anchorViewRectProvider,
                tabGroupId,
                /* horizontalOverlapAnchor= */ true,
                /* verticalOverlapAnchor= */ false,
                /* animStyle= */ ResourcesCompat.ID_NULL,
                HorizontalOrientation.LAYOUT_DIRECTION,
                mWindowAndroid.getActivity().get());
        RecordUserAction.record("MobileToolbarTabGroupMenu.Shown");
    }

    @Override
    protected void buildCustomView(View contentView, boolean isIncognito) {
        mContentView = contentView;
        mContext = contentView.getContext();

        buildTitleEditor(isIncognito);

        buildColorEditor(isIncognito);
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
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                        R.string.open_new_tab_in_group_context_menu_item,
                        R.id.open_new_tab_in_group,
                        isIncognito,
                        /* enabled= */ true));

        if (!hasCollaborationData) {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                            R.string.ungroup_tab_group_menu_item,
                            R.id.ungroup_tab,
                            isIncognito,
                            /* enabled= */ true));
        }

        if (!isIncognito
                && mCollaborationService != null
                && mCollaborationService.getServiceStatus().isAllowedToCreate()
                && !hasCollaborationData) {
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItem(
                            R.string.share_tab_group_context_menu_item,
                            R.id.share_group,
                            /* startIconId= */ 0,
                            /* enabled= */ true));
        }

        itemList.add(
                BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                        R.string.tab_grid_dialog_toolbar_close_group,
                        R.id.close_tab_group,
                        isIncognito,
                        /* enabled= */ true));

        if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            // TODO(crbug.com/417272356): Update text; Currently shows "Move to new window" instead
            //  of "Move _group_ to new window."
            Activity activity = mWindowAndroid.getActivity().get();
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItemWithIncognitoBranding(
                            activity.getResources()
                                    .getQuantityString(
                                            R.plurals
                                                    .move_group_to_another_window_context_menu_item,
                                            MultiWindowUtils.getInstanceCount()),
                            R.id.move_to_other_window_menu_id,
                            isIncognito,
                            /* enabled= */ true));
        }

        // Delete does not make sense for incognito since the tab group is not saved to sync.
        if ((mTabGroupSyncService != null) && !isIncognito && !hasCollaborationData) {
            itemList.add(buildMenuDivider(isIncognito));
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItem(
                            R.string.tab_grid_dialog_toolbar_delete_group,
                            R.id.delete_tab_group,
                            /* startIconId= */ 0,
                            /* enabled= */ true));
        }
    }

    @Override
    public void buildCollaborationMenuItems(ModelList itemList, @MemberRole int memberRole) {
        if (memberRole != MemberRole.UNKNOWN) {
            int insertionIndex = getMenuItemIndex(itemList, R.id.close_tab_group);
            itemList.add(
                    insertionIndex++,
                    BrowserUiListMenuUtils.buildMenuListItem(
                            R.string.tab_grid_dialog_toolbar_manage_sharing,
                            R.id.manage_sharing,
                            /* startIconId= */ 0,
                            /* enabled= */ true));
            itemList.add(
                    insertionIndex++,
                    BrowserUiListMenuUtils.buildMenuListItem(
                            R.string.tab_grid_dialog_toolbar_recent_activity,
                            R.id.recent_activity,
                            /* startIconId= */ 0,
                            /* enabled= */ true));
        }

        if (memberRole == MemberRole.OWNER) {
            itemList.add(buildMenuDivider(/* isIncognito= */ false));
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItem(
                            R.string.tab_grid_dialog_toolbar_delete_group,
                            R.id.delete_shared_group,
                            /* startIconId= */ 0,
                            /* enabled= */ true));
        } else if (memberRole == MemberRole.MEMBER) {
            itemList.add(buildMenuDivider(/* isIncognito= */ false));
            itemList.add(
                    BrowserUiListMenuUtils.buildMenuListItem(
                            R.string.tab_grid_dialog_toolbar_leave_group,
                            R.id.leave_group,
                            /* startIconId= */ 0,
                            /* enabled= */ true));
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
    }

    private int getMenuItemIndex(ModelList itemList, int menuItemId) {
        for (int i = 0; i < itemList.size(); i++) {
            PropertyModel model = itemList.get(i).model;
            if (model.containsKey(ListMenuItemProperties.MENU_ITEM_ID)
                    && model.get(ListMenuItemProperties.MENU_ITEM_ID) == menuItemId) {
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

    private void updateTabGroupColor() {
        @TabGroupColorId int newColor = mColorPickerCoordinator.getSelectedColorSupplier().get();
        if (TabUiUtils.updateTabGroupColor(mTabGroupModelFilter, mGroupRootId, newColor)) {
            RecordUserAction.record("MobileToolbarTabGroupMenu.ColorChanged");
        }
    }

    private void setSelectedColorItem(@TabGroupColorId int newColor) {
        mColorPickerCoordinator.setSelectedColorItem(newColor);
    }

    @VisibleForTesting
    void updateTabGroupTitle() {
        String newTitle = mCurrentModifiedTitle;
        if (newTitle == null) {
            return;
        } else if (TextUtils.isEmpty(newTitle) || newTitle.equals(getDefaultTitle())) {
            mTabGroupModelFilter.deleteTabGroupTitle(mGroupRootId);
            RecordUserAction.record("MobileToolbarTabGroupMenu.TitleReset");
            setExistingOrDefaultTitle(getDefaultTitle());
        } else if (TabUiUtils.updateTabGroupTitle(mTabGroupModelFilter, mGroupRootId, newTitle)) {
            RecordUserAction.record("MobileToolbarTabGroupMenu.TitleChanged");
        }
        mCurrentModifiedTitle = null;
    }

    private void setExistingOrDefaultTitle(String s) {
        // Flip `IsPresetTitleUsed`to prevent `TextWatcher` from treating `#setText` as a title
        // update.
        mIsPresetTitleUsed = true;
        mGroupTitleEditText.setText(s);
    }

    private String getDefaultTitle() {
        return TabGroupTitleUtils.getDefaultTitle(
                mContext, mTabGroupModelFilter.getTabCountForGroup(mTabGroupId));
    }

    // TODO(crbug.com/358689769): Enable live editing and updating of the group title.
    private void buildTitleEditor(boolean isIncognito) {
        mGroupTitleEditText = mContentView.findViewById(R.id.tab_group_title);

        // Set incognito style.
        if (isIncognito) {
            mGroupTitleEditText.setBackgroundTintList(
                    AppCompatResources.getColorStateList(
                            mContext,
                            org.chromium.chrome.R.color.menu_edit_text_bg_tint_list_baseline));
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

        // Set the initial text to the existing group title, defaulting to "N tabs" if no title name
        // is set.
        String curGroupTitle = mTabGroupModelFilter.getTabGroupTitle(mGroupRootId);
        if (curGroupTitle == null || curGroupTitle.isEmpty()) {
            setExistingOrDefaultTitle(getDefaultTitle());
        } else {
            setExistingOrDefaultTitle(curGroupTitle);
        }

        // Add listener to group title EditText to update group title when keyboard starts hiding.
        mWindowAndroid
                .getKeyboardDelegate()
                .addKeyboardVisibilityListener(mKeyboardVisibilityListener);
    }

    private void buildColorEditor(boolean isIncognito) {
        // Set horizontal padding to custom view to match list items.
        int horizontalPadding =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.list_menu_item_horizontal_padding);

        // TODO(crbug.com/357104424): Consider create ColorPickerCoordinator once during the first
        // call, and reuse it for subsequent calls.
        mColorPickerCoordinator =
                new ColorPickerCoordinator(
                        mContext,
                        TabGroupColorUtils.getTabGroupColorIdList(),
                        ((ViewStub) mContentView.findViewById(R.id.color_picker_stub)).inflate(),
                        ColorPickerType.TAB_GROUP,
                        isIncognito,
                        ColorPickerLayoutType.DYNAMIC,
                        this::updateTabGroupColor);
        mColorPickerCoordinator
                .getContainerView()
                .setPadding(horizontalPadding, 0, horizontalPadding, 0);

        // The color picker should select the current color of the tab group when it is displayed.
        @TabGroupColorId
        int curGroupColor = mTabGroupModelFilter.getTabGroupColorWithFallback(mGroupRootId);
        mColorPickerCoordinator.setSelectedColorItem(curGroupColor);
    }

    public void destroy() {
        if (mTabGroupModelFilter != null) {
            mTabGroupModelFilter.removeTabGroupObserver(mTabGroupModelFilterObserver);
            mTabGroupModelFilter = null;
        }
    }

    EditText getGroupTitleEditTextForTesting() {
        return mGroupTitleEditText;
    }

    ColorPickerCoordinator getColorPickerCoordinatorForTesting() {
        return mColorPickerCoordinator;
    }

    KeyboardVisibilityDelegate.KeyboardVisibilityListener
            getKeyboardVisibilityListenerForTesting() {
        return mKeyboardVisibilityListener;
    }

    void setGroupDataForTesting(int id, Token tabGroupId) {
        mGroupRootId = id;
        mTabGroupId = tabGroupId;
    }
}
