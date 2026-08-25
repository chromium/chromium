// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridge.SendTabToSelfModelObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;

import java.util.function.Supplier;

/** Controller to manage when and how we show Send Tab to Self Omnibox IPH on startup. */
@NullMarked
public class SendTabToSelfOmniboxIphController implements SendTabToSelfModelObserver {
    private final UserEducationHelper mUserEducationHelper;
    private final Supplier<@Nullable Tab> mTabSupplier;
    private final View mAnchorView;

    private long mNativeModelObserver;

    /**
     * @param activity The current activity.
     * @param profile The current Profile.
     * @param tabSupplier Supplies the current active tab.
     * @param anchorView The anchor view for the IPH bubble.
     */
    public SendTabToSelfOmniboxIphController(
            Activity activity,
            Profile profile,
            Supplier<@Nullable Tab> tabSupplier,
            View anchorView) {
        mUserEducationHelper =
                new UserEducationHelper(activity, profile, new Handler(Looper.getMainLooper()));
        mTabSupplier = tabSupplier;
        mAnchorView = anchorView;
    }

    /** Attempts to show the Send Tab to Self Omnibox IPH if eligible. */
    public void maybeShowIph() {
        Tab tab = mTabSupplier.get();
        if (tab == null
                || tab.isDestroyed()
                || tab.isIncognito()
                || !ChromeFeatureList.sSendTabToSelfExtraEntryPoints.isEnabled()) {
            return;
        }

        if (SendTabToSelfAndroidBridge.isModelReady(tab.getProfile())) {
            onModelReady();
            return;
        }

        mNativeModelObserver = SendTabToSelfAndroidBridge.addModelObserver(tab.getProfile(), this);
    }

    @Override
    public void onModelReady() {
        destroy();

        Tab tab = mTabSupplier.get();
        if (tab == null || tab.isDestroyed()) return;

        @EntryPointDisplayReason
        Integer displayReason =
                SendTabToSelfAndroidBridge.getEntryPointDisplayReason(
                        tab.getProfile(), tab.getUrl().getSpec());
        if (displayReason != null && displayReason == EntryPointDisplayReason.OFFER_FEATURE) {
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
        if (mNativeModelObserver != 0) {
            SendTabToSelfAndroidBridge.removeModelObserver(mNativeModelObserver);
            mNativeModelObserver = 0;
        }
    }
}
