// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.android.material.appbar.AppBarLayout;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.ntp.IncognitoCookieControlsManager;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate.TabSwitcherType;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Coordinator for displaying task-related surfaces (Tab Switcher, MV Tiles, Omnibox, etc.).
 *  Concrete implementation of {@link TasksSurface}.
 */
public class TasksSurfaceCoordinator implements TasksSurface {
    private final TabSwitcher mTabSwitcher;
    private final TasksView mView;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private final TasksSurfaceMediator mMediator;
    private MostVisitedListCoordinator mMostVisitedList;
    private final PropertyModel mPropertyModel;
    private final @TabSwitcherType int mTabSwitcherType;
    private final SnackbarManager mSnackbarManager;
    private final Supplier<DynamicResourceLoader> mDynamicResourceLoaderSupplier;
    private final TabContentManager mTabContentManager;
    private final ModalDialogManager mModalDialogManager;

    /** This flag should be reset once {@link mMostVisitedList#destroyMVTiles()} is called. */
    private boolean mIsMVTilesInitialized;

    /** {@see TabManagementDelegate#createTasksSurface} */
    public TasksSurfaceCoordinator(@NonNull Activity activity,
            @NonNull ScrimCoordinator scrimCoordinator, @NonNull PropertyModel propertyModel,
            @TabSwitcherType int tabSwitcherType, @NonNull Supplier<Tab> parentTabSupplier,
            boolean hasMVTiles, @NonNull WindowAndroid windowAndroid,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull TabModelSelector tabModelSelector, @NonNull SnackbarManager snackbarManager,
            @NonNull Supplier<DynamicResourceLoader> dynamicResourceLoaderSupplier,
            @NonNull TabContentManager tabContentManager,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull Supplier<ShareDelegate> shareDelegateSupplier,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull ViewGroup rootView) {
        mView = (TasksView) LayoutInflater.from(activity).inflate(R.layout.tasks_view_layout, null);
        mView.initialize(activityLifecycleDispatcher,
                parentTabSupplier.hasValue() && parentTabSupplier.get().isIncognito(),
                windowAndroid);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(propertyModel, mView, TasksViewBinder::bind);
        mPropertyModel = propertyModel;
        mTabSwitcherType = tabSwitcherType;
        mSnackbarManager = snackbarManager;
        mDynamicResourceLoaderSupplier = dynamicResourceLoaderSupplier;
        mTabContentManager = tabContentManager;
        mModalDialogManager = modalDialogManager;
        if (tabSwitcherType == TabSwitcherType.CAROUSEL) {
            mTabSwitcher = TabManagementModuleProvider.getDelegate().createCarouselTabSwitcher(
                    activity, activityLifecycleDispatcher, tabModelSelector, tabContentManager,
                    browserControlsStateProvider, tabCreatorManager, menuOrKeyboardActionController,
                    mView.getCarouselTabSwitcherContainer(), shareDelegateSupplier,
                    multiWindowModeStateDispatcher, scrimCoordinator, rootView);
        } else if (tabSwitcherType == TabSwitcherType.GRID) {
            mTabSwitcher = TabManagementModuleProvider.getDelegate().createGridTabSwitcher(activity,
                    activityLifecycleDispatcher, tabModelSelector, tabContentManager,
                    browserControlsStateProvider, tabCreatorManager, menuOrKeyboardActionController,
                    mView.getBodyViewContainer(), shareDelegateSupplier,
                    multiWindowModeStateDispatcher, scrimCoordinator, rootView);
        } else if (tabSwitcherType == TabSwitcherType.SINGLE) {
            mTabSwitcher = new SingleTabSwitcherCoordinator(
                    activity, mView.getCarouselTabSwitcherContainer(), tabModelSelector);
        } else if (tabSwitcherType == TabSwitcherType.NONE) {
            mTabSwitcher = null;
        } else {
            mTabSwitcher = null;
            assert false : "Unsupported tab switcher type";
        }

        View.OnClickListener incognitoLearnMoreClickListener = v -> {
            HelpAndFeedbackLauncherImpl.getInstance().show(activity,
                    activity.getString(R.string.help_context_incognito_learn_more),
                    Profile.getLastUsedRegularProfile().getPrimaryOTRProfile(
                            /*createIfNeeded=*/true),
                    null);
        };
        IncognitoCookieControlsManager incognitoCookieControlsManager =
                new IncognitoCookieControlsManager();
        mMediator = new TasksSurfaceMediator(propertyModel, incognitoLearnMoreClickListener,
                incognitoCookieControlsManager, tabSwitcherType == TabSwitcherType.CAROUSEL);

        if (hasMVTiles) {
            MvTilesLayout mvTilesLayout = mView.findViewById(R.id.mv_tiles_layout);
            mMostVisitedList = new MostVisitedListCoordinator(activity, mvTilesLayout,
                    mPropertyModel, parentTabSupplier, snackbarManager, windowAndroid);
            mMostVisitedList.initialize();
        }
    }

    /**
     * TasksSurface implementation.
     */
    @Override
    public void initialize() {
        assert LibraryLoader.getInstance().isInitialized();
        if (!mIsMVTilesInitialized && mMostVisitedList != null) {
            mMostVisitedList.initWithNative();
            mIsMVTilesInitialized = true;
        }
        mMediator.initialize();
    }

    @Override
    public void setOnTabSelectingListener(TabSwitcher.OnTabSelectingListener listener) {
        if (mTabSwitcher != null) {
            mTabSwitcher.setOnTabSelectingListener(listener);
        }
    }

    @Override
    public @Nullable TabSwitcher.Controller getController() {
        return mTabSwitcher != null ? mTabSwitcher.getController() : null;
    }

    @Override
    public @Nullable TabSwitcher.TabListDelegate getTabListDelegate() {
        return mTabSwitcher != null ? mTabSwitcher.getTabListDelegate() : null;
    }

    @Override
    public Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        if (mTabSwitcherType != TabSwitcherType.CAROUSEL
                && mTabSwitcherType != TabSwitcherType.GRID) {
            return null;
        }
        assert mTabSwitcher != null;
        return mTabSwitcher.getTabGridDialogVisibilitySupplier();
    }

    @Override
    public ViewGroup getBodyViewContainer() {
        return mView.getBodyViewContainer();
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public View getTopToolbarPlaceholderView() {
        return mView != null ? mView.findViewById(R.id.top_toolbar_placeholder) : null;
    }

    @Override
    public void onFinishNativeInitialization(Context context, OmniboxStub omniboxStub) {
        if (mTabSwitcher != null) {
            mTabSwitcher.initWithNative(context, mTabContentManager,
                    mDynamicResourceLoaderSupplier.get(), mSnackbarManager, mModalDialogManager);
        }

        mMediator.initWithNative(omniboxStub);
    }

    @Override
    public void addHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener) {
        mView.addHeaderOffsetChangeListener(onOffsetChangedListener);
    }

    @Override
    public void removeHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener) {
        mView.removeHeaderOffsetChangeListener(onOffsetChangedListener);
    }

    @Override
    public void addFakeSearchBoxShrinkAnimation() {
        mView.addFakeSearchBoxShrinkAnimation();
    }

    @Override
    public void removeFakeSearchBoxShrinkAnimation() {
        mView.removeFakeSearchBoxShrinkAnimation();
    }

    @Override
    public void onHide() {
        if (mMostVisitedList != null) {
            mMostVisitedList.destroyMVTiles();
            mIsMVTilesInitialized = false;
        }
    }

    @VisibleForTesting
    @Override
    public boolean isMVTilesCleanedUp() {
        assert mMostVisitedList != null;
        return mMostVisitedList.isMVTilesCleanedUp();
    }

    @VisibleForTesting
    @Override
    public boolean isMVTilesInitialized() {
        return mIsMVTilesInitialized;
    }
}
