// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import static org.chromium.chrome.browser.ttc.TtcSessionProperties.MIC_MUTED;
import static org.chromium.chrome.browser.ttc.TtcSessionProperties.ON_PAUSE_CLICK_LISTENER;
import static org.chromium.chrome.browser.ttc.TtcSessionProperties.PAUSE_ENABLED;
import static org.chromium.chrome.browser.ttc.TtcSessionProperties.STATUS_TEXT_RES_ID;
import static org.chromium.chrome.browser.ttc.TtcSessionProperties.VISIBLE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds {@link TtcSessionProperties} to the large-screen {@link TtcVoicePlateView}. */
@NullMarked
class TtcVoicePlateViewBinder {
    static void bind(PropertyModel model, TtcVoicePlateView view, PropertyKey key) {
        if (key == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        } else if (key == STATUS_TEXT_RES_ID) {
            view.setStatusText(model.get(STATUS_TEXT_RES_ID));
        } else if (key == MIC_MUTED) {
            view.setMicMuted(model.get(MIC_MUTED));
        } else if (key == ON_PAUSE_CLICK_LISTENER) {
            view.setOnPauseClickListener(model.get(ON_PAUSE_CLICK_LISTENER));
        } else if (key == PAUSE_ENABLED) {
            view.setPauseEnabled(model.get(PAUSE_ENABLED));
        }
    }
}
