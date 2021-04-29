// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import com.google.android.material.appbar.AppBarLayout;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.ntp.IncognitoCookieControlsManager;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate.TabSwitcherType;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

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
    private TrendyTermsCoordinator mTrendyTermsCoordinator;
    private final PropertyModel mPropertyModel;
    private final boolean mHasTrendyTerm;
    private final @TabSwitcherType int mTabSwitcherType;

    public TasksSurfaceCoordinator(ChromeActivity activity, ScrimCoordinator scrimCoordinator,
            PropertyModel propertyModel, @TabSwitcherType int tabSwitcherType,
            Supplier<Tab> parentTabSupplier, boolean hasMVTiles, boolean hasTrendyTerms,
            WindowAndroid windowAndroid) {
        mView = (TasksView) LayoutInflater.from(activity).inflate(R.layout.tasks_view_layout, null);
        mView.initialize(activity.getLifecycleDispatcher(),
                parentTabSupplier.hasValue() ? parentTabSupplier.get().isIncognito() : false,
                windowAndroid);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(propertyModel, mView, TasksViewBinder::bind);
        mPropertyModel = propertyModel;
        mHasTrendyTerm = hasTrendyTerms;
        mTabSwitcherType = tabSwitcherType;
        if (tabSwitcherType == TabSwitcherType.CAROUSEL) {
            mTabSwitcher = TabManagementModuleProvider.getDelegate().createCarouselTabSwitcher(
                    activity, mView.getCarouselTabSwitcherContainer(), scrimCoordinator);
        } else if (tabSwitcherType == TabSwitcherType.GRID) {
            mTabSwitcher = TabManagementModuleProvider.getDelegate().createGridTabSwitcher(
                    activity, mView.getBodyViewContainer(), scrimCoordinator);
        } else if (tabSwitcherType == TabSwitcherType.SINGLE) {
            mTabSwitcher = new SingleTabSwitcherCoordinator(
                    activity, mView.getCarouselTabSwitcherContainer());
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
        Runnable trendyTermsUpdater = null;
        if (hasTrendyTerms) {
            mTrendyTermsCoordinator = new TrendyTermsCoordinator(activity,
                    getView().findViewById(R.id.trendy_terms_recycler_view), parentTabSupplier);

            trendyTermsUpdater = () -> {
                TrendyTermsCache.maybeFetch(Profile.getLastUsedRegularProfile());
                mTrendyTermsCoordinator.populateTrendyTerms();
            };
        }
        mMediator = new TasksSurfaceMediator(propertyModel, incognitoLearnMoreClickListener,
                incognitoCookieControlsManager, tabSwitcherType == TabSwitcherType.CAROUSEL,
                trendyTermsUpdater);

        if (hasMVTiles) {
            MvTilesLayout mvTilesLayout = mView.findViewById(R.id.mv_tiles_layout);
            mMostVisitedList = new MostVisitedListCoordinator(
                    activity, mvTilesLayout, mPropertyModel, parentTabSupplier);
            mMostVisitedList.initialize();
        }
    }

    /** TasksSurface implementation. */
    @Override
    public void initialize() {
        assert LibraryLoader.getInstance().isInitialized();

        if (mMostVisitedList != null) mMostVisitedList.initWithNative();
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
            ChromeActivity activity = (ChromeActivity) context;
            mTabSwitcher.initWithNative(activity, activity.getTabContentManager(),
                    activity.getCompositorViewHolder().getDynamicResourceLoader(), activity,
                    activity.getModalDialogManager());
        }

        mMediator.initWithNative(omniboxStub);

        if (mHasTrendyTerm && mTabSwitcher != null) {
            mTabSwitcher.getController().addOverviewModeObserver(mMediator);
            TrendyTermsCache.maybeFetch(Profile.getLastUsedRegularProfile());
        }
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
}
