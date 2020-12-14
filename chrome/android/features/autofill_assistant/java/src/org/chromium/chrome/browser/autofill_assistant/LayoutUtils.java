// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.view.LayoutInflater;

import org.chromium.chrome.browser.base.SplitCompatUtils;

/** Utilities for dealing with layouts and inflation. */
public class LayoutUtils {
    private static final String ASSISTANT_SPLIT_NAME = "autofill_assistant";

    /**
     * Creates a LayoutInflater which can be used to inflate layouts from the autofill_assistant
     * module.
     */
    public static LayoutInflater createInflater(Context context) {
        return LayoutInflater.from(
                SplitCompatUtils.createContextForInflation(context, ASSISTANT_SPLIT_NAME));
    }
}
