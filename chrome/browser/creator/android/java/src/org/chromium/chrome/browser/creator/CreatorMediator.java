// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import android.content.Context;

import org.chromium.chrome.browser.creator.CreatorApiBridge.Creator;

/**
 * Sets up the Mediator for Cormorant Creator surface.  It is based on the doc at
 * https://chromium.googlesource.com/chromium/src/+/HEAD/docs/ui/android/mvc_simple_list_tutorial.md
 */
public class CreatorMediator {
    private Context mContext;
    private Creator mCreator;

    CreatorMediator(Context context) {
        mContext = context;
    }

    private void getCreator() {
        CreatorApiBridge.getCreator("test", this::onGetCreator);
    }

    private void onGetCreator(Creator creator) {
        mCreator = creator;
    }
}
