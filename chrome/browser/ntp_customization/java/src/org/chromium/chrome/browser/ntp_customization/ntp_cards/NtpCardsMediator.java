// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.ntp_cards;

import static org.chromium.chrome.browser.magic_stack.HomeModulesUtils.getTitleForModuleType;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LIST_CONTAINER_VIEW_DELEGATE;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.view.View;

import org.jni_zero.internal.Nullable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.ListContainerViewDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * A mediator class that manages navigation between bottom sheets and manages the container view on
 * the NTP cards bottom sheet.
 */
@NullMarked
public class NtpCardsMediator {

    private final PropertyModel mContainerPropertyModel;
    private final PropertyModel mBottomSheetPropertyModel;

    public NtpCardsMediator(
            PropertyModel containerPropertyModel,
            PropertyModel bottomSheetPropertyModel,
            BottomSheetDelegate delegate) {
        mContainerPropertyModel = containerPropertyModel;
        mBottomSheetPropertyModel = bottomSheetPropertyModel;

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
                return getTitleForModuleType(type, context.getResources());
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
        };
    }

    /** Clears the back press handler and click listeners of NTP cards bottom sheet. */
    void destroy() {
        mBottomSheetPropertyModel.set(BACK_PRESS_HANDLER, null);
        mContainerPropertyModel.set(LIST_CONTAINER_VIEW_DELEGATE, null);
    }
}
