// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import static org.chromium.chrome.browser.base.SplitCompatApplication.CHROME_SPLIT_NAME;

import android.app.Activity;
import android.app.AppComponentFactory;
import android.content.BroadcastReceiver;
import android.content.ContentProvider;
import android.content.Context;
import android.content.Intent;
import android.os.Build;

import androidx.annotation.RequiresApi;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

/**
 * There are some cases where the ClassLoader for components in the chrome split does not match the
 * ClassLoader from the application Context. This can cause issues, such as ClassCastExceptions when
 * trying to cast between the two ClassLoaders. This class attempts to workaround this bug by
 * explicitly setting the activity's ClassLoader. See b/172602571 for more details.
 *
 * Note: this workaround is not needed for services, since they always uses the base module's
 * ClassLoader, see b/169196314 for more details.
 */
@RequiresApi(Build.VERSION_CODES.P)
public class SplitCompatAppComponentFactory extends AppComponentFactory {
    private static final String TAG = "SplitCompat";

    @Override
    public Activity instantiateActivity(ClassLoader cl, String className, Intent intent)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        // Activities will not call createContextForSplit() which will normally ensure the preload
        // is finished, so we have to manually ensure that here.
        SplitChromeApplication.finishPreload(CHROME_SPLIT_NAME);
        return super.instantiateActivity(getComponentClassLoader(cl, className), className, intent);
    }

    @Override
    public ContentProvider instantiateProvider(ClassLoader cl, String className)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        return super.instantiateProvider(getComponentClassLoader(cl, className), className);
    }

    @Override
    public BroadcastReceiver instantiateReceiver(ClassLoader cl, String className, Intent intent)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        // Receivers call createContextForSplit() on the ContextImpl, which will not run the logic
        // in SplitChromeApplication which makes sure the preload has finished.
        SplitChromeApplication.finishPreload(CHROME_SPLIT_NAME);
        return super.instantiateReceiver(getComponentClassLoader(cl, className), className, intent);
    }

    private static ClassLoader getComponentClassLoader(ClassLoader cl, String className) {
        Context appContext = ContextUtils.getApplicationContext();
        if (appContext == null) {
            Log.e(TAG, "Unexpected null Context when instantiating component: %s", className);
            return cl;
        }

        ClassLoader baseClassLoader = SplitCompatAppComponentFactory.class.getClassLoader();
        ClassLoader chromeClassLoader = appContext.getClassLoader();
        if (!cl.equals(chromeClassLoader) && !BundleUtils.canLoadClass(baseClassLoader, className)
                && BundleUtils.canLoadClass(chromeClassLoader, className)) {
            Log.w(TAG, "Mismatched ClassLoaders between Application and component: %s", className);
            return chromeClassLoader;
        }

        return cl;
    }
}
