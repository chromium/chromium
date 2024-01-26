// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.view.View.OnClickListener;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.hub.DelegateButtonData;
import org.chromium.chrome.browser.hub.DrawableButtonData;
import org.chromium.chrome.browser.hub.HubColorScheme;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.ResourceButtonData;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.tab_ui.R;

/** A {@link Pane} representing the regular tab switcher. */
public class TabSwitcherPane extends TabSwitcherPaneBase {
    private final @NonNull SharedPreferences mSharedPreferences;
    private final @NonNull Supplier<TabModelFilter> mTabModelFilterSupplier;
    private final @NonNull TabSwitcherPaneDrawableCoordinator mTabSwitcherPaneDrawableCoordinator;

    private @Nullable OnSharedPreferenceChangeListener mPriceAnnotationsPrefListener;

    /**
     * @param context The activity context.
     * @param sharedPreferences The app shared preferences.
     * @param profileProviderSupplier The profile provider supplier.
     * @param factory The factory used to construct {@link TabSwitcherPaneCoordinator}s.
     * @param tabModelFilterSupplier The supplier of the regular {@link TabModelFilter}.
     * @param newTabButtonClickListener The {@link OnClickListener} for the new tab button.
     * @param tabSwitcherDrawableCoordinator The drawable to represent the pane.
     */
    TabSwitcherPane(
            @NonNull Context context,
            @NonNull SharedPreferences sharedPreferences,
            @NonNull OneshotSupplier<ProfileProvider> profileProviderSupplier,
            @NonNull TabSwitcherPaneCoordinatorFactory factory,
            @NonNull Supplier<TabModelFilter> tabModelFilterSupplier,
            @NonNull OnClickListener newTabButtonClickListener,
            @NonNull TabSwitcherPaneDrawableCoordinator tabSwitcherDrawableCoordinator) {
        super(
                context,
                factory,
                /* isIncognito= */ false);
        mSharedPreferences = sharedPreferences;
        mTabModelFilterSupplier = tabModelFilterSupplier;
        mTabSwitcherPaneDrawableCoordinator = tabSwitcherDrawableCoordinator;

        // TODO(crbug/1505772): Update this string to not be an a11y string and it should probably
        // just say "Tabs".
        mReferenceButtonDataSupplier.set(
                new DrawableButtonData(
                        R.string.accessibility_tab_switcher_standard_stack,
                        R.string.accessibility_tab_switcher_standard_stack,
                        tabSwitcherDrawableCoordinator.getTabSwitcherDrawable()));

        mNewTabButtonDataSupplier.set(
                new DelegateButtonData(
                        new ResourceButtonData(
                                R.string.button_new_tab,
                                R.string.button_new_tab,
                                R.drawable.new_tab_icon),
                        () -> newTabButtonClickListener.onClick(null)));

        profileProviderSupplier.onAvailable(this::onProfileProviderAvailable);
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.TAB_SWITCHER;
    }

    @Override
    public @HubColorScheme int getColorScheme() {
        return HubColorScheme.DEFAULT;
    }

    @Override
    public void destroy() {
        super.destroy();
        mTabSwitcherPaneDrawableCoordinator.destroy();
        if (mPriceAnnotationsPrefListener != null) {
            mSharedPreferences.unregisterOnSharedPreferenceChangeListener(
                    mPriceAnnotationsPrefListener);
        }
    }

    @Override
    public void showAllTabs() {
        resetWithTabList(mTabModelFilterSupplier.get(), false);
    }

    @Override
    public int getCurrentTabId() {
        return TabModelUtils.getCurrentTabId(mTabModelFilterSupplier.get().getTabModel());
    }

    @Override
    public boolean resetWithTabList(@Nullable TabList tabList, boolean quickMode) {
        @Nullable TabSwitcherPaneCoordinator coordinator = getTabSwitcherPaneCoordinator();
        if (coordinator == null) return false;

        boolean isNotVisibleOrSelected =
                !isVisible() || !mTabModelFilterSupplier.get().isCurrentlySelectedFilter();

        if (isNotVisibleOrSelected) {
            coordinator.resetWithTabList(null);
        } else {
            coordinator.resetWithTabList(tabList);
        }
        return true;
    }

    private void onProfileProviderAvailable(@NonNull ProfileProvider profileProvider) {
        if (!PriceTrackingFeatures.isPriceTrackingEnabled(profileProvider.getOriginalProfile())
                && getTabListMode() == TabListMode.GRID) {
            return;
        }
        mPriceAnnotationsPrefListener =
                (sharedPrefs, key) -> {
                    if (!PriceTrackingUtilities.TRACK_PRICES_ON_TABS.equals(key) || !isVisible()) {
                        return;
                    }
                    TabModelFilter filter = mTabModelFilterSupplier.get();
                    @Nullable
                    TabSwitcherPaneCoordinator coordinator = getTabSwitcherPaneCoordinator();
                    if (filter.isCurrentlySelectedFilter()
                            && filter.isTabModelRestored()
                            && coordinator != null) {
                        coordinator.resetWithTabList(filter);
                    }
                };
        mSharedPreferences.registerOnSharedPreferenceChangeListener(mPriceAnnotationsPrefListener);
    }
}
