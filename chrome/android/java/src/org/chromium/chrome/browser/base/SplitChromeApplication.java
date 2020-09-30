// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;

import org.chromium.base.compat.ApiHelperForO;

/**
 * Application class to use for Chrome when //chrome code is in an isolated split. This class will
 * perform any necessary initialization for non-browser processes without loading code from the
 * chrome split. In the browser process, the necessary logic is loaded from the chrome split using
 * reflection.
 */
public class SplitChromeApplication extends SplitCompatApplication {
    private String mChromeApplicationClassName;

    public SplitChromeApplication() {
        this("org.chromium.chrome.browser.ChromeApplication$ChromeApplicationImpl");
    }

    public SplitChromeApplication(String chromeApplicationClassName) {
        mChromeApplicationClassName = chromeApplicationClassName;
    }

    @Override
    protected void attachBaseContext(Context context) {
        if (isBrowserProcess()) {
            context = createChromeContext(context);
            setImpl(createChromeApplication(context));
        } else {
            setImpl(createNonBrowserApplication());
        }
        super.attachBaseContext(context);
    }

    protected Impl createNonBrowserApplication() {
        return new Impl();
    }

    private Impl createChromeApplication(Context context) {
        try {
            return (Impl) context.getClassLoader()
                    .loadClass(mChromeApplicationClassName)
                    .newInstance();
        } catch (ReflectiveOperationException e) {
            throw new RuntimeException(e);
        }
    }

    private Context createChromeContext(Context base) {
        assert isBrowserProcess();
        // Isolated splits are only supported in O+, so just return the base context on other
        // versions, since this will have access to all splits.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return base;
        }
        try {
            return ApiHelperForO.createContextForSplit(base, "chrome");
        } catch (PackageManager.NameNotFoundException e) {
            // This application class should not be used if the chrome split does not exist.
            throw new RuntimeException(e);
        }
    }
}
