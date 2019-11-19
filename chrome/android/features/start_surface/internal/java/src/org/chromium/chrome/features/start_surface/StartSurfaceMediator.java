// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_FAKE_SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_VOICE_RECOGNITION_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.MORE_TABS_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.MV_TILES_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_CLICKLISTENER;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_SELECTED_TAB_POSITION;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.FEED_SURFACE_COORDINATOR;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_BOTTOM_BAR_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_EXPLORE_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SECONDARY_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_BAR_HEIGHT;

import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.ntp.FakeboxDelegate;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.start_surface.R;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The mediator implements the logic to interact with the surfaces and caller. */
class StartSurfaceMediator
        implements StartSurface.Controller, TabSwitcher.OverviewModeObserver, View.OnClickListener {
    @IntDef({SurfaceMode.NO_START_SURFACE, SurfaceMode.TASKS_ONLY, SurfaceMode.TWO_PANES,
            SurfaceMode.SINGLE_PANE})
    @Retention(RetentionPolicy.SOURCE)
    @interface SurfaceMode {
        int NO_START_SURFACE = 0;
        int TASKS_ONLY = 1;
        int TWO_PANES = 2;
        int SINGLE_PANE = 3;
    }

    /** Interface to initialize a secondary tasks surface for more tabs. */
    interface SecondaryTasksSurfaceInitializer {
        /**
         * Initialize the secondary tasks surface and return the surface controller, which is
         * TabSwitcher.Controller.
         * @return The {@link TabSwitcher.Controller} of the secondary tasks surface.
         */
        TabSwitcher.Controller initialize();
    }

    private final ObserverList<StartSurface.OverviewModeObserver> mObservers = new ObserverList<>();
    private final TabSwitcher.Controller mController;
    private final TabModelSelector mTabModelSelector;
    @Nullable
    private final PropertyModel mPropertyModel;
    @Nullable
    private final ExploreSurfaceCoordinator.FeedSurfaceCreator mFeedSurfaceCreator;
    @Nullable
    private final SecondaryTasksSurfaceInitializer mSecondaryTasksSurfaceInitializer;
    @SurfaceMode
    private final int mSurfaceMode;
    @Nullable
    private TabSwitcher.Controller mSecondaryTasksSurfaceController;
    @Nullable
    private PropertyModel mSecondaryTasksSurfacePropertyModel;
    private boolean mIsIncognito;
    @Nullable
    private FakeboxDelegate mFakeboxDelegate;
    private NightModeStateProvider mNightModeStateProvider;
    @Nullable
    UrlFocusChangeListener mUrlFocusChangeListener;
    @Nullable
    private StartSurface.StateObserver mStateObserver;

    StartSurfaceMediator(TabSwitcher.Controller controller, TabModelSelector tabModelSelector,
            @Nullable PropertyModel propertyModel,
            @Nullable ExploreSurfaceCoordinator.FeedSurfaceCreator feedSurfaceCreator,
            @Nullable SecondaryTasksSurfaceInitializer secondaryTasksSurfaceInitializer,
            @SurfaceMode int surfaceMode, @Nullable FakeboxDelegate fakeboxDelegate,
            NightModeStateProvider nightModeStateProvider) {
        mController = controller;
        mTabModelSelector = tabModelSelector;
        mPropertyModel = propertyModel;
        mFeedSurfaceCreator = feedSurfaceCreator;
        mSecondaryTasksSurfaceInitializer = secondaryTasksSurfaceInitializer;
        mSurfaceMode = surfaceMode;
        mFakeboxDelegate = fakeboxDelegate;
        mNightModeStateProvider = nightModeStateProvider;

        if (mPropertyModel != null) {
            assert mSurfaceMode == SurfaceMode.SINGLE_PANE || mSurfaceMode == SurfaceMode.TWO_PANES
                    || mSurfaceMode == SurfaceMode.TASKS_ONLY;
            assert mFakeboxDelegate != null;

            mIsIncognito = mTabModelSelector.isIncognitoSelected();

            mTabModelSelector.addObserver(new EmptyTabModelSelectorObserver() {
                @Override
                public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                    // TODO(crbug.com/982018): Optimize to not listen for selected Tab model change
                    // when overview is not shown.
                    updateIncognitoMode(newModel.isIncognito());
                }
            });
            mPropertyModel.set(IS_INCOGNITO, mIsIncognito);

            if (mSurfaceMode == SurfaceMode.TWO_PANES) {
                mPropertyModel.set(BOTTOM_BAR_CLICKLISTENER,
                        new StartSurfaceProperties.BottomBarClickListener() {
                            // TODO(crbug.com/982018): Animate switching between explore and home
                            // surface.
                            @Override
                            public void onHomeButtonClicked() {
                                setExploreSurfaceVisibility(false);
                                notifyStateChange();
                                RecordUserAction.record("StartSurface.TwoPanes.BottomBar.TapHome");
                            }

                            @Override
                            public void onExploreButtonClicked() {
                                // TODO(crbug.com/982018): Hide the Tab switcher toolbar when
                                // showing explore surface.
                                setExploreSurfaceVisibility(true);
                                notifyStateChange();
                                RecordUserAction.record(
                                        "StartSurface.TwoPanes.BottomBar.TapExploreSurface");
                            }
                        });
                mPropertyModel.set(BOTTOM_BAR_HEIGHT,
                        ContextUtils.getApplicationContext().getResources().getDimensionPixelSize(
                                R.dimen.ss_bottom_bar_height));
                mPropertyModel.set(IS_BOTTOM_BAR_VISIBLE, !mIsIncognito);
            }

            if (mSurfaceMode == SurfaceMode.SINGLE_PANE) {
                mPropertyModel.set(MORE_TABS_CLICK_LISTENER, this);

                // Hide tab carousel, which does not exist in incognito mode, when closing all
                // normal tabs.
                TabModel normalTabModel = mTabModelSelector.getModel(false);
                normalTabModel.addObserver(new EmptyTabModelObserver() {
                    @Override
                    public void willCloseTab(Tab tab, boolean animate) {
                        if (normalTabModel.getCount() <= 1
                                && mPropertyModel.get(IS_SHOWING_OVERVIEW)) {
                            setTabCarouselVisibility(false);
                        }
                    }
                    @Override
                    public void tabClosureUndone(Tab tab) {
                        if (mPropertyModel.get(IS_SHOWING_OVERVIEW)) {
                            setTabCarouselVisibility(true);
                        }
                    }
                });
            }

            // Set the initial state.

            // Show explore surface if not in incognito and either in SINGLE PANES mode
            // or in TWO PANES mode with last visible pane explore.
            boolean shouldShowExploreSurface =
                    (mSurfaceMode == SurfaceMode.SINGLE_PANE
                            || ReturnToStartSurfaceUtil.shouldShowExploreSurface())
                    && !mIsIncognito;
            setExploreSurfaceVisibility(shouldShowExploreSurface);
            mPropertyModel.set(MV_TILES_VISIBLE, !mIsIncognito);

            // Note that isVoiceSearchEnabled will return false in incognito mode.
            mPropertyModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE,
                    mFakeboxDelegate.getLocationBarVoiceRecognitionHandler()
                            .isVoiceSearchEnabled());

            int toolbarHeight =
                    ContextUtils.getApplicationContext().getResources().getDimensionPixelSize(
                            R.dimen.toolbar_height_no_shadow);
            mPropertyModel.set(TOP_BAR_HEIGHT, toolbarHeight);

            mUrlFocusChangeListener = new UrlFocusChangeListener() {
                @Override
                public void onUrlFocusChange(boolean hasFocus) {
                    // No fake search box on the explore pane in two panes mode.
                    if (mSurfaceMode != SurfaceMode.TWO_PANES
                            || !mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)) {
                        mPropertyModel.set(IS_FAKE_SEARCH_BOX_VISIBLE, !hasFocus);
                    }
                    if (mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE)) {
                        mSecondaryTasksSurfacePropertyModel.set(
                                IS_FAKE_SEARCH_BOX_VISIBLE, !hasFocus);
                    }
                    notifyStateChange();
                }
            };
        }
        mController.addOverviewModeObserver(this);
    }

    void setSecondaryTasksSurfacePropertyModel(PropertyModel propertyModel) {
        mSecondaryTasksSurfacePropertyModel = propertyModel;
        mSecondaryTasksSurfacePropertyModel.set(IS_INCOGNITO, mIsIncognito);

        // Secondary tasks surface is used for more Tabs or incognito mode single pane, where MV
        // tiles and voice recognition button should be invisible.
        mSecondaryTasksSurfacePropertyModel.set(MV_TILES_VISIBLE, false);
        mSecondaryTasksSurfacePropertyModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE, false);
    }

    void setStateChangeObserver(StartSurface.StateObserver observer) {
        mStateObserver = observer;
    }

    // Implements StartSurface.Controller
    @Override
    public boolean overviewVisible() {
        return mController.overviewVisible();
    }

    @Override
    public void addOverviewModeObserver(StartSurface.OverviewModeObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeOverviewModeObserver(StartSurface.OverviewModeObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void hideOverview(boolean animate) {
        if (mSecondaryTasksSurfaceController != null
                && mSecondaryTasksSurfaceController.overviewVisible()) {
            assert mSurfaceMode == SurfaceMode.SINGLE_PANE;

            setSecondaryTasksSurfaceVisibility(false);
        }
        mController.hideOverview(animate);
    }

    @Override
    public void showOverview(boolean animate) {
        // TODO(crbug.com/982018): Animate the bottom bar together with the Tab Grid view.
        if (mPropertyModel != null) {
            if (mSurfaceMode == SurfaceMode.SINGLE_PANE) {
                RecordUserAction.record("StartSurface.SinglePane");
                if (mIsIncognito) {
                    setSecondaryTasksSurfaceVisibility(true);
                } else {
                    setExploreSurfaceVisibility(true);
                    setTabCarouselVisibility(mTabModelSelector.getModel(false).getCount() > 0);
                }
            } else if (mSurfaceMode == SurfaceMode.TWO_PANES) {
                RecordUserAction.record("StartSurface.TwoPanes");
                String defaultOnUserActionString = mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)
                        ? "ExploreSurface"
                        : "HomeSurface";
                RecordUserAction.record(
                        "StartSurface.TwoPanes.DefaultOn" + defaultOnUserActionString);
            } else if (mSurfaceMode == SurfaceMode.TASKS_ONLY) {
                RecordUserAction.record("StartSurface.TasksOnly");
            }

            // Make sure FeedSurfaceCoordinator is built before the explore surface is showing by
            // default.
            if (mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)
                    && mPropertyModel.get(FEED_SURFACE_COORDINATOR) == null) {
                mPropertyModel.set(FEED_SURFACE_COORDINATOR,
                        mFeedSurfaceCreator.createFeedSurfaceCoordinator(
                                mNightModeStateProvider.isInNightMode()));
            }

            mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
            mFakeboxDelegate.addUrlFocusChangeListener(mUrlFocusChangeListener);
        }

        mController.showOverview(animate);
    }

    @Override
    public boolean onBackPressed() {
        if (mSecondaryTasksSurfaceController != null
                && mSecondaryTasksSurfaceController.overviewVisible()
                // Secondary tasks surface is used as the main surface in incognito mode.
                && !mIsIncognito) {
            assert mSurfaceMode == SurfaceMode.SINGLE_PANE;

            setSecondaryTasksSurfaceVisibility(false);
            setExploreSurfaceVisibility(true);
            notifyStateChange();
            return true;
        }

        if (mPropertyModel != null && mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)
                && mSurfaceMode == SurfaceMode.TWO_PANES) {
            setExploreSurfaceVisibility(false);
            notifyStateChange();
            return true;
        }

        return mController.onBackPressed();
    }

    // Implements TabSwitcher.OverviewModeObserver.
    @Override
    public void startedShowing() {
        for (StartSurface.OverviewModeObserver observer : mObservers) {
            observer.startedShowing();
        }
    }

    @Override
    public void finishedShowing() {
        for (StartSurface.OverviewModeObserver observer : mObservers) {
            observer.finishedShowing();
        }

        // TODO(crbug.com/982018): Move to showOverview before overview is showing.
        // Note that Tab switcher mode toolbar is lazily created when showing Tab switcher the first
        // time.
        if (mSurfaceMode != SurfaceMode.NO_START_SURFACE) {
            notifyStateChange();
        }
    }

    @Override
    public void startedHiding() {
        if (mPropertyModel != null) {
            mFakeboxDelegate.removeUrlFocusChangeListener(mUrlFocusChangeListener);
            mPropertyModel.set(IS_SHOWING_OVERVIEW, false);

            destroyFeedSurfaceCoordinator();
        }
        for (StartSurface.OverviewModeObserver observer : mObservers) {
            observer.startedHiding();
        }
    }

    @Override
    public void finishedHiding() {
        for (StartSurface.OverviewModeObserver observer : mObservers) {
            observer.finishedHiding();
        }
    }

    private void destroyFeedSurfaceCoordinator() {
        FeedSurfaceCoordinator feedSurfaceCoordinator =
                mPropertyModel.get(FEED_SURFACE_COORDINATOR);
        if (feedSurfaceCoordinator != null) feedSurfaceCoordinator.destroy();
        mPropertyModel.set(FEED_SURFACE_COORDINATOR, null);
    }

    // Implements View.OnClickListener, which listens for the more tabs button.
    @Override
    public void onClick(View v) {
        assert mSurfaceMode == SurfaceMode.SINGLE_PANE;

        if (mSecondaryTasksSurfaceController == null) {
            mSecondaryTasksSurfaceController = mSecondaryTasksSurfaceInitializer.initialize();
            assert mSecondaryTasksSurfacePropertyModel != null;
        }

        setExploreSurfaceVisibility(false);
        setSecondaryTasksSurfaceVisibility(true);
        RecordUserAction.record("StartSurface.SinglePane.MoreTabs");
    }

    /** This interface builds the feed surface coordinator when showing if needed. */
    private void setExploreSurfaceVisibility(boolean isVisible) {
        if (isVisible == mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)) return;

        if (isVisible && mPropertyModel.get(IS_SHOWING_OVERVIEW)
                && mPropertyModel.get(FEED_SURFACE_COORDINATOR) == null) {
            mPropertyModel.set(FEED_SURFACE_COORDINATOR,
                    mFeedSurfaceCreator.createFeedSurfaceCoordinator(
                            mNightModeStateProvider.isInNightMode()));
        }

        mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, isVisible);

        if (mSurfaceMode == SurfaceMode.TWO_PANES) {
            // Update the 'BOTTOM_BAR_SELECTED_TAB_POSITION' property to reflect the change. This is
            // needed when clicking back button on the explore surface.
            mPropertyModel.set(BOTTOM_BAR_SELECTED_TAB_POSITION, isVisible ? 1 : 0);
            ReturnToStartSurfaceUtil.setExploreSurfaceVisibleLast(isVisible);
        }
    }

    private void updateIncognitoMode(boolean isIncognito) {
        if (isIncognito == mIsIncognito) return;
        mIsIncognito = isIncognito;

        mPropertyModel.set(MV_TILES_VISIBLE, !mIsIncognito);

        // This is because LocationBarVoiceRecognitionHandler monitors incognito mode and returns
        // false in incognito mode. However, when switching incognito mode, this class is notified
        // earlier than the LocationBarVoiceRecognitionHandler, so isVoiceSearchEnabled returns
        // incorrect state if check synchronously.
        ThreadUtils.postOnUiThread(() -> {
            mPropertyModel.set(IS_VOICE_RECOGNITION_BUTTON_VISIBLE,
                    mFakeboxDelegate.getLocationBarVoiceRecognitionHandler()
                            .isVoiceSearchEnabled());
        });

        if (mSurfaceMode == SurfaceMode.SINGLE_PANE) {
            setExploreSurfaceVisibility(!mIsIncognito);
            setSecondaryTasksSurfaceVisibility(
                    mIsIncognito && mPropertyModel.get(IS_SHOWING_OVERVIEW));
        } else if (mSurfaceMode == SurfaceMode.TWO_PANES) {
            mPropertyModel.set(BOTTOM_BAR_HEIGHT,
                    mIsIncognito ? 0
                                 : ContextUtils.getApplicationContext()
                                           .getResources()
                                           .getDimensionPixelSize(R.dimen.ss_bottom_bar_height));
            mPropertyModel.set(IS_BOTTOM_BAR_VISIBLE, !mIsIncognito);

            // Hide explore surface if going incognito. When returning to normal mode, we
            // always show the Home Pane, so the Explore Pane stays hidden.
            if (mIsIncognito) setExploreSurfaceVisibility(false);
        }

        mPropertyModel.set(IS_INCOGNITO, mIsIncognito);
        if (mSecondaryTasksSurfacePropertyModel != null) {
            mSecondaryTasksSurfacePropertyModel.set(IS_INCOGNITO, mIsIncognito);
        }

        // TODO(crbug.com/1021399): This looks not needed since there is no way to change incognito
        // mode when focusing on the omnibox and incognito mode change won't affect the visibility
        // of the tab switcher toolbar.
        if (mPropertyModel.get(IS_SHOWING_OVERVIEW)) notifyStateChange();
    }

    private void setSecondaryTasksSurfaceVisibility(boolean isVisible) {
        assert mSurfaceMode == SurfaceMode.SINGLE_PANE;

        if (isVisible) {
            if (mSecondaryTasksSurfaceController == null) {
                mSecondaryTasksSurfaceController = mSecondaryTasksSurfaceInitializer.initialize();
            }
            mSecondaryTasksSurfacePropertyModel.set(IS_FAKE_SEARCH_BOX_VISIBLE, mIsIncognito);
            mSecondaryTasksSurfaceController.showOverview(false);
        } else {
            if (mSecondaryTasksSurfaceController == null) return;
            mSecondaryTasksSurfaceController.hideOverview(false);
        }
        mPropertyModel.set(IS_SECONDARY_SURFACE_VISIBLE, isVisible);
    }

    private void notifyStateChange() {
        assert mSurfaceMode != SurfaceMode.NO_START_SURFACE;
        assert mPropertyModel.get(IS_SHOWING_OVERVIEW);

        if (mStateObserver != null) {
            mStateObserver.onStateChanged(shouldShowTabSwitcherToolbar());
        }
    }

    private boolean shouldShowTabSwitcherToolbar() {
        if (mSurfaceMode == SurfaceMode.SINGLE_PANE) {
            // Show Tab switcher toolbar when showing more Tabs and in incognito single pane when
            // fake search box is visible.
            if (mPropertyModel.get(IS_SECONDARY_SURFACE_VISIBLE)) {
                return !mIsIncognito
                        || (mIsIncognito
                                && mSecondaryTasksSurfacePropertyModel.get(
                                        IS_FAKE_SEARCH_BOX_VISIBLE));
            }
        }

        // Do not show Tab switcher toolbar on explore pane.
        if (mSurfaceMode == SurfaceMode.TWO_PANES
                && mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE)) {
            return false;
        }

        // Do not show Tab switcher toolbar when focusing the Omnibox.
        return mPropertyModel.get(IS_FAKE_SEARCH_BOX_VISIBLE);
    }

    private void setTabCarouselVisibility(boolean isVisible) {
        assert !mIsIncognito;

        if (isVisible == mPropertyModel.get(IS_TAB_CAROUSEL_VISIBLE)) return;

        // Hide the more Tabs view when the last Tab is closed.
        if (!isVisible && mSecondaryTasksSurfaceController != null
                && mSecondaryTasksSurfaceController.overviewVisible()) {
            setSecondaryTasksSurfaceVisibility(false);
            setExploreSurfaceVisibility(true);
        }

        mPropertyModel.set(IS_TAB_CAROUSEL_VISIBLE, isVisible);
    }
}
