// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.single_tab;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.magic_stack.HomeModulesCoordinator;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.magic_stack.ModuleDelegateHost;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.magic_stack.ModuleProviderBuilder;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.function.BooleanSupplier;

/** The {@link ModuleProviderBuilder} to build the single tab module on the magic stack. */
@NullMarked
public class SingleTabModuleBuilder implements ModuleProviderBuilder {
    private final Activity mActivity;
    private final MonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final MonotonicObservableSupplier<TabContentManager> mTabContentManagerSupplier;
    private final BooleanSupplier mUseManualRankSupplier;

    /**
     * @param activity The instance of {@link Activity}.
     * @param tabModelSelectorSupplier The supplier of the {@link TabModelSelector}.
     * @param tabContentManagerSupplier The supplier of the {@link TabContentManager}.
     * @param useManualRankSupplier The supplier of whether to use a manual rank.
     */
    public SingleTabModuleBuilder(
            Activity activity,
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            MonotonicObservableSupplier<TabContentManager> tabContentManagerSupplier,
            BooleanSupplier useManualRankSupplier) {
        mActivity = activity;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mTabContentManagerSupplier = tabContentManagerSupplier;
        mUseManualRankSupplier = useManualRankSupplier;
    }

    // ModuleProviderBuilder implementation.

    @Override
    public boolean build(
            ModuleDelegate moduleDelegate, Callback<ModuleProvider> onModuleBuiltCallback) {
        ModuleDelegateHost moduleDelegateHost =
                ((HomeModulesCoordinator) moduleDelegate).getModuleDelegateHost();
        Callback<Integer> singleTabCardClickedCallback =
                (tabId) -> {
                    moduleDelegate.onTabClicked(tabId, ModuleType.SINGLE_TAB);
                };
        Runnable seeMoreLinkClickedCallback =
                () -> {
                    moduleDelegate.onUrlClicked(
                            new GURL(UrlConstants.RECENT_TABS_URL), ModuleType.SINGLE_TAB);
                };
        Runnable snapshotParentViewRunnable = moduleDelegateHost::onCaptureThumbnailStatusChanged;

        // If the host surface is NTP and there isn't a last visited Tab to track, don't create the
        // single Tab module.
        Tab trackingTab = moduleDelegate.getTrackingTab();
        if (trackingTab == null) {
            return false;
        }
        SingleTabSwitcherCoordinator singleTabSwitcherCoordinator =
                new SingleTabSwitcherCoordinator(
                        mActivity,
                        /* container= */ null,
                        assertNonNull(mTabModelSelectorSupplier.get()),
                        DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity),
                        trackingTab,
                        singleTabCardClickedCallback,
                        seeMoreLinkClickedCallback,
                        snapshotParentViewRunnable,
                        assertNonNull(mTabContentManagerSupplier.get()),
                        moduleDelegateHost.getUiConfig(),
                        moduleDelegate);
        onModuleBuiltCallback.onResult(singleTabSwitcherCoordinator);
        return true;
    }

    @Override
    public ViewGroup createView(ViewGroup parentView) {
        return (ViewGroup)
                LayoutInflater.from(mActivity)
                        .inflate(
                                SingleTabSwitcherCoordinator.getModuleLayoutId(),
                                parentView,
                                false);
    }

    @Override
    public void bind(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        SingleTabViewBinder.bind(model, view, propertyKey);
    }

    @Override
    public boolean isEligible() {
        return true;
    }

    @Override
    public @Nullable Integer getManualRank() {
        if (mUseManualRankSupplier.getAsBoolean()) {
            return 0;
        }
        return null;
    }
}
