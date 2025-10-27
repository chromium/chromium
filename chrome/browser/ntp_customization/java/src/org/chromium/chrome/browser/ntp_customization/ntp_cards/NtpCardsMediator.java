// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.ntp_cards;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.magic_stack.HomeModulesUtils.getSettingsPreferenceKey;
import static org.chromium.chrome.browser.magic_stack.HomeModulesUtils.getTitleForModuleType;
import static org.chromium.chrome.browser.magic_stack.HomeModulesUtils.updateBooleanUserPrefs;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LIST_CONTAINER_VIEW_DELEGATE;

import android.content.Context;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.internal.Nullable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.ListContainerViewDelegate;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.Map;
import java.util.function.Supplier;

/**
 * A mediator class that manages navigation between bottom sheets and manages the container view on
 * the NTP cards bottom sheet.
 */
@NullMarked
public class NtpCardsMediator {

    // LINT.IfChange(HomeModuleTypes)
    private static final Map<Integer, String> MODULE_TYPE_TO_USER_PREFS_KEY =
            Map.of(
                    ModuleType.SINGLE_TAB, Pref.TAB_RESUMPTION_HOME_MODULE_ENABLED,
                    ModuleType.PRICE_CHANGE, Pref.PRICE_TRACKING_HOME_MODULE_ENABLED,
                    ModuleType.SAFETY_HUB, Pref.SAFETY_CHECK_HOME_MODULE_ENABLED,
                    // We only need one "Chrome Tips" module
                    // TODO(crbug.com/441782354): Update this once Chrome Tips pref is available
                    ModuleType.DEFAULT_BROWSER_PROMO, "");
    // LINT.ThenChange()

    private final PropertyModel mContainerPropertyModel;
    private final PropertyModel mBottomSheetPropertyModel;
    private final Supplier<@Nullable Profile> mProfileSupplier;

    public NtpCardsMediator(
            PropertyModel containerPropertyModel,
            PropertyModel bottomSheetPropertyModel,
            BottomSheetDelegate delegate,
            Supplier<@Nullable Profile> profileSupplier) {
        mContainerPropertyModel = containerPropertyModel;
        mBottomSheetPropertyModel = bottomSheetPropertyModel;
        mProfileSupplier = profileSupplier;

        mContainerPropertyModel.set(LIST_CONTAINER_VIEW_DELEGATE, createListDelegate());
        // Hides the back button when the NTP Cards bottom sheet is displayed standalone.
        mBottomSheetPropertyModel.set(
                BACK_PRESS_HANDLER,
                delegate.shouldShowAlone() ? null : v -> delegate.backPressOnCurrentBottomSheet());
    }

    /** Returns {@link ListContainerViewDelegate} that defines the content of each list item. */
    @VisibleForTesting
    ListContainerViewDelegate createListDelegate() {
        return new ListContainerViewDelegate() {
            @Override
            public List<Integer> getListItems() {
                HomeModulesConfigManager homeModulesConfigManager =
                        HomeModulesConfigManager.getInstance();
                return homeModulesConfigManager.getModuleListShownInSettings();
            }

            @Override
            public String getListItemTitle(int type, Context context) {
                return getTitleForModuleType(type, context);
            }

            @Override
            public @Nullable String getListItemSubtitle(int type, Context context) {
                return null;
            }

            @Override
            public View.@Nullable OnClickListener getListener(int type) {
                return null;
            }

            @Override
            public @Nullable Integer getTrailingIcon(int type) {
                return null;
            }

            @Override
            public @Nullable Integer getTrailingIconDescriptionResId(int type) {
                return null;
            }
        };
    }

    @VisibleForTesting
    void updateUserPrefs() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.HOME_MODULE_PREF_REFACTOR)) return;

        @Nullable Profile profile = mProfileSupplier.get();
        if (profile == null) return; // Return if profile not ready yet

        for (@ModuleType int moduleType : MODULE_TYPE_TO_USER_PREFS_KEY.keySet()) {
            updateBooleanUserPrefs(
                    getSettingsPreferenceKey(moduleType),
                    assumeNonNull(MODULE_TYPE_TO_USER_PREFS_KEY.get(moduleType)),
                    profile);
        }
    }

    /** Clears the back press handler and click listeners of NTP cards bottom sheet. */
    void destroy() {
        mBottomSheetPropertyModel.set(BACK_PRESS_HANDLER, null);
        mContainerPropertyModel.set(LIST_CONTAINER_VIEW_DELEGATE, null);
        updateUserPrefs();
    }
}
