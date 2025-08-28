// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_suggestion.toolbar.GroupSuggestionsButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.function.Supplier;

@NullMarked
public class TabGroupingActionProvider implements ContextualPageActionController.ActionProvider {

    private final Supplier<@Nullable GroupSuggestionsButtonController>
            mGroupSuggestionsButtonControllerSupplier;

    public TabGroupingActionProvider(
            Supplier<@Nullable GroupSuggestionsButtonController> groupSuggestionsButtonController) {
        mGroupSuggestionsButtonControllerSupplier = groupSuggestionsButtonController;
    }

    @Override
    public void onActionShown(@Nullable Tab tab, @AdaptiveToolbarButtonVariant int action) {
        var groupSuggestionsButtonController = mGroupSuggestionsButtonControllerSupplier.get();
        if (groupSuggestionsButtonController == null) {
            return;
        }

        var controller = assumeNonNull(mGroupSuggestionsButtonControllerSupplier.get());
        if (action == AdaptiveToolbarButtonVariant.TAB_GROUPING) {
            controller.onButtonShown(tab);
        } else {
            controller.onButtonHidden();
        }
    }

    @Override
    public void getAction(Tab tab, SignalAccumulator signalAccumulator) {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (tab == null
                            || tab.getWindowAndroid() == null
                            || !(mGroupSuggestionsButtonControllerSupplier.get() != null)) {
                        signalAccumulator.setSignal(
                                AdaptiveToolbarButtonVariant.TAB_GROUPING, false);
                        return;
                    }

                    var tabWindow = tab.getWindowAndroid();
                    // Action not supported on tablet UI.
                    if (tabWindow.getContext() == null
                            || DeviceFormFactor.isWindowOnTablet(tabWindow)) {
                        signalAccumulator.setSignal(
                                AdaptiveToolbarButtonVariant.TAB_GROUPING, false);
                        return;
                    }

                    var activity = tabWindow.getActivity().get();
                    if (activity == null) {
                        signalAccumulator.setSignal(
                                AdaptiveToolbarButtonVariant.TAB_GROUPING, false);
                        return;
                    }

                    var windowId = TabWindowManagerSingleton.getInstance().getIdForWindow(activity);

                    var controller = assumeNonNull(mGroupSuggestionsButtonControllerSupplier.get());
                    signalAccumulator.setSignal(
                            AdaptiveToolbarButtonVariant.TAB_GROUPING,
                            controller.shouldShowButton(tab, windowId));
                });
    }
}
