// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupUtils;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.util.List;

/**
 * A coordinator for TabGroupUi component. Manages the communication with
 * {@link TabListCoordinator} as well as the life-cycle of
 * shared component objects.
 */
public class TabGroupUiCoordinator implements TabGroupUiMediator.ResetHandler, TabGroupUi,
                                              PauseResumeWithNativeObserver,
                                              TabGroupUiMediator.TabGroupUiController {
    static final String COMPONENT_NAME = "TabStrip";
    private final Activity mActivity;
    private final Context mContext;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final PropertyModel mModel;
    private final IncognitoStateProvider mIncognitoStateProvider;
    private final TabGroupUiToolbarView mToolbarView;
    private final ViewGroup mTabListContainerView;
    private final ScrimCoordinator mScrimCoordinator;
    private final ObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private final BottomSheetController mBottomSheetController;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final Supplier<Boolean> mIsWarmOnResumeSupplier;
    private final ViewGroup mRootView;
    private final TabModelSelector mTabModelSelector;
    private final OneshotSupplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    private final SnackbarManager mSnackbarManager;
    private final TabCreatorManager mTabCreatorManager;
    private final Supplier<DynamicResourceLoader> mDynamicResourceLoaderSupplier;
    private final TabContentManager mTabContentManager;
    private PropertyModelChangeProcessor mModelChangeProcessor;
    private TabGridDialogCoordinator mTabGridDialogCoordinator;
    private LazyOneshotSupplier<TabGridDialogMediator.DialogController>
            mTabGridDialogControllerSupplier;
    private TabListCoordinator mTabStripCoordinator;
    private TabGroupUiMediator mMediator;

    /**
     * Creates a new {@link TabGroupUiCoordinator}
     */
    public TabGroupUiCoordinator(@NonNull Activity activity, @NonNull ViewGroup parentView,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull IncognitoStateProvider incognitoStateProvider,
            @NonNull ScrimCoordinator scrimCoordinator,
            @NonNull ObservableSupplier<Boolean> omniboxFocusStateSupplier,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull Supplier<Boolean> isWarmOnResumeSupplier,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager, @NonNull ViewGroup rootView,
            @NonNull Supplier<DynamicResourceLoader> dynamicResourceLoaderSupplier,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            @NonNull SnackbarManager snackbarManager) {
        try (TraceEvent e = TraceEvent.scoped("TabGroupUiCoordinator.constructor")) {
            mActivity = activity;
            mContext = parentView.getContext();
            mBrowserControlsStateProvider = browserControlsStateProvider;
            mIncognitoStateProvider = incognitoStateProvider;
            mScrimCoordinator = scrimCoordinator;
            mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
            mModel = new PropertyModel(TabGroupUiProperties.ALL_KEYS);
            mToolbarView = (TabGroupUiToolbarView) LayoutInflater.from(mContext).inflate(
                    R.layout.bottom_tab_strip_toolbar, parentView, false);
            mTabListContainerView = mToolbarView.getViewContainer();
            mBottomSheetController = bottomSheetController;
            mActivityLifecycleDispatcher = activityLifecycleDispatcher;
            mActivityLifecycleDispatcher.register(this);
            mIsWarmOnResumeSupplier = isWarmOnResumeSupplier;
            mTabModelSelector = tabModelSelector;
            mLayoutStateProviderSupplier = layoutStateProviderSupplier;
            mRootView = rootView;
            mSnackbarManager = snackbarManager;
            mTabCreatorManager = tabCreatorManager;
            mDynamicResourceLoaderSupplier = dynamicResourceLoaderSupplier;
            mTabContentManager = tabContentManager;
            parentView.addView(mToolbarView);
        }
    }

    private void initTabGridDialogCoordinator() {
        assert mTabGridDialogControllerSupplier != null;
        if (mTabGridDialogCoordinator != null) return;

        mTabGridDialogCoordinator = new TabGridDialogCoordinator(mActivity,
                mBrowserControlsStateProvider, mTabModelSelector, mTabContentManager,
                mTabCreatorManager, mActivity.findViewById(R.id.coordinator), null, null, null,
                mScrimCoordinator, mTabStripCoordinator.getTabGroupTitleEditor(), mRootView);
        mTabGridDialogControllerSupplier.set(mTabGridDialogCoordinator);
    }

    /**
     * Handle any initialization that occurs once native has been loaded.
     */
    @Override
    public void initializeWithNative(Activity activity,
            BottomControlsCoordinator.BottomControlsVisibilityController visibilityController,
            Callback<Object> onModelTokenChange) {
        try (TraceEvent e = TraceEvent.scoped("TabGroupUiCoordinator.initializeWithNative")) {
            mTabStripCoordinator = new TabListCoordinator(TabListCoordinator.TabListMode.STRIP,
                    mContext, mBrowserControlsStateProvider, mTabModelSelector, null, null, false,
                    null, null, TabProperties.UiType.STRIP, null, null, mTabListContainerView, true,
                    COMPONENT_NAME, mRootView, onModelTokenChange);
            mTabStripCoordinator.initWithNative(mDynamicResourceLoaderSupplier.get());

            mModelChangeProcessor = PropertyModelChangeProcessor.create(mModel,
                    new TabGroupUiViewBinder.ViewHolder(
                            mToolbarView, mTabStripCoordinator.getContainerView()),
                    TabGroupUiViewBinder::bind);

            // TODO(crbug.com/972217): find a way to enable interactions between grid tab switcher
            //  and the dialog here.
            if (mScrimCoordinator != null) {
                mTabGridDialogControllerSupplier = new LazyOneshotSupplier<>() {
                    @Override
                    public void doSet() {
                        initTabGridDialogCoordinator();
                    }
                };
            } else {
                mTabGridDialogControllerSupplier = null;
            }

            mMediator = new TabGroupUiMediator(mActivity, visibilityController, this, mModel,
                    mTabModelSelector, mTabCreatorManager, mLayoutStateProviderSupplier,
                    mIncognitoStateProvider, mTabGridDialogControllerSupplier,
                    mOmniboxFocusStateSupplier);

            TabGroupUtils.startObservingForCreationIPH();

            // TODO(meiliang): Potential leak if the observer is added after restoreCompleted. Fix
            // it. Record the group count after all tabs are being restored. This only happen once
            // per life cycle, therefore remove the observer after recording. We only focus on
            // normal tab model because we don't restore tabs in incognito tab model.
            mTabModelSelector.getModel(false).addObserver(new TabModelObserver() {
                @Override
                public void restoreCompleted() {
                    recordTabGroupCount();
                    mTabModelSelector.getModel(false).removeObserver(this);
                }
            });
        }
    }

    /**
     * @return {@link Supplier} that provides dialog visibility.
     */
    @Override
    public boolean isTabGridDialogVisible() {
        return mTabGridDialogCoordinator != null && mTabGridDialogCoordinator.isVisible();
    }

    /**
     * Handles a reset event originated from {@link TabGroupUiMediator} to reset the tab strip.
     *
     * @param tabs List of Tabs to reset.
     */
    @Override
    public void resetStripWithListOfTabs(List<Tab> tabs) {
        if (tabs != null && tabs.size() > 1
                && mBottomSheetController.getSheetState()
                        == BottomSheetController.SheetState.HIDDEN) {
            TabGroupUtils.maybeShowIPH(FeatureConstants.TAB_GROUPS_TAP_TO_SEE_ANOTHER_TAB_FEATURE,
                    mTabStripCoordinator.getContainerView(), mBottomSheetController);
        }
        mTabStripCoordinator.resetWithListOfTabs(tabs);
    }

    /**
     * Handles a reset event originated from {@link TabGroupUiMediator}
     * when the bottom sheet is expanded or the dialog is shown.
     *
     * @param tabs List of Tabs to reset.
     */
    @Override
    public void resetGridWithListOfTabs(List<Tab> tabs) {
        if (mTabGridDialogControllerSupplier != null) {
            mTabGridDialogControllerSupplier.get().resetWithListOfTabs(tabs);
        }
    }

    /**
     * TabGroupUi implementation.
     */
    @Override
    public boolean onBackPressed() {
        return mMediator.onBackPressed();
    }

    @Override
    public @BackPressResult int handleBackPress() {
        return mMediator.handleBackPress();
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mMediator.getHandleBackPressChangedSupplier();
    }

    /**
     * Destroy any members that needs clean up.
     */
    @Override
    public void destroy() {
        // TODO(crbug.com/1208462): Add tests for destroy conditions.
        // Early return if the component hasn't initialized yet.
        if (mActivity == null) return;

        mTabStripCoordinator.onDestroy();
        if (mTabGridDialogCoordinator != null) {
            mTabGridDialogCoordinator.destroy();
        }
        mModelChangeProcessor.destroy();
        mMediator.destroy();
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
        }
    }

    // PauseResumeWithNativeObserver implementation.
    @Override
    public void onResumeWithNative() {
        // Since we use AsyncTask for restoring tabs, this method can be called before or after
        // restoring all tabs. Therefore, we skip recording the count here during cold start and
        // record that elsewhere when TabModel emits the restoreCompleted signal.
        if (!mIsWarmOnResumeSupplier.get()) return;

        recordTabGroupCount();
    }

    private void recordTabGroupCount() {
        if (mTabModelSelector == null) return;

        TabModelFilterProvider provider = mTabModelSelector.getTabModelFilterProvider();
        TabGroupModelFilter normalFilter = (TabGroupModelFilter) provider.getTabModelFilter(false);
        TabGroupModelFilter incognitoFilter =
                (TabGroupModelFilter) provider.getTabModelFilter(true);
        int groupCount = normalFilter.getTabGroupCount() + incognitoFilter.getTabGroupCount();
        RecordHistogram.recordCount1MHistogram("TabGroups.UserGroupCount", groupCount);
    }

    @Override
    public void onPauseWithNative() {}

    // TabGroupUiController implementation.
    @Override
    public void setupLeftButtonDrawable(int drawableId) {
        mMediator.setupLeftButtonDrawable(drawableId);
    }

    @Override
    public void setupLeftButtonOnClickListener(View.OnClickListener listener) {
        mMediator.setupLeftButtonOnClickListener(listener);
    }
}
