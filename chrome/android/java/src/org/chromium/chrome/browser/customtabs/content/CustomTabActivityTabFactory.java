// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import android.content.Intent;
import android.util.Pair;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabTabPersistencePolicy;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.init.StartupTabPreloader;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Creates {@link Tab}, {@link TabModelSelector}, and {@link ChromeTabCreator}s in the context of a
 * Custom Tab activity.
 */
@ActivityScope
public class CustomTabActivityTabFactory {
    private final ChromeActivity mActivity;
    private final CustomTabTabPersistencePolicy mPersistencePolicy;
    private final Lazy<ActivityWindowAndroid> mActivityWindowAndroid;
    private final Lazy<CustomTabDelegateFactory> mCustomTabDelegateFactory;
    private final CustomTabIntentDataProvider mIntentDataProvider;
    private final StartupTabPreloader mStartupTabPreloader;

    @Nullable
    private TabModelSelectorImpl mTabModelSelector;

    @Inject
    public CustomTabActivityTabFactory(ChromeActivity activity,
            CustomTabTabPersistencePolicy persistencePolicy,
            Lazy<ActivityWindowAndroid> activityWindowAndroid,
            Lazy<CustomTabDelegateFactory> customTabDelegateFactory,
            CustomTabIntentDataProvider intentDataProvider,
            StartupTabPreloader startupTabPreloader) {
        mActivity = activity;
        mPersistencePolicy = persistencePolicy;
        mActivityWindowAndroid = activityWindowAndroid;
        mCustomTabDelegateFactory = customTabDelegateFactory;
        mIntentDataProvider = intentDataProvider;
        mStartupTabPreloader = startupTabPreloader;
    }

    /** Creates a {@link TabModelSelector} for the custom tab. */
    public TabModelSelectorImpl createTabModelSelector() {
        mTabModelSelector = new TabModelSelectorImpl(
                mActivity, mActivity, mPersistencePolicy, false, false, false);
        return mTabModelSelector;
    }

    /** Returns the previously created {@link TabModelSelector}. */
    public TabModelSelectorImpl getTabModelSelector() {
        if (mTabModelSelector == null) {
            assert false;
            return createTabModelSelector();
        }
        return mTabModelSelector;
    }

    /** Creates a {@link ChromeTabCreator}s for the custom tab. */
    public Pair<ChromeTabCreator, ChromeTabCreator> createTabCreators() {
        return Pair.create(createTabCreator(false), createTabCreator(true));
    }

    private ChromeTabCreator createTabCreator(boolean incognito) {
        return new ChromeTabCreator(
                mActivity, mActivityWindowAndroid.get(), mStartupTabPreloader, incognito) {
            @Override
            public TabDelegateFactory createDefaultTabDelegateFactory() {
                return mCustomTabDelegateFactory.get();
            }
        };
    }

    /** Creates a new tab for a Custom Tab activity */
    public Tab createTab(WebContents webContents, TabDelegateFactory delegateFactory) {
        Intent intent = mIntentDataProvider.getIntent();
        int assignedTabId =
                IntentUtils.safeGetIntExtra(intent, IntentHandler.EXTRA_TAB_ID, Tab.INVALID_TAB_ID);
        int parentTabId = IntentUtils.safeGetIntExtra(
                intent, IntentHandler.EXTRA_PARENT_TAB_ID, Tab.INVALID_TAB_ID);
        Tab parent = mTabModelSelector != null ? mTabModelSelector.getTabById(parentTabId) : null;
        return new TabBuilder()
                .setId(assignedTabId)
                .setParent(parent)
                .setIncognito(mIntentDataProvider.isIncognito())
                .setWindow(mActivityWindowAndroid.get())
                .setLaunchType(TabLaunchType.FROM_EXTERNAL_APP)
                .setWebContents(webContents)
                .setDelegateFactory(delegateFactory)
                .build();
    }

    /**
     * This method is to circumvent calling final {@link ChromeActivity#initializeTabModels} in unit
     * tests for {@link CustomTabActivityTabController}.
     * TODO(pshmakov): remove once mock-maker-inline is introduced.
     */
    public void initializeTabModels() {
        mActivity.initializeTabModels();
    }
}
