// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;
import org.chromium.url.GURL;

/**
 * A coordinator for the recent activity UI. This includes (1) creating and managing bottom sheet,
 * and (2) building and showing the list of recent activity UI.
 */
@NullMarked
public class RecentActivityListCoordinator {
    /** Interface to fetch favicons for tabs to be used for data sharing UI. */
    public interface FaviconProvider {

        /**
         * Given a tab URL, fetches favicon to be shown from the favicon backend.
         *
         * @param tabUrl The URL of the tab.
         * @param faviconDrawableCallback The callback to be invoked when favicon drawable is ready.
         */
        void fetchFavicon(GURL tabUrl, Callback<Drawable> faviconDrawableCallback);

        /** Must be invoked to destroy the C++ side of the favicon helper used underneath. */
        void destroy();
    }

    /** Interface to fetch avatar for users from data sharing backend. */
    @FunctionalInterface
    public interface AvatarProvider {

        /**
         * Given a {@link GroupMember}, fetches their avatar from the data sharing backend.
         *
         * @param member The associated {@link GroupMember}. If null, returns a default fallback
         *     avatar.
         * @param avatarDrawableCallback The callback to be invoked when avatar is ready.
         */
        void getAvatarBitmap(
                @Nullable GroupMember member, Callback<Drawable> avatarDrawableCallback);
    }

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final ModelList mModelList;
    private final View mContentContainer;
    private final RecyclerView mContentRecyclerView;
    private final RecentActivityListMediator mMediator;
    private @Nullable RecentActivityBottomSheetContent mBottomSheetContent;
    private final Runnable mShowFullActivityRunnable;

    /**
     * Constructor.
     *
     * @param collaborationId The collaboration ID for which recent activities are to be shown.
     * @param context The context for creating views and loading resources.
     * @param bottomSheetController The controller for showing bottom sheet.
     * @param messagingBackendService The backend for providing the recent activity list.
     * @param faviconProvider The backend for providing favicon for URLs.
     * @param avatarProvider The backend for providing avatars for users.
     * @param recentActivityActionHandler Click event handler for activity rows.
     * @param showFullActivityRunnable Runnable to show the full activity log.
     */
    public RecentActivityListCoordinator(
            String collaborationId,
            Context context,
            BottomSheetController bottomSheetController,
            MessagingBackendService messagingBackendService,
            TabGroupSyncService tabGroupSyncService,
            FaviconProvider faviconProvider,
            AvatarProvider avatarProvider,
            RecentActivityActionHandler recentActivityActionHandler,
            Runnable showFullActivityRunnable) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mShowFullActivityRunnable = showFullActivityRunnable;
        mContentContainer =
                LayoutInflater.from(context)
                        .inflate(R.layout.recent_activity_bottom_sheet, /* root= */ null);
        PropertyModel propertyModel =
                new PropertyModel.Builder(RecentActivityContainerProperties.ALL_KEYS)
                        .with(
                                RecentActivityContainerProperties.MENU_CLICK_LISTENER,
                                createRecentActivityMenuButtonClickListener())
                        .build();
        PropertyModelChangeProcessor.create(
                propertyModel, mContentContainer, RecentActivityContainerViewBinder::bind);

        mModelList = new ModelList();
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(mModelList);
        adapter.registerType(
                0,
                new LayoutViewBuilder(R.layout.recent_activity_log_item),
                RecentActivityListViewBinder::bind);

        mContentRecyclerView = mContentContainer.findViewById(R.id.recent_activity_recycler_view);
        mContentRecyclerView.setAdapter(adapter);

        mBottomSheetController.addObserver(
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetClosed(int reason) {
                        faviconProvider.destroy();
                        mBottomSheetController.removeObserver(this);
                        mMediator.onBottomSheetClosed();
                    }
                });

        mMediator =
                new RecentActivityListMediator(
                        collaborationId,
                        context,
                        propertyModel,
                        mModelList,
                        messagingBackendService,
                        tabGroupSyncService,
                        faviconProvider,
                        avatarProvider,
                        recentActivityActionHandler,
                        this::closeBottomSheet);
    }

    private OnClickListener createRecentActivityMenuButtonClickListener() {
        return view -> {
            ListMenuButton menuView = view.findViewById(R.id.recent_activity_menu_button);
            ModelList modelList = new ModelList();
            modelList.add(
                    BrowserUiListMenuUtils.buildMenuListItem(
                            R.string.data_sharing_shared_tab_groups_activity,
                            R.id.see_full_activity,
                            0,
                            /* enabled= */ true));
            ListMenu.Delegate delegate =
                    (model) -> {
                        int textId = model.get(ListMenuItemProperties.TITLE_ID);
                        if (textId == R.string.data_sharing_shared_tab_groups_activity) {
                            mShowFullActivityRunnable.run();
                        }
                    };

            BasicListMenu listMenu =
                    BrowserUiListMenuUtils.getBasicListMenu(mContext, modelList, delegate);

            ListMenuDelegate listMenuDelegate =
                    new ListMenuDelegate() {
                        @Override
                        public ListMenu getListMenu() {
                            return listMenu;
                        }

                        @Override
                        public RectProvider getRectProvider(View listMenuButton) {
                            ViewRectProvider rectProvider = new ViewRectProvider(listMenuButton);
                            rectProvider.setIncludePadding(true);

                            int handleBarHeight =
                                    mContentContainer.findViewById(R.id.handlebar).getHeight();
                            int buttonHeight = listMenuButton.getHeight();
                            rectProvider.setInsetPx(0, handleBarHeight + buttonHeight, 0, 0);
                            return rectProvider;
                        }
                    };

            menuView.setMenuMaxWidth(
                    view.getResources().getDimensionPixelSize(R.dimen.recent_activity_menu_width));
            menuView.setDelegate(listMenuDelegate);
            menuView.tryToFitLargestItem(true);
            menuView.showMenu();
        };
    }

    /**
     * Called to create the UI creation. Builds the list UI and after the list is created pulls up
     * the bottom sheet UI.
     */
    public void requestShowUI() {
        mMediator.requestShowUI(this::showBottomSheet);
    }

    private void showBottomSheet() {
        mBottomSheetContent = new RecentActivityBottomSheetContent(mContentContainer);
        mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true);
    }

    private void closeBottomSheet() {
        assumeNonNull(mBottomSheetContent);
        mBottomSheetController.hideContent(mBottomSheetContent, /* animate= */ false);
    }
}
