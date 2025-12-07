// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabwindow.TabWindowInfo;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

import java.util.function.Consumer;
import java.util.function.Supplier;

/** Handle the events related to {@link OmniboxAction}. */
@NullMarked
public class OmniboxActionDelegateImpl implements OmniboxActionDelegate {
    /** Callback to bring the tab window to foreground and switch to the tab. */
    @NullMarked
    public interface BringTabToFrontCallback {
        /**
         * Invoked to bring the tab window to foreground and switch to the tab.
         *
         * @param tabWindowInfo The info of the {@link Tab}.
         * @param url The page url of the {@link Tab}.
         */
        void onResult(TabWindowInfo tabWindowInfo, GURL url);
    }

    private final Context mContext;
    private final Consumer<String> mOpenUrlInExistingTabElseNewTabCb;
    private final Runnable mOpenIncognitoTabCb;
    private final Runnable mOpenPasswordSettingsCb;
    private final Supplier<@Nullable Tab> mTabSupplier;
    private final @Nullable Runnable mOpenQuickDeleteCb;
    private final Supplier<TabWindowManager> mTabWindowManagerSupplier;
    private final BringTabToFrontCallback mBringTabToFrontCallback;

    public OmniboxActionDelegateImpl(
            Context context,
            Supplier<@Nullable Tab> tabSupplier,
            Consumer<String> openUrlInExistingTabElseNewTabCb,
            Runnable openIncognitoTabCb,
            Runnable openPasswordSettingsCb,
            @Nullable Runnable openQuickDeleteCb,
            Supplier<TabWindowManager> tabWindowManagerSupplier,
            BringTabToFrontCallback bringTabToFrontCallback) {
        mContext = context;
        mTabSupplier = tabSupplier;
        mOpenUrlInExistingTabElseNewTabCb = openUrlInExistingTabElseNewTabCb;
        mOpenIncognitoTabCb = openIncognitoTabCb;
        mOpenPasswordSettingsCb = openPasswordSettingsCb;
        mOpenQuickDeleteCb = openQuickDeleteCb;
        mTabWindowManagerSupplier = tabWindowManagerSupplier;
        mBringTabToFrontCallback = bringTabToFrontCallback;
    }

    @Override
    public void handleClearBrowsingData() {
        if (mOpenQuickDeleteCb != null) {
            mOpenQuickDeleteCb.run();
        } else {
            openSettingsPage(SettingsFragment.CLEAR_BROWSING_DATA);
        }
    }

    @Override
    public void openIncognitoTab() {
        mOpenIncognitoTabCb.run();
    }

    @Override
    public void openPasswordManager() {
        mOpenPasswordSettingsCb.run();
    }

    @Override
    public void openSettingsPage(@SettingsFragment int fragment) {
        SettingsNavigationFactory.createSettingsNavigation().startSettings(mContext, fragment);
    }

    @Override
    public boolean isIncognito() {
        var tab = mTabSupplier.get();
        return (tab != null && tab.isIncognito());
    }

    @Override
    public void loadPageInCurrentTab(String url) {
        var tab = mTabSupplier.get();
        if (tab != null && tab.isUserInteractable()) {
            tab.loadUrl(new LoadUrlParams(url));
        } else {
            mOpenUrlInExistingTabElseNewTabCb.accept(url);
        }
    }

    @Override
    public boolean startActivity(Intent intent) {
        try {
            if (IntentUtils.intentTargetsSelf(intent)) {
                IntentUtils.addTrustedIntentExtras(intent);
            }
            mContext.startActivity(intent);
            return true;
        } catch (ActivityNotFoundException e) {
        }
        return false;
    }

    @Override
    public boolean switchToTab(int tabId, GURL url) {
        TabWindowManager tabWindowManager = mTabWindowManagerSupplier.get();
        if (tabWindowManager == null) return false;

        TabWindowInfo tabWindowInfo = tabWindowManager.getTabWindowInfoById(tabId);
        if (tabWindowInfo == null || tabWindowInfo.windowId == TabWindowManager.INVALID_WINDOW_ID) {
            return false;
        }

        mBringTabToFrontCallback.onResult(tabWindowInfo, url);
        return true;
    }
}
