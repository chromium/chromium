// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import android.app.Activity;
import android.content.Intent;
import android.util.Pair;

import androidx.annotation.Nullable;

import dagger.Lazy;

import org.chromium.base.Callback;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.CustomTabsTabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory;
import org.chromium.chrome.browser.customtabs.CustomTabTabPersistencePolicy;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;

import javax.inject.Inject;

/**
 * Creates {@link Tab}, {@link TabModelSelector}, and {@link ChromeTabCreator}s in the context of a
 * Custom Tab activity.
 */
@ActivityScope
public class CustomTabActivityTabFactory {
    private final Activity mActivity;
    private final CustomTabTabPersistencePolicy mPersistencePolicy;
    private final Lazy<ActivityWindowAndroid> mActivityWindowAndroid;
    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;
    private final Lazy<CustomTabDelegateFactory> mCustomTabDelegateFactory;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final TabCreatorManager mTabCreatorManager;
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final Supplier<CompositorViewHolder> mCompositorViewHolderSupplier;
    private final CipherFactory mCipherFactory;

    private final Lazy<AsyncTabParamsManager> mAsyncTabParamsManager;

    @Nullable private CustomTabsTabModelOrchestrator mTabModelOrchestrator;
    @ActivityType int mActivityType;

    @Inject
    public CustomTabActivityTabFactory(
            Activity activity,
            CustomTabTabPersistencePolicy persistencePolicy,
            Lazy<ActivityWindowAndroid> activityWindowAndroid,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            Lazy<CustomTabDelegateFactory> customTabDelegateFactory,
            BrowserServicesIntentDataProvider intentDataProvider,
            Lazy<AsyncTabParamsManager> asyncTabParamsManager,
            TabCreatorManager tabCreatorManager,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            CipherFactory cipherFactory) {
        mActivity = activity;
        mPersistencePolicy = persistencePolicy;
        mActivityWindowAndroid = activityWindowAndroid;
        mProfileProviderSupplier = profileProviderSupplier;
        mCustomTabDelegateFactory = customTabDelegateFactory;
        mIntentDataProvider = intentDataProvider;
        mAsyncTabParamsManager = asyncTabParamsManager;
        mTabCreatorManager = tabCreatorManager;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mCompositorViewHolderSupplier = compositorViewHolderSupplier;
        mCipherFactory = cipherFactory;
    }

    public void setActivityType(int activityType) {
        mActivityType = activityType;
    }

    /** Creates a {@link TabModelOrchestrator} for the custom tab. */
    public TabModelOrchestrator createTabModelOrchestrator() {
        mTabModelOrchestrator = new CustomTabsTabModelOrchestrator();
        return mTabModelOrchestrator;
    }

    public void destroyTabModelOrchestrator() {
        if (mTabModelOrchestrator != null) {
            mTabModelOrchestrator.destroy();
        }
    }

    /** Calls the {@link TabModelOrchestrator} to create TabModels and TabPersistentStore. */
    public void createTabModels() {
        mTabModelOrchestrator.createTabModels(
                mProfileProviderSupplier,
                mTabCreatorManager,
                mPersistencePolicy,
                mActivityType,
                mAsyncTabParamsManager.get(),
                mCipherFactory);
    }

    /** Returns the previously created {@link TabModelSelector}. */
    public TabModelSelectorBase getTabModelSelector() {
        getTabModelOrchestrator();
        if (mTabModelOrchestrator.getTabModelSelector() == null) {
            assert false;
            createTabModels();
        }
        return mTabModelOrchestrator.getTabModelSelector();
    }

    /** Returns the previously created {@link CustomTabsTabModelOrchestrator}. */
    public CustomTabsTabModelOrchestrator getTabModelOrchestrator() {
        if (mTabModelOrchestrator == null) {
            assert false;
            createTabModelOrchestrator();
        }
        return mTabModelOrchestrator;
    }

    /** Creates a {@link ChromeTabCreator}s for the custom tab. */
    public Pair<ChromeTabCreator, ChromeTabCreator> createTabCreators() {
        return Pair.create(createTabCreator(false), createTabCreator(true));
    }

    private ChromeTabCreator createTabCreator(boolean incognito) {
        return new ChromeTabCreator(
                mActivity,
                mActivityWindowAndroid.get(),
                mCustomTabDelegateFactory::get,
                mProfileProviderSupplier,
                incognito,
                AsyncTabParamsManagerSingleton.getInstance(),
                mTabModelSelectorSupplier,
                mCompositorViewHolderSupplier,
                null);
    }

    /** Creates a new tab for a Custom Tab activity */
    public Tab createTab(
            WebContents webContents, TabDelegateFactory delegateFactory, Callback<Tab> action) {
        Intent intent = mIntentDataProvider.getIntent();
        Profile profile =
                ProfileProvider.getOrCreateProfile(
                        mProfileProviderSupplier.get(), mIntentDataProvider.isOffTheRecord());
        return TabBuilder.createLiveTab(profile, /* initiallyHidden= */ false)
                .setId(IntentHandler.getTabId(intent))
                .setWindow(mActivityWindowAndroid.get())
                .setLaunchType(TabLaunchType.FROM_EXTERNAL_APP)
                .setWebContents(webContents)
                .setDelegateFactory(delegateFactory)
                .setPreInitializeAction(action)
                .build();
    }
}
