// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.graphics.Rect;
import android.util.Size;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.PopupWindow;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.data_sharing.SharedImageTilesCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator.ColorPickerLayoutType;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.List;

/**
 * A coordinator for TabGridDialog component. Manages the communication with
 * {@link TabListCoordinator} as well as the life-cycle of shared component
 * objects.
 */
public class TabGridDialogCoordinator implements TabGridDialogMediator.DialogController {
    private final String mComponentName;
    private final TabListCoordinator mTabListCoordinator;
    private final TabGridDialogMediator mMediator;
    private final PropertyModel mModel;
    private final PropertyModelChangeProcessor mModelChangeProcessor;
    private final ViewGroup mRootView;
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private final Activity mActivity;
    private final ObservableSupplier<TabModelFilter> mCurrentTabModelFilterSupplier;
    private final Supplier<TabModel> mRegularTabModelSupplier;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private TabContentManager mTabContentManager;
    private TabListEditorCoordinator mTabListEditorCoordinator;
    private TabGridDialogView mDialogView;
    private ColorPickerCoordinator mColorPickerCoordinator;
    private TabGridDialogShareBottomSheetContent mShareBottomSheetContent;
    private @Nullable SnackbarManager mSnackbarManager;
    private @Nullable SharedImageTilesCoordinator mSharedImageTilesCoordinator;
    private @Nullable AnchoredPopupWindow mColorIconPopupWindow;
    private @Nullable TabSwitcherResetHandler mTabSwitcherResetHandler;

    TabGridDialogCoordinator(
            Activity activity,
            BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull ObservableSupplier<TabModelFilter> currentTabModelFilterSupplier,
            @NonNull Supplier<TabModel> regularTabModelSupplier,
            TabContentManager tabContentManager,
            TabCreatorManager tabCreatorManager,
            ViewGroup containerView,
            TabSwitcherResetHandler resetHandler,
            TabListMediator.GridCardOnClickListenerProvider gridCardOnClickListenerProvider,
            TabGridDialogMediator.AnimationSourceViewProvider animationSourceViewProvider,
            ScrimCoordinator scrimCoordinator,
            TabGroupTitleEditor tabGroupTitleEditor,
            ViewGroup rootView) {
        try (TraceEvent e = TraceEvent.scoped("TabGridDialogCoordinator.constructor")) {
            mActivity = activity;
            mComponentName =
                    animationSourceViewProvider == null
                            ? "TabGridDialogFromStrip"
                            : "TabGridDialogInSwitcher";
            mBrowserControlsStateProvider = browserControlsStateProvider;
            mCurrentTabModelFilterSupplier = currentTabModelFilterSupplier;
            mRegularTabModelSupplier = regularTabModelSupplier;
            mTabContentManager = tabContentManager;
            mTabSwitcherResetHandler = resetHandler;

            mModel =
                    new PropertyModel.Builder(TabGridDialogProperties.ALL_KEYS)
                            .with(
                                    TabGridDialogProperties.BROWSER_CONTROLS_STATE_PROVIDER,
                                    mBrowserControlsStateProvider)
                            .with(
                                    TabGridDialogProperties.COLOR_ICON_CLICK_LISTENER,
                                    getColorIconClickListener())
                            .build();
            mRootView = rootView;

            mDialogView = containerView.findViewById(R.id.dialog_parent_view);
            if (mDialogView == null) {
                ViewStub dialogStub = containerView.findViewById(R.id.tab_grid_dialog_stub);
                assert dialogStub != null;

                dialogStub.setLayoutResource(R.layout.tab_grid_dialog_layout);
                dialogStub.inflate();

                mDialogView = containerView.findViewById(R.id.dialog_parent_view);
                mDialogView.setupScrimCoordinator(scrimCoordinator);
            }

            if (!activity.isDestroyed() && !activity.isFinishing()) {
                mSnackbarManager =
                        new SnackbarManager(activity, mDialogView.getSnackBarContainer(), null);
            } else {
                mSnackbarManager = null;
            }

            View shareBar = null;
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING_ANDROID)) {
                shareBar =
                        LayoutInflater.from(activity)
                                .inflate(
                                        R.layout.data_sharing_group_bar,
                                        mDialogView.findViewById(R.id.dialog_container_view),
                                        false);
                ViewGroup manageBar = shareBar.findViewById(R.id.dialog_data_sharing_manage);
                mSharedImageTilesCoordinator =
                        new SharedImageTilesCoordinator(mDialogView.getContext());
                manageBar.addView(mSharedImageTilesCoordinator.getView(), 0);

                mShareBottomSheetContent =
                        new TabGridDialogShareBottomSheetContent(
                                LayoutInflater.from(activity)
                                        .inflate(R.layout.data_sharing_bottom_sheet, null));
            }

            Runnable showShareBottomSheetRunnable =
                    () -> {
                        bottomSheetController.requestShowContent(mShareBottomSheetContent, true);
                    };

            Runnable showColorPickerPopupRunnable =
                    () -> {
                        showColorPickerPopup(mDialogView.findViewById(R.id.tab_group_color_icon));
                    };

            mMediator =
                    new TabGridDialogMediator(
                            activity,
                            this,
                            mModel,
                            currentTabModelFilterSupplier,
                            tabCreatorManager,
                            resetHandler,
                            this::getRecyclerViewPosition,
                            animationSourceViewProvider,
                            mSnackbarManager,
                            mSharedImageTilesCoordinator,
                            bottomSheetController,
                            showShareBottomSheetRunnable,
                            mComponentName,
                            showColorPickerPopupRunnable);

            // TODO(crbug.com/1031349) : Remove the inline mode logic here, make the constructor to
            // take in a mode parameter instead.
            mTabListCoordinator =
                    new TabListCoordinator(
                            TabUiFeatureUtilities.shouldUseListMode()
                                    ? TabListCoordinator.TabListMode.LIST
                                    : TabListCoordinator.TabListMode.GRID,
                            activity,
                            mBrowserControlsStateProvider,
                            currentTabModelFilterSupplier,
                            regularTabModelSupplier,
                            (tabId,
                                    thumbnailSize,
                                    callback,
                                    forceUpdate,
                                    writeBack,
                                    isSelected) -> {
                                tabContentManager.getTabThumbnailWithCallback(
                                        tabId, thumbnailSize, callback, forceUpdate, writeBack);
                            },
                            null,
                            false,
                            gridCardOnClickListenerProvider,
                            mMediator.getTabGridDialogHandler(),
                            TabProperties.UiType.CLOSABLE,
                            null,
                            null,
                            containerView,
                            false,
                            mComponentName,
                            rootView,
                            null);
            mTabListCoordinator.setOnLongPressTabItemEventListener(mMediator);
            TabListRecyclerView recyclerView = mTabListCoordinator.getContainerView();

            TabGroupUiToolbarView toolbarView =
                    (TabGroupUiToolbarView)
                            LayoutInflater.from(activity)
                                    .inflate(R.layout.tab_group_ui_toolbar, recyclerView, false);
            toolbarView.setupDialogToolbarLayout();
            mModelChangeProcessor =
                    PropertyModelChangeProcessor.create(
                            mModel,
                            new TabGridDialogViewBinder.ViewHolder(
                                    toolbarView, recyclerView, mDialogView, shareBar),
                            TabGridDialogViewBinder::bind);
            mBackPressChangedSupplier.set(isVisible());
            mModel.addObserver((source, key) -> mBackPressChangedSupplier.set(isVisible()));

            // This is always created post-native so calling these immediately is safe.
            // TODO(crbug/1418690): Consider inlining these behaviors in their respective
            // constructors if possible.
            mMediator.initWithNative(this::getTabListEditorController, tabGroupTitleEditor);
            mTabListCoordinator.initWithNative(mRegularTabModelSupplier.get().getProfile(), null);
        }
    }

    @NonNull
    RecyclerViewPosition getRecyclerViewPosition() {
        return mTabListCoordinator.getRecyclerViewPosition();
    }

    private @Nullable TabListEditorController getTabListEditorController() {
        if (mTabListEditorCoordinator == null) {
            assert mSnackbarManager != null
                    : "SnackbarManager should have been created or the activity was already"
                            + " finishing.";

            @TabListCoordinator.TabListMode
            int mode =
                    TabUiFeatureUtilities.shouldUseListMode()
                            ? TabListCoordinator.TabListMode.LIST
                            : TabListCoordinator.TabListMode.GRID;
            mTabListEditorCoordinator =
                    new TabListEditorCoordinator(
                            mActivity,
                            mDialogView.findViewById(R.id.dialog_container_view),
                            mBrowserControlsStateProvider,
                            mCurrentTabModelFilterSupplier,
                            mRegularTabModelSupplier,
                            mTabContentManager,
                            mTabListCoordinator::setRecyclerViewPosition,
                            mode,
                            mRootView,
                            /* displayGroups= */ false,
                            mSnackbarManager,
                            TabProperties.UiType.SELECTABLE);
        }

        return mTabListEditorCoordinator.getController();
    }

    private View.OnClickListener getColorIconClickListener() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)) {
            return (view) -> {
                showColorPickerPopup(view);
            };
        }
        return null;
    }

    private void showColorPickerPopup(View anchorView) {
        PopupWindow.OnDismissListener onDismissListener =
                new PopupWindow.OnDismissListener() {
                    @Override
                    public void onDismiss() {
                        mMediator.setSelectedTabGroupColor(
                                mColorPickerCoordinator.getSelectedColorSupplier().get());

                        // Only require a refresh of the tab list if accessed from the GTS,
                        // skip if this is reached from the tab strip as the color will
                        // refresh upon re-entering the tab switcher.
                        if (mTabSwitcherResetHandler != null) {
                            // Refresh the TabSwitcher's tab list to reflect the last
                            // selected color in the color picker when it is dismissed. This
                            // call will be invoked for both Grid and List modes on the GTS.
                            mTabSwitcherResetHandler.resetWithTabList(
                                    (TabGroupModelFilter) mCurrentTabModelFilterSupplier.get(),
                                    false);
                        }
                    }
                };

        List<Integer> colors = ColorPickerUtils.getTabGroupColorIdList();
        mColorPickerCoordinator =
                new ColorPickerCoordinator(
                        mActivity,
                        colors,
                        R.layout.tab_group_color_picker_container,
                        ColorPickerType.TAB_GROUP,
                        mModel.get(TabGridDialogProperties.IS_INCOGNITO),
                        ColorPickerLayoutType.DOUBLE_ROW,
                        () -> {
                            mColorIconPopupWindow.dismiss();
                            mColorIconPopupWindow = null;
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
    @NonNull
    Rect getTabThumbnailRect(int tabId) {
        return mTabListCoordinator.getTabThumbnailRect(tabId);
    }

    @NonNull
    Size getThumbnailSize() {
        return mTabListCoordinator.getThumbnailSize();
    }

    void waitForLayoutWithTab(int tabId, Runnable r) {
        mTabListCoordinator.waitForLayoutWithTab(tabId, r);
    }

    @NonNull
    Rect getGlobalLocationOfCurrentThumbnail() {
        Rect thumbnail = mTabListCoordinator.getThumbnailLocationOfCurrentTab();
        Rect recyclerViewLocation = mTabListCoordinator.getRecyclerViewLocation();
        thumbnail.offset(recyclerViewLocation.left, recyclerViewLocation.top);
        return thumbnail;
    }

    TabGridDialogMediator.DialogController getDialogController() {
        return this;
    }

    @Override
    public void resetWithListOfTabs(@Nullable List<Tab> tabs) {
        mTabListCoordinator.resetWithListOfTabs(tabs);
        mMediator.onReset(tabs);
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
        // TODO(crbug/1366128): This shouldn't be required if resetWithListOfTabs(null) is called.
        // Find out why this helps and fix upstream if possible.
        mTabListCoordinator.softCleanup();
    }

    @Override
    public boolean handleBackPressed() {
        if (!isVisible()) return false;
        handleBackPress();
        return true;
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
}
