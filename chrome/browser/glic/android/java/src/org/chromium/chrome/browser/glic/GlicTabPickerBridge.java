// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.app.Activity;
import android.content.Intent;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.browser_ui.util.ChromeItemPickerUtils;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;

/** Bridge between native C++ Glic and Java to launch the tab picker. */
@JNINamespace("glic")
@NullMarked
class GlicTabPickerBridge {

    @CalledByNative
    static void openTabPicker(
            @JniType("ui::WindowAndroid*") @Nullable WindowAndroid windowAndroid,
            @JniType("std::vector<TabAndroid*>") List<Tab> alreadySelectedTabs,
            @JniType("base::OnceCallback<void(std::optional<std::vector<TabAndroid*>>)>&&")
                    Callback<@Nullable List<Tab>> callback) {
        if (windowAndroid == null) {
            callback.onResult(null);
            return;
        }

        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            callback.onResult(null);
            return;
        }

        List<@TabId Integer> preselectedTabIds = new ArrayList<>(alreadySelectedTabs.size());
        for (Tab tab : alreadySelectedTabs) {
            preselectedTabIds.add(tab.getId());
        }

        Intent intent =
                ChromeItemPickerUtils.createChromeItemPickerIntent(activity, preselectedTabIds);
        if (intent == null) {
            callback.onResult(null);
            return;
        }

        boolean shown =
                windowAndroid.showIntent(
                        intent,
                        (resultCode, resultIntent) -> {
                            if (resultCode != Activity.RESULT_OK || resultIntent == null) {
                                callback.onResult(null);
                                return;
                            }
                            List<Integer> selectedTabIds =
                                    ChromeItemPickerUtils.getSelectedTabIdsFromIntent(resultIntent);
                            if (selectedTabIds == null) {
                                callback.onResult(null);
                                return;
                            }

                            TabModelSelector tabModelSelector =
                                    TabModelSelectorSupplier.getValueOrNullFrom(windowAndroid);
                            if (tabModelSelector == null) {
                                callback.onResult(null);
                                return;
                            }

                            List<Tab> selectedTabs = new ArrayList<>(selectedTabIds.size());
                            for (@TabId int tabId : selectedTabIds) {
                                Tab tab = tabModelSelector.getTabById(tabId);
                                if (tab != null) {
                                    selectedTabs.add(tab);
                                }
                            }
                            callback.onResult(selectedTabs);
                        },
                        /* errorId= */ null);

        if (!shown) {
            callback.onResult(null);
        }
    }
}
