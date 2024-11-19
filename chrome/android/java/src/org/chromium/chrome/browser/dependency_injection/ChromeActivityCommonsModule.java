// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dependency_injection;

import android.app.Activity;
import android.content.res.Resources;

import dagger.Module;
import dagger.Provides;

import org.chromium.chrome.browser.app.ChromeActivity;

/** Module for common dependencies in {@link ChromeActivity}. */
@Module
public class ChromeActivityCommonsModule {
    private final ChromeActivity mActivity;

    public ChromeActivityCommonsModule(ChromeActivity activity) {
        mActivity = activity;
    }

    @Provides
    public Activity provideActivity() {
        return mActivity;
    }

    @Provides
    public Resources provideResources() {
        return mActivity.getResources();
    }
}
