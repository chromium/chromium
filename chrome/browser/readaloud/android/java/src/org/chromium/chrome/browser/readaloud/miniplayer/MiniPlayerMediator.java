// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.miniplayer;

import android.view.View;

import org.chromium.ui.modelutil.PropertyModel;

/** Mediator class responsible for controlling Read Aloud mini player. */
public class MiniPlayerMediator {
    private final PropertyModel mModel;

    public MiniPlayerMediator(PropertyModel model) {
        mModel = model;
        mModel.set(MiniPlayerProperties.ON_CLOSE_CLICK_KEY, (view) -> { dismiss(); });
    }

    public void show() {
        mModel.set(MiniPlayerProperties.VIEW_VISIBILITY_KEY, View.VISIBLE);
    }

    public void dismiss() {
        mModel.set(MiniPlayerProperties.VIEW_VISIBILITY_KEY, View.GONE);
    }
}
