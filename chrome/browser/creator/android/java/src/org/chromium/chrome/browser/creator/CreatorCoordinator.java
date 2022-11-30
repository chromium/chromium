// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;

/**
 * Sets up the Coordinator for Cormorant Creator surface.  It is based on the doc at
 * https://chromium.googlesource.com/chromium/src/+/HEAD/docs/ui/android/mvc_simple_list_tutorial.md
 */
public class CreatorCoordinator {
    private CreatorMediator mMediator;
    private Activity mActivity;
    private final View mView;

    public CreatorCoordinator(Activity activity) {
        mActivity = activity;

        // Inflate the XML.
        mView = LayoutInflater.from(mActivity).inflate(R.layout.creator_activity, null);

        mMediator = new CreatorMediator(mActivity);
    }

    public View getView() {
        return mView;
    }
}
