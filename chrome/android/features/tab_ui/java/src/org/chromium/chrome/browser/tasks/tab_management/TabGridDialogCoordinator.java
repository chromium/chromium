// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.PAGE_KEY_LISTENER;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Rect;
import android.util.Size;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;
import android.widget.PopupWindow;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.LayoutRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesConfig;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabContentManagerThumbnailProvider;
import org.chromium.chrome.browser.tabmodel.TabGroupColorUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator.ColorPickerLayoutType;
import org.chromium.chrome.browser.tasks.tab_management.TabGridDialogMediator.AnimationSourceViewProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.CreationMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorController;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.GridCardOnClickListenerProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabGroupColorChangeActionType;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarThrottle;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.List;
import java.util.function.Supplier;

/**
 * A coordinator for TabGridDialog component. Manages the communication with {@link
 * TabListCoordinator} as well as the life-cycle of shared component objects.
 */
@NullMarked
public class TabGridDialogCoordinator implements TabGridDialogMediator.DialogController {
    static final String COMPONENT_NAME_PREFIX = "TabGridDialog";
    private static final String FROM_STRIP_COMPONENT_NAME = COMPONENT_NAME_PREFIX + "FromStrip";
    private static final String IN_SWITCHER_COMPONENT_NAME = COMPONENT_NAME_PREFIX + "InSwitcher";

    private final String mComponentName;
    private final TabListCoordinator mTabListCoordinator;
    private final TabGridDialogMediator mMediator;
    private final PropertyModel mModel;
    private final PropertyModelChangeProcessor mModelChangeProcessor;
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private final Activity mActivity;
    private final ObservableSupplier<@Nullable TabGroupModelFilter>
            mCurrentTabGroupModelFilterSupplier;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final ModalDialogManager mModalDialogManager;
    private final TabListOnScrollListener mTabListOnScrollListener = new TabListOnScrollListener();
    private final BottomSheetController mBottomSheetController;
    private final UndoBarThrottle mUndoBarThrottle;
    private @Nullable final TabLabeller mTabLabeller;
    private final ObservableSupplierImpl<Boolean> mShowingOrAnimationSupplier =
            new ObservableSupplierImpl<>(false);
    private final ObservableSupplierImpl<@Nullable Token> mCurrentTabGroupId =
            new ObservableSupplierImpl<>();
    private final TabContentManager mTabContentManager;
    private final @Nullable SnackbarManager mSnackbarManager;
    private final @Nullable TabSwitcherResetHandler mTabSwitcherResetHandler;
    private final TabGridDialogView mDialogView;
    private final Callback<@Nullable View> mAttachViewCallback;

    private @Nullable TabListEditorCoordinator mTabListEditorCoordinator;
    private @Nullable ColorPickerCoordinator mColorPickerCoordinator;
    private @Nullable SharedImageTilesCoordinator mSharedImageTilesCoordinator;
    private @Nullable AnchoredPopupWindow mColorIconPopupWindow;
    private @Nullable Integer mUndoBarThrottleToken;

    TabGridDialogCoordinator(
            Activity activity,
            BrowserControlsStateProvider browserControlsStateProvider,
            BottomSheetController bottomSheetController,
            DataSharingTabManager dataSharingTabManager,
            ObservableSupplier<@Nullable TabGroupModelFilter> currentTabGroupModelFilterSupplier,
            TabContentManager tabContentManager,
            @Nullable TabSwitcherResetHandler resetHandler,
            @Nullable GridCardOnClickListenerProvider gridCardOnClickListenerProvider,
            @Nullable AnimationSourceViewProvider animationSourceViewProvider,
            ScrimManager scrimManager,
            ModalDialogManager modalDialogManager,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            UndoBarThrottle undoBarThrottle,
            ObservableSupplier<TabBookmarker> tabBookmarkerSupplier,
            Supplier<ShareDelegate> shareDelegateSupplier,
            Callback<@Nullable View> attachViewCallback) {
        try (TraceEvent e = TraceEvent.scoped("TabGridDialogCoordinator.constructor")) {
            mActivity = activity;
            mComponentName =
                    animationSourceViewProvider == null
                            ? FROM_STRIP_COMPONENT_NAME
                            : IN_SWITCHER_COMPONENT_NAME;
            mBrowserControlsStateProvider = browserControlsStateProvider;
            mModalDialogManager = modalDialogManager;
            mCurrentTabGroupModelFilterSupplier = currentTabGroupModelFilterSupplier;
            mTabContentManager = tabContentManager;
            mTabSwitcherResetHandler = resetHandler;
            mUndoBarThrottle = undoBarThrottle;
            mAttachViewCallback = attachViewCallback;

            mDialogView =
                    (TabGridDialogView)
                            LayoutInflater.from(mActivity)
                                    .inflate(R.layout.tab_grid_dialog_layout, null);
            mDialogView.setLayoutParams(
                    new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
            mDialogView.setupScrimManager(scrimManager);

            TabGroupModelFilter currentTabGroupModelFilter =
                    mCurrentTabGroupModelFilterSupplier.get();
            assumeNonNull(currentTabGroupModelFilter);
            Profile originalProfile =
                    assumeNonNull(currentTabGroupModelFilter.getTabModel().getProfile())
                            .getOriginalProfile();

            CollaborationService collaborationService =
                    CollaborationServiceFactory.getForProfile(originalProfile);
            ServiceStatus serviceStatus = collaborationService.getServiceStatus();
            boolean isDataSharingAndroidEnabled = serviceStatus.isAllowedToJoin();

            mModel =
                    new PropertyModel.Builder(TabGridDialogProperties.ALL_KEYS)
                            .with(
                                    TabGridDialogProperties.BROWSER_CONTROLS_STATE_PROVIDER,
                                    mBrowserControlsStateProvider)
                            .with(
                                    TabGridDialogProperties.COLOR_ICON_CLICK_LISTENER,
                                    getColorIconClickListener())
                            .build();

            if (!activity.isDestroyed() && !activity.isFinishing()) {
                mSnackbarManager =
                        new SnackbarManager(activity, mDialogView.getSnackBarContainer(), null);
            } else {
                mSnackbarManager = null;
            }
            mBottomSheetController = bottomSheetController;

            if (isDataSharingAndroidEnabled) {
                DataSharingService dataSharingService =
                        DataSharingServiceFactory.getForProfile(originalProfile);

                @ColorInt
                int backgroundColor =
                        TabUiThemeProvider.getTabGridDialogBackgroundColor(
                                mDialogView.getContext(), /* isIncognito= */ false);
                SharedImageTilesConfig config =
                        SharedImageTilesConfig.Builder.createForButton(activity)
                                .setBorderColor(backgroundColor)
                                .build();
                mSharedImageTilesCoordinator =
                        new SharedImageTilesCoordinator(
                                activity, config, dataSharingService, collaborationService);
            }

            Runnable showColorPickerPopupRunnable =
                    () -> {
                        showColorPickerPopup(mDialogView.findViewById(R.id.tab_group_color_icon));
                    };

            mMediator =
                    new TabGridDialogMediator(
                            activity,
                            this,
                            mModel,
                            currentTabGroupModelFilterSupplier,
                            resetHandler,
                            this::getRecyclerViewPosition,
                            animationSourceViewProvider,
                            mSnackbarManager,
                            mBottomSheetController,
                            mSharedImageTilesCoordinator,
                            dataSharingTabManager,
                            mComponentName,
                            showColorPickerPopupRunnable,
                            modalDialogManager,
                            desktopWindowStateManager,
                            tabBookmarkerSupplier,
                            shareDelegateSupplier);

            // TODO(crbug.com/40662311) : Remove the inline mode logic here, make the constructor to
            // take in a mode parameter instead.
            mTabListCoordinator =
                    new TabListCoordinator(
                            TabListMode.GRID,
                            activity,
                            mBrowserControlsStateProvider,
                            mModalDialogManager,
                            currentTabGroupModelFilterSupplier,
                            new TabContentManagerThumbnailProvider(tabContentManager),
                            /* actionOnRelatedTabs= */ false,
                            dataSharingTabManager,
                            gridCardOnClickListenerProvider,
                            mMediator.getTabGridDialogHandler(),
                            TabProperties.TabActionState.CLOSABLE,
                            /* selectionDelegateProvider= */ null,
                            /* priceWelcomeMessageControllerSupplier= */ null,
                            mDialogView,
                            /* attachToParent= */ false,
                            mComponentName,
                            /* onModelTokenChange= */ null,
                            /* emptyViewParent= */ null,
                            /* emptyImageResId= */ Resources.ID_NULL,
                            /* emptyHeadingStringResId= */ Resources.ID_NULL,
                            /* emptySubheadingStringResId= */ Resources.ID_NULL,
                            /* onTabGroupCreation= */ null,
                            /* allowDragAndDrop= */ true,
                            /* tabSwitcherDragHandler= */ null,
                            /* undoBarExplicitTrigger= */ null,
                            mSnackbarManager,
                            TabListEditorCoordinator.UNLIMITED_SELECTION);
            mTabListCoordinator.setOnLongPressTabItemEventListener(mMediator);
            mTabListCoordinator.registerItemType(
                    UiType.COLLABORATION_ACTIVITY_MESSAGE,
                    new LayoutViewBuilder<>(R.layout.tab_grid_message_card_item),
                    MessageCardViewBinder::bind);

            mTabListOnScrollListener
                    .getYOffsetNonZeroSupplier()
                    .addObserver(
                            (showHairline) ->
                                    mModel.set(
                                            TabGridDialogProperties.HAIRLINE_VISIBILITY,
                                            showHairline));
            TabListRecyclerView recyclerView = mTabListCoordinator.getContainerView();
            recyclerView.addOnScrollListener(mTabListOnScrollListener);

            @LayoutRes
            int toolbar_res_id =
                    isDataSharingAndroidEnabled
                            ? R.layout.tab_grid_dialog_toolbar_two_row
                            : R.layout.tab_grid_dialog_toolbar;
            TabGridDialogToolbarView toolbarView =
                    (TabGridDialogToolbarView)
                            LayoutInflater.from(activity)
                                    .inflate(toolbar_res_id, recyclerView, false);
            if (isDataSharingAndroidEnabled) {
                FrameLayout imageTilesContainer =
                        toolbarView.findViewById(R.id.image_tiles_container);
                assumeNonNull(mSharedImageTilesCoordinator);
                TabUiUtils.attachSharedImageTilesCoordinatorToFrameLayout(
                        mSharedImageTilesCoordinator, imageTilesContainer);
            }

            mModelChangeProcessor =
                    PropertyModelChangeProcessor.create(
                            mModel,
                            new TabGridDialogViewBinder.ViewHolder(
                                    toolbarView, recyclerView, mDialogView),
                            TabGridDialogViewBinder::bind);
            mBackPressChangedSupplier.set(isVisible());
            mModel.addObserver((source, key) -> mBackPressChangedSupplier.set(isVisible()));

            // This is always created post-native so calling these immediately is safe.
            // TODO(crbug.com/40894893): Consider inlining these behaviors in their respective
            // constructors if possible.
            mMediator.initWithNative(
                    LazyOneshotSupplier.fromSupplier(this::getTabListEditorController));
            mTabListCoordinator.initWithNative(originalProfile);

            if (isDataSharingAndroidEnabled) {
                DataSharingService dataSharingService =
                        DataSharingServiceFactory.getForProfile(originalProfile);
                mTabLabeller =
                        new TabLabeller(
                                originalProfile,
                                activity,
                                dataSharingService.getUiDelegate(),
                                mTabListCoordinator.getTabListNotificationHandler(),
                                mCurrentTabGroupId);
            } else {
                mTabLabeller = null;
            }
        }
    }

    /** Interface to handle Ctrl+Shift+PageUp or Ctrl+Shift+PageDown key press events. */
    /* package */ interface TabPageKeyListener {
        /**
         * Invoked when a valid key combination is detected.
         *
         * @param eventData The {@link TabKeyEventData}.
         */
        void onPageKeyEvent(TabKeyEventData eventData);
    }

    void setPageKeyEvent(TabPageKeyListener listener) {
        mModel.set(PAGE_KEY_LISTENER, listener::onPageKeyEvent);
    }

    RecyclerViewPosition getRecyclerViewPosition() {
        return mTabListCoordinator.getRecyclerViewPosition();
    }

    private TabListEditorController getTabListEditorController() {
        if (mTabListEditorCoordinator == null) {
            assert mSnackbarManager != null
                    : "SnackbarManager should have been created or the activity was already"
                            + " finishing.";

            ViewGroup container = mDialogView.findViewById(R.id.dialog_container_view);
            mTabListEditorCoordinator =
                    new TabListEditorCoordinator(
                            mActivity,
                            container,
                            container,
                            mBrowserControlsStateProvider,
                            mCurrentTabGroupModelFilterSupplier,
                            mTabContentManager,
                            mTabListCoordinator::setRecyclerViewPosition,
                            TabListMode.GRID,
                            /* displayGroups= */ false,
                            mSnackbarManager,
                            mBottomSheetController,
                            TabProperties.TabActionState.SELECTABLE,
                            /* gridCardOnClickListenerProvider= */ null,
                            mModalDialogManager,
                            // Parent container handles desktop window state.
                            /* desktopWindowStateManager= */ null,
                            /* edgeToEdgeSupplier= */ null,
                            CreationMode.DIALOG,
                            /* undoBarExplicitTrigger= */ null,
                            /* componentName= */ null,
                            TabListEditorCoordinator.UNLIMITED_SELECTION);
        }

        return mTabListEditorCoordinator.getController();
    }

    private View.OnClickListener getColorIconClickListener() {
        return (view) -> {
            showColorPickerPopup(view);
            TabUiMetricsHelper.recordTabGroupColorChangeActionMetrics(
                    TabGroupColorChangeActionType.VIA_COLOR_ICON);
        };
    }

    private void showColorPickerPopup(View anchorView) {
        PopupWindow.OnDismissListener onDismissListener =
                new PopupWindow.OnDismissListener() {
                    @Override
                    public void onDismiss() {
                        assumeNonNull(mColorPickerCoordinator);
                        mMediator.setSelectedTabGroupColor(
                                mColorPickerCoordinator.getSelectedColorSupplier().get());

                        // Only require a refresh of the tab list if accessed from the GTS,
                        // skip if this is reached from the tab strip as the color will
                        // refresh upon re-entering the tab switcher.
                        if (mTabSwitcherResetHandler != null) {
                            // Refresh the TabSwitcher's tab list to reflect the last
                            // selected color in the color picker when it is dismissed. This
                            // call will be invoked for both Grid and List modes on the GTS.
                            TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
                            assumeNonNull(filter);
                            mTabSwitcherResetHandler.resetWithListOfTabs(
                                    filter.getRepresentativeTabList());
                        }
                    }
                };

        List<Integer> colors = TabGroupColorUtils.getTabGroupColorIdList();
        mColorPickerCoordinator =
                new ColorPickerCoordinator(
                        mActivity,
                        colors,
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.tab_group_color_picker_container, null),
                        ColorPickerType.TAB_GROUP,
                        mModel.get(TabGridDialogProperties.IS_INCOGNITO),
                        ColorPickerLayoutType.DOUBLE_ROW,
                        () -> {
                            if (mColorIconPopupWindow != null) {
                                mColorIconPopupWindow.dismiss();
                                mColorIconPopupWindow = null;
                            }
                            onDismissListener.onDismiss();
                        });
        mColorPickerCoordinator.setSelectedColorItem(
                mModel.get(TabGridDialogProperties.TAB_GROUP_COLOR_ID));

        int popupMargin =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_group_color_picker_popup_padding);

        View contentView = mColorPickerCoordinator.getContainerView();
        contentView.setPadding(popupMargin, popupMargin, popupMargin, popupMargin);
        View decorView = ((Activity) contentView.getContext()).getWindow().getDecorView();

        // If the filter is in incognito mode, apply the incognito background drawable.
        @DrawableRes
        int bgDrawableId =
                mModel.get(TabGridDialogProperties.IS_INCOGNITO)
                        ? R.drawable.menu_bg_tinted_on_dark_bg
                        : R.drawable.menu_bg_tinted;

        mColorIconPopupWindow =
                new AnchoredPopupWindow(
                        mActivity,
                        decorView,
                        AppCompatResources.getDrawable(mActivity, bgDrawableId),
                        contentView,
                        new ViewRectProvider(anchorView));
        mColorIconPopupWindow.addOnDismissListener(onDismissListener);
        mColorIconPopupWindow.setFocusable(true);
        mColorIconPopupWindow.setHorizontalOverlapAnchor(true);
        mColorIconPopupWindow.setVerticalOverlapAnchor(true);
        mColorIconPopupWindow.show();
    }

    /** Destroy any members that needs clean up. */
    public void destroy() {
        mAttachViewCallback.onResult(null);
        mTabListCoordinator.onDestroy();
        mMediator.destroy();
        mModelChangeProcessor.destroy();
        if (mTabListEditorCoordinator != null) {
            mTabListEditorCoordinator.destroy();
        }

        if (mColorIconPopupWindow != null) {
            mColorIconPopupWindow.dismiss();
            mColorIconPopupWindow = null;
        }
        if (mTabLabeller != null) {
            mTabLabeller.destroy();
        }
    }

    @Override
    public boolean isVisible() {
        return mMediator.isVisible();
    }

    /**
     * @param tabId The tab ID to get a rect for.
     * @return a {@link Rect} for the tab's thumbnail (may be an empty rect if the tab is not
     *     found).
     */
    Rect getTabThumbnailRect(int tabId) {
        return mTabListCoordinator.getTabThumbnailRect(tabId);
    }

    Size getThumbnailSize() {
        return mTabListCoordinator.getThumbnailSize();
    }

    void waitForLayoutWithTab(int tabId, Runnable r) {
        mTabListCoordinator.waitForLayoutWithTab(tabId, r);
    }

    Rect getGlobalLocationOfCurrentThumbnail() {
        Rect thumbnail = mTabListCoordinator.getThumbnailLocationOfCurrentTab();
        Rect recyclerViewLocation = mTabListCoordinator.getRecyclerViewLocation();
        thumbnail.offset(recyclerViewLocation.left, recyclerViewLocation.top);
        return thumbnail;
    }

    TabGridDialogMediator.DialogController getDialogController() {
        return this;
    }

    /* package */ PropertyModel getModelForTesting() {
        return mModel;
    }

    @Override
    public void resetWithListOfTabs(@Nullable List<Tab> tabs) {
        mAttachViewCallback.onResult(mDialogView);
        mTabListCoordinator.resetWithListOfTabs(
                tabs, /* tabGroupSyncIds= */ null, /* quickMode= */ false);
        boolean startedToShow = mMediator.onReset(tabs);
        if (startedToShow) {
            mShowingOrAnimationSupplier.set(true);

            // Defer any undo snackbars while the dialog is open or animating. While the dialog
            // is open and not animating all tab closure events get dropped and are handled by
            // TabGridDialogMediator instead. During animations we should instead queue the
            // snackbars so that talkback announcements will not get clobbered.
            throttleUndoBar();
        } else {
            mAttachViewCallback.onResult(null);
        }
        mTabListOnScrollListener.postUpdate(mTabListCoordinator.getContainerView());

        mCurrentTabGroupId.set(
                !startedToShow || tabs == null || tabs.isEmpty()
                        ? null
                        : tabs.get(0).getTabGroupId());
        if (mTabLabeller != null) {
            mTabLabeller.showAll();
        }
    }

    @Override
    public void hideDialog(boolean showAnimation) {
        mMediator.hideDialog(showAnimation);
    }

    @Override
    public void prepareDialog() {
        mTabListCoordinator.prepareTabGridView();
    }

    @Override
    public void postHiding() {
        mTabListCoordinator.postHiding();
        // TODO(crbug.com/40239632): This shouldn't be required if resetWithListOfTabs(null) is
        // called. Find out why this helps and fix upstream if possible.
        mTabListCoordinator.softCleanup();
        mShowingOrAnimationSupplier.set(false);
        mAttachViewCallback.onResult(null);

        // Stop throttling the undo snackbar and allow any pending snackbars to show. At this
        // point a11y announcements will work correctly as there isn't an ongoing animation
        // occluding the snackbar region.
        stopThrottlingUndoBar();
    }

    @Override
    public boolean handleBackPressed() {
        if (!isVisible()) return false;
        handleBackPress();
        return true;
    }

    @Override
    public ObservableSupplier<Boolean> getShowingOrAnimationSupplier() {
        return mShowingOrAnimationSupplier;
    }

    @Override
    public @BackPressResult int handleBackPress() {
        final boolean handled = mMediator.handleBackPress();
        return handled ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressChangedSupplier;
    }

    @Override
    public void addMessageCardItem(int position, PropertyModel messageCardModel) {
        mTabListCoordinator.addSpecialListItem(
                position, UiType.COLLABORATION_ACTIVITY_MESSAGE, messageCardModel);
    }

    @Override
    public void removeMessageCardItem(@MessageType int messageType) {
        mTabListCoordinator.removeSpecialListItem(
                UiType.COLLABORATION_ACTIVITY_MESSAGE, messageType);
    }

    @Override
    public boolean messageCardExists(@MessageType int messageType) {
        return mTabListCoordinator.specialItemExists(messageType);
    }

    @Override
    public void setGridContentSensitivity(boolean contentIsSensitive) {
        mMediator.setGridContentSensitivity(contentIsSensitive);
    }

    private void throttleUndoBar() {
        if (mUndoBarThrottleToken != null) {
            mUndoBarThrottle.stopThrottling(mUndoBarThrottleToken);
        }
        mUndoBarThrottleToken = mUndoBarThrottle.startThrottling();
    }

    private void stopThrottlingUndoBar() {
        if (mUndoBarThrottleToken != null) {
            mUndoBarThrottle.stopThrottling(mUndoBarThrottleToken);
            mUndoBarThrottleToken = null;
        }
    }
}
