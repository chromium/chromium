// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Intent;
import android.util.Pair;

import org.chromium.base.Callback;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.CustomTabsTabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory;
import org.chromium.chrome.browser.customtabs.CustomTabTabPersistencePolicy;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.util.function.Supplier;

/**
 * Creates {@link Tab}, {@link TabModelSelector}, and {@link ChromeTabCreator}s in the context of a
 * Custom Tab activity.
 */
@NullMarked
public class CustomTabActivityTabFactory {
    private final Activity mActivity;
    private final CustomTabTabPersistencePolicy mPersistencePolicy;
    private final ActivityWindowAndroid mActivityWindowAndroid;
    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;
    private final CustomTabDelegateFactory mCustomTabDelegateFactory;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final TabCreatorManager mTabCreatorManager;
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final Supplier<CompositorViewHolder> mCompositorViewHolderSupplier;
    private final CipherFactory mCipherFactory;

    private @Nullable CustomTabsTabModelOrchestrator mTabModelOrchestrator;
    @ActivityType int mActivityType;

    public CustomTabActivityTabFactory(
            Activity activity,
            CustomTabTabPersistencePolicy persistencePolicy,
            ActivityWindowAndroid activityWindowAndroid,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            CustomTabDelegateFactory customTabDelegateFactory,
            BrowserServicesIntentDataProvider intentDataProvider,
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
        assumeNonNull(mTabModelOrchestrator)
                .createTabModels(
                        mActivity,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mPersistencePolicy,
                        mActivityType,
                        AsyncTabParamsManagerSingleton.getInstance(),
                        mCipherFactory);
    }

    /** Returns the previously created {@link TabModelSelector}. */
    public TabModelSelectorBase getTabModelSelector() {
        TabModelOrchestrator orchestrator = getTabModelOrchestrator();
        if (orchestrator.getTabModelSelector() == null) {
            assert false;
            createTabModels();
        }
        return assertNonNull(orchestrator.getTabModelSelector());
    }

    /** Returns the previously created {@link CustomTabsTabModelOrchestrator}. */
    public TabModelOrchestrator getTabModelOrchestrator() {
        TabModelOrchestrator ret = mTabModelOrchestrator;
        if (ret == null) {
            assert false;
            ret = createTabModelOrchestrator();
        }
        return ret;
    }

    /** Creates a {@link ChromeTabCreator}s for the custom tab. */
    public Pair<ChromeTabCreator, ChromeTabCreator> createTabCreators() {
        return Pair.create(createTabCreator(false), createTabCreator(true));
    }

    private ChromeTabCreator createTabCreator(boolean incognito) {
        return new ChromeTabCreator(
                mActivity,
                mActivityWindowAndroid,
                () -> mCustomTabDelegateFactory,
                mProfileProviderSupplier,
                incognito,
                AsyncTabParamsManagerSingleton.getInstance(),
                mTabModelSelectorSupplier,
                mCompositorViewHolderSupplier,
                /* multiInstanceManager= */ null);
    }

    /** Creates a new tab for a Custom Tab activity */
    public Tab createTab(
            WebContents webContents, TabDelegateFactory delegateFactory, Callback<Tab> action) {
        Intent intent = mIntentDataProvider.getIntent();
        assertNonNull(mProfileProviderSupplier.get());
        Profile profile =
                ProfileProvider.getOrCreateProfile(
                        mProfileProviderSupplier.get(), mIntentDataProvider.isOffTheRecord());
        return TabBuilder.createLiveTab(profile, /* initiallyHidden= */ false)
                .setId(IntentHandler.getTabId(intent))
                .setWindow(mActivityWindowAndroid)
                .setLaunchType(TabLaunchType.FROM_EXTERNAL_APP)
                .setWebContents(webContents)
                .setDelegateFactory(delegateFactory)
                .setPreInitializeAction(action)
                .build();
    }
}
