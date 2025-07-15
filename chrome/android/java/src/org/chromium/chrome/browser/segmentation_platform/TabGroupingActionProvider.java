// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_suggestion.toolbar.GroupSuggestionsButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.ui.base.DeviceFormFactor;

@NullMarked
public class TabGroupingActionProvider implements ContextualPageActionController.ActionProvider {

    private final GroupSuggestionsButtonController mGroupSuggestionsButtonController;

    public TabGroupingActionProvider(
            GroupSuggestionsButtonController groupSuggestionsButtonController) {
        mGroupSuggestionsButtonController = groupSuggestionsButtonController;
    }

    @Override
    public void onActionShown(Tab tab, @AdaptiveToolbarButtonVariant int action) {
        if (action == AdaptiveToolbarButtonVariant.TAB_GROUPING) {
            mGroupSuggestionsButtonController.onButtonShown(tab);
        } else {
            mGroupSuggestionsButtonController.onButtonHidden();
        }
    }

    @Override
    public void getAction(Tab tab, SignalAccumulator signalAccumulator) {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (tab == null || tab.getWindowAndroid() == null) {
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

                    signalAccumulator.setSignal(
                            AdaptiveToolbarButtonVariant.TAB_GROUPING,
                            mGroupSuggestionsButtonController.shouldShowButton(tab, windowId));
                });
    }
}
