// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.content.Context;

/**
 * Application class to use for Chrome when //chrome code is in an isolated split. This class will
 * perform any necessary initialization for non-browser processes without loading code from the
 * chrome split. In the browser process, the necessary logic is loaded from the chrome split using
 * reflection.
 */
public class SplitChromeApplication extends SplitCompatApplication {
    private String mChromeApplicationClassName;

    public SplitChromeApplication() {
        this(SplitCompatUtils.getIdentifierName(
                "org.chromium.chrome.browser.ChromeApplication$ChromeApplicationImpl"));
    }

    public SplitChromeApplication(String chromeApplicationClassName) {
        mChromeApplicationClassName = chromeApplicationClassName;
    }

    @Override
    protected void attachBaseContext(Context context) {
        if (isBrowserProcess()) {
            context = SplitCompatUtils.createChromeContext(context);
            setImpl((Impl) SplitCompatUtils.newInstance(context, mChromeApplicationClassName));
        } else {
            setImpl(createNonBrowserApplication());
        }
        super.attachBaseContext(context);
    }

    protected MainDexApplicationImpl createNonBrowserApplication() {
        return new MainDexApplicationImpl();
    }
}
