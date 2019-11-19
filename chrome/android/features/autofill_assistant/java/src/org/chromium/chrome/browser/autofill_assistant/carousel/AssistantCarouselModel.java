// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.carousel;

import org.chromium.ui.modelutil.ListModel;

/**
 * State for the carousel of the Autofill Assistant.
 */
public class AssistantCarouselModel {
    private final ListModel<AssistantChip> mChipsModel = new ListModel<>();

    public ListModel<AssistantChip> getChipsModel() {
        return mChipsModel;
    }
}
