// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * This class is responsible for pushing updates the view of the tab switcher bottom toolbar. These
 * updates are pulled from the {@link TabSwitcherBottomToolbarModel} when a notification of an
 * update is received.
 */
public class TabSwitcherBottomToolbarViewBinder
        implements PropertyModelChangeProcessor
                           .ViewBinder<TabSwitcherBottomToolbarModel, View, PropertyKey> {
    private final ViewGroup mTopRoot;
    private final ViewGroup mBottomRoot;

    /**
     * Build a binder that handles interaction between the model and the tab switcher bottom toolbar
     * view.
     */
    TabSwitcherBottomToolbarViewBinder(ViewGroup topRoot, ViewGroup bottomRoot) {
        mTopRoot = topRoot;
        mBottomRoot = bottomRoot;
    }

    @Override
    public final void bind(
            TabSwitcherBottomToolbarModel model, View view, PropertyKey propertyKey) {
        if (TabSwitcherBottomToolbarModel.IS_VISIBLE == propertyKey) {
            view.setVisibility(
                    model.get(TabSwitcherBottomToolbarModel.IS_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (TabSwitcherBottomToolbarModel.PRIMARY_COLOR == propertyKey) {
            view.findViewById(R.id.bottom_toolbar_buttons)
                    .setBackgroundColor(model.get(TabSwitcherBottomToolbarModel.PRIMARY_COLOR));
        } else if (TabSwitcherBottomToolbarModel.SHOW_ON_TOP == propertyKey) {
            final boolean showOnTop = model.get(TabSwitcherBottomToolbarModel.SHOW_ON_TOP);
            view.findViewById(R.id.bottom_toolbar_bottom_shadow)
                    .setVisibility(showOnTop ? View.VISIBLE : View.GONE);
            // When shown on the bottom, the layout should match_parent so that it fills its
            // parent container. When the layout is shown on the top, it should wrap_content
            // so that the toolbar shadow is visible.
            view.getLayoutParams().height = showOnTop ? ViewGroup.LayoutParams.WRAP_CONTENT
                                                      : ViewGroup.LayoutParams.MATCH_PARENT;
            reparentView(view, showOnTop ? mTopRoot : mBottomRoot);
        } else {
            assert false : "Unhandled property detected in TabSwitcherBottomToolbarViewBinder!";
        }
    }

    private static void reparentView(View v, ViewGroup newParent) {
        UiUtils.removeViewFromParent(v);
        newParent.addView(v);
    }
}
