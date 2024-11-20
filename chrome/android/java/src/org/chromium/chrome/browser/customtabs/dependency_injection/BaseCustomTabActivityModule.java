// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dependency_injection;

import dagger.Module;
import dagger.Provides;

import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;

/** Module for bindings shared between custom tabs and webapps. */
@Module
public class BaseCustomTabActivityModule {
    private final BaseCustomTabActivity mActivity;

    public BaseCustomTabActivityModule(BaseCustomTabActivity activity) {
        mActivity = activity;
    }

    @Provides
    public BaseCustomTabActivity providesBaseCustomTabActivity() {
        return mActivity;
    }

    public interface Factory {
        BaseCustomTabActivityModule create(BaseCustomTabActivity activity);
    }
}
