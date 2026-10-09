// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import static org.chromium.chrome.browser.ttc.TtcSessionProperties.AUDIO_LEVEL;
import static org.chromium.chrome.browser.ttc.TtcSessionProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.ttc.TtcSessionProperties.STATUS_TEXT_RES_ID;
import static org.chromium.chrome.browser.ttc.TtcSessionProperties.VISIBLE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds {@link TtcSessionProperties} to the {@link TtcSessionPillView}. */
@NullMarked
class TtcSessionViewBinder {
    static void bind(PropertyModel model, TtcSessionPillView view, PropertyKey key) {
        if (key == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        } else if (key == STATUS_TEXT_RES_ID) {
            view.setStatusText(model.get(STATUS_TEXT_RES_ID));
        } else if (key == AUDIO_LEVEL) {
            view.setAudioLevel(model.get(AUDIO_LEVEL));
        } else if (key == ON_CLICK_LISTENER) {
            view.setOnClickListener(model.get(ON_CLICK_LISTENER));
        }
    }
}
