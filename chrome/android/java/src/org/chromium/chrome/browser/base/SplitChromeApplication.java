// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import static org.chromium.chrome.browser.base.SplitCompatUtils.CHROME_SPLIT_NAME;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.PackageManager;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.JNIUtils;

import java.lang.reflect.Field;

/**
 * Application class to use for Chrome when //chrome code is in an isolated split. This class will
 * perform any necessary initialization for non-browser processes without loading code from the
 * chrome split. In the browser process, the necessary logic is loaded from the chrome split using
 * reflection.
 */
public class SplitChromeApplication extends SplitCompatApplication {
    private String mChromeApplicationClassName;
    private SplitPreloader mSplitPreloader;

    public SplitChromeApplication() {
        this(SplitCompatUtils.getIdentifierName(
                "org.chromium.chrome.browser.ChromeApplication$ChromeApplicationImpl"));
    }

    public SplitChromeApplication(String chromeApplicationClassName) {
        mChromeApplicationClassName = chromeApplicationClassName;
    }

    @Override
    public void onCreate() {
        if (mSplitPreloader != null) {
            mSplitPreloader.wait(CHROME_SPLIT_NAME);
        }
        super.onCreate();
    }

    @Override
    protected void attachBaseContext(Context context) {
        super.attachBaseContext(context);
        if (isBrowserProcess()) {
            setImplSupplier(() -> {
                Context chromeContext = SplitCompatUtils.createChromeContext(this);
                return (Impl) SplitCompatUtils.newInstance(
                        chromeContext, mChromeApplicationClassName);
            });
            applyActivityClassLoaderWorkaround();
        } else {
            setImplSupplier(() -> createNonBrowserApplication());
        }
    }

    @Override
    public Context createContextForSplit(String name) throws PackageManager.NameNotFoundException {
        if (mSplitPreloader != null) {
            // Wait for any splits that are preloading so we don't have a race to update the
            // class loader cache (b/172602571).
            mSplitPreloader.wait(name);
        }
        return super.createContextForSplit(name);
    }

    @Override
    protected void performBrowserProcessPreloading(Context context) {
        // The chrome split has a large amount of code, which can slow down startup. Loading
        // this in the background allows us to do this in parallel with startup tasks which do
        // not depend on code in the chrome split.
        mSplitPreloader = new SplitPreloader(context);
        // If the chrome module is not enabled or isolated splits are not supported (e.g. in Android
        // N), the onComplete function will run immediately so it must handle the case where the
        // base context of the application has not been set yet.
        mSplitPreloader.preload(CHROME_SPLIT_NAME, (chromeContext) -> {
            // When installed, the vr module is always loaded on startup, so preload here.
            mSplitPreloader.preload("vr", null);
            // If the chrome module is not enabled or isolated splits are not supported,
            // chromeContext will have the same ClassLoader as the base context, so no need to
            // replace the ClassLoaders here.
            if (!context.getClassLoader().equals(chromeContext.getClassLoader())) {
                replaceClassLoader(this, chromeContext.getClassLoader());
                JNIUtils.setClassLoader(chromeContext.getClassLoader());
            }
        });
    }

    protected Impl createNonBrowserApplication() {
        return new Impl();
    }

    /**
     * Fixes Activity ClassLoader if necessary. Isolated splits can cause a ClassLoader mismatch
     * between the Application and Activity ClassLoaders. We have a workaround in
     * SplitCompatAppComponentFactory which overrides the Activity ClassLoader, but this does not
     * change the ClassLoader for the Activity's base context. We override that ClassLoader here, so
     * it matches the ClassLoader that was used to load the Activity class. Note that
     * ContextUtils.getApplicationContext().getClassLoader() may not give the right ClassLoader here
     * because the Activity may be in a DFM which is a child of the chrome DFM. See
     * crbug.com/1146745 for more info.
     */
    private static void applyActivityClassLoaderWorkaround() {
        ApplicationStatus.registerStateListenerForAllActivities(
                new ApplicationStatus.ActivityStateListener() {
                    @Override
                    public void onActivityStateChange(
                            Activity activity, @ActivityState int newState) {
                        if (newState != ActivityState.CREATED) {
                            return;
                        }

                        // ClassLoaders are already the same, no workaround needed.
                        if (activity.getClassLoader().equals(
                                    activity.getClass().getClassLoader())) {
                            return;
                        }

                        replaceClassLoader(
                                activity.getBaseContext(), activity.getClass().getClassLoader());
                    }
                });
    }

    private static void replaceClassLoader(Context baseContext, ClassLoader classLoader) {
        while (baseContext instanceof ContextWrapper) {
            baseContext = ((ContextWrapper) baseContext).getBaseContext();
        }

        try {
            // baseContext should now be an instance of ContextImpl.
            Field classLoaderField = baseContext.getClass().getDeclaredField("mClassLoader");
            classLoaderField.setAccessible(true);
            classLoaderField.set(baseContext, classLoader);
        } catch (ReflectiveOperationException e) {
            throw new RuntimeException("Error setting ClassLoader.", e);
        }
    }
}
