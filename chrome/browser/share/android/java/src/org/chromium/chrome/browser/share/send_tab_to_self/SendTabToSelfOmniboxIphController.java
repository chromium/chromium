// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSupplierObserver;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.PageTransition;

/**
 * Manages showing the Send Tab to Self omnibox IPH after the first user navigation or tab switch.
 */
@NullMarked
public class SendTabToSelfOmniboxIphController {
    private final UserEducationHelper mUserEducationHelper;
    private final View mAnchorView;
    private final TabSupplierObserver mTabObserver;

    private int mLastTabId = Tab.INVALID_TAB_ID;

    /**
     * Creates the controller, or returns null if the IPH would not trigger.
     *
     * @param activity The current activity.
     * @param profile The current Profile.
     * @param tabSupplier Supplies the current active tab.
     * @param anchorView The anchor view for the IPH bubble.
     */
    public static @Nullable SendTabToSelfOmniboxIphController create(
            Activity activity,
            Profile profile,
            NullableObservableSupplier<Tab> tabSupplier,
            View anchorView) {
        if (!TrackerFactory.getTrackerForProfile(profile)
                .wouldTriggerHelpUi(FeatureConstants.SEND_TAB_TO_SELF_OMNIBOX)) {
            return null;
        }
        return new SendTabToSelfOmniboxIphController(activity, profile, tabSupplier, anchorView);
    }

    private SendTabToSelfOmniboxIphController(
            Activity activity,
            Profile profile,
            NullableObservableSupplier<Tab> tabSupplier,
            View anchorView) {
        mUserEducationHelper =
                new UserEducationHelper(activity, profile, new Handler(Looper.getMainLooper()));
        mAnchorView = anchorView;
        mTabObserver =
                new TabSupplierObserver(tabSupplier, /* shouldTrigger= */ true) {
                    // Fires on active tab changes, and with null while the hub is shown.
                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        maybeShowIphOnTabSwitch(tab);
                    }

                    // Covers a tab picked in the hub, which becomes interactable after the hub
                    // hides.
                    @Override
                    public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
                        maybeShowIphOnTabSwitch(tab);
                    }

                    @Override
                    public void onDidFinishNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        int transition = navigation.pageTransition();
                        int core = transition & PageTransition.CORE_MASK;
                        // Only loads that landed on a new page.
                        if (navigation.hasCommitted()
                                && !navigation.isSameDocument()
                                // Reload button, pull-to-refresh and offline auto-reloads.
                                && !navigation.isReload()
                                // Session restores and reloads of killed or discarded tabs.
                                && core != PageTransition.RELOAD
                                // Startup loads, e.g. from a widget or homescreen shortcut.
                                && core != PageTransition.AUTO_TOPLEVEL
                                // Loads started by other apps.
                                && (transition & PageTransition.FROM_API) == 0
                                // Started from browser UI, or by the page after a user tap.
                                && (!navigation.isRendererInitiated()
                                        || navigation.hasUserGesture())) {
                            maybeShowIph(tab);
                        }
                    }
                };
    }

    private void maybeShowIphOnTabSwitch(@Nullable Tab tab) {
        // Ignore null or non-interactable tabs (e.g. while the hub is shown or hiding).
        if (tab == null || !tab.isUserInteractable()) return;
        int tabId = tab.getId();
        if (mLastTabId != Tab.INVALID_TAB_ID && tabId != mLastTabId) maybeShowIph(tab);
        mLastTabId = tabId;
    }

    /** Attempts to show the Send Tab to Self Omnibox IPH if eligible. */
    private void maybeShowIph(Tab tab) {
        if (!mAnchorView.isShown()) return;
        @EntryPointDisplayReason
        Integer displayReason =
                SendTabToSelfAndroidBridge.getEntryPointDisplayReason(
                        tab.getProfile(), tab.getUrl().getSpec());
        if (displayReason != null && displayReason == EntryPointDisplayReason.OFFER_FEATURE) {
            mTabObserver.destroy();
            mUserEducationHelper.requestShowIph(
                    new IphCommandBuilder(
                                    mAnchorView.getContext().getResources(),
                                    FeatureConstants.SEND_TAB_TO_SELF_OMNIBOX,
                                    R.string.send_tab_to_self_omnibox_iph_text,
                                    R.string.send_tab_to_self_omnibox_iph_text)
                            .setAnchorView(mAnchorView)
                            .build());
        }
    }

    public void destroy() {
        mTabObserver.destroy();
    }
}
