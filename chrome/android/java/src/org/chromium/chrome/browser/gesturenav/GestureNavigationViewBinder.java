// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import static org.chromium.chrome.browser.gesturenav.GestureNavigationProperties.ACTION;
import static org.chromium.chrome.browser.gesturenav.GestureNavigationProperties.ALLOW_NAV;
import static org.chromium.chrome.browser.gesturenav.GestureNavigationProperties.BUBBLE_OFFSET;
import static org.chromium.chrome.browser.gesturenav.GestureNavigationProperties.CLOSE_INDICATOR;
import static org.chromium.chrome.browser.gesturenav.GestureNavigationProperties.DIRECTION;
import static org.chromium.chrome.browser.gesturenav.GestureNavigationProperties.GESTURE_POS;
import static org.chromium.chrome.browser.gesturenav.GestureNavigationProperties.GLOW_OFFSET;

import android.view.View;

import org.chromium.chrome.browser.gesturenav.NavigationHandler.GestureAction;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class responsible for pushing updates to gesture navigation view.
 */
class GestureNavigationViewBinder {
    /**
     * view binder that associates a view with a model.
     * @param model The {@link PropertyObservable} model.
     * @param view The view {@link HistoryNavigationLayout} object that is changing.
     * @param key The property of the view that changed.
     */
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        HistoryNavigationLayout topView = (HistoryNavigationLayout) view;
        if (BUBBLE_OFFSET == key) {
            topView.pullBubble(model.get(BUBBLE_OFFSET));
        } else if (GLOW_OFFSET == key) {
            topView.pullGlow(model.get(GLOW_OFFSET));
        } else if (ACTION == key) {
            switch (model.get(ACTION)) {
                case GestureAction.SHOW_ARROW:
                    topView.showBubble(model.get(DIRECTION), model.get(CLOSE_INDICATOR));
                    break;
                case GestureAction.SHOW_GLOW:
                    topView.showGlow(model.get(GESTURE_POS));
                    break;
                case GestureAction.RELEASE_BUBBLE:
                    topView.releaseBubble(model.get(ALLOW_NAV));
                    break;
                case GestureAction.RELEASE_GLOW:
                    topView.releaseGlow();
                    break;
                case GestureAction.RESET_BUBBLE:
                    topView.resetBubble();
                    break;
                case GestureAction.RESET_GLOW:
                    topView.resetGlow();
                    break;
                default:
                    assert false : "Unhandled action";
            }
        }
    }
}
