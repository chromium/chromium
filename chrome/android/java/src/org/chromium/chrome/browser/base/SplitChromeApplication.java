// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import static org.chromium.chrome.browser.base.SplitCompatUtils.CHROME_SPLIT_NAME;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.BundleUtils;
import org.chromium.base.JNIUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;

/**
 * Application class to use for Chrome when //chrome code is in an isolated split. This class will
 * perform any necessary initialization for non-browser processes without loading code from the
 * chrome split. In the browser process, the necessary logic is loaded from the chrome split using
 * reflection.
 *
 * This class will be used when isolated splits are enabled.
 */
public class SplitChromeApplication extends SplitCompatApplication {
    private static final String TAG = "SplitChromeApp";

    @SuppressLint("StaticFieldLeak")
    private static SplitPreloader sSplitPreloader;

    private String mChromeApplicationClassName;

    public SplitChromeApplication() {
        this(SplitCompatUtils.getIdentifierName(
                "org.chromium.chrome.browser.ChromeApplicationImpl"));
    }

    public SplitChromeApplication(String chromeApplicationClassName) {
        mChromeApplicationClassName = chromeApplicationClassName;
    }

    @Override
    public void onCreate() {
        finishPreload(CHROME_SPLIT_NAME);
        super.onCreate();
    }

    @Override
    protected void attachBaseContext(Context context) {
        super.attachBaseContext(context);
        if (isBrowserProcess()) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                DexFixer.setHasIsolatedSplits(true);
            }
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
        try (TraceEvent te = TraceEvent.scoped("SplitChromeApplication.createContextForSplit")) {
            // Wait for any splits that are preloading so we don't have a race to update the
            // class loader cache (b/172602571).
            finishPreload(name);
            long startTime = SystemClock.uptimeMillis();
            Context context;
            synchronized (BundleUtils.getSplitContextLock()) {
                context = super.createContextForSplit(name);
            }
            RecordHistogram.recordTimesHistogram("Android.IsolatedSplits.ContextCreateTime." + name,
                    SystemClock.uptimeMillis() - startTime);
            return context;
        }
    }

    @Override
    protected void performBrowserProcessPreloading(Context context) {
        // The chrome split has a large amount of code, which can slow down startup. Loading
        // this in the background allows us to do this in parallel with startup tasks which do
        // not depend on code in the chrome split.
        sSplitPreloader = new SplitPreloader(context);
        // If the chrome module is not enabled or isolated splits are not supported (e.g. in Android
        // N), the onComplete function will run immediately so it must handle the case where the
        // base context of the application has not been set yet.
        sSplitPreloader.preload(CHROME_SPLIT_NAME, new SplitPreloader.OnComplete() {
            @Override
            public void runImmediatelyInBackgroundThread(Context chromeContext) {
                // A new thread is started here because we do not want to delay returning the chrome
                // Context, since that slows down startup. This thread must be a HandlerThread
                // because AsyncInitializationActivity (a base class of ChromeTabbedActivity)
                // creates a Handler, so needs to have a Looper prepared.
                HandlerThread thread = new HandlerThread("ActivityPreload");
                thread.start();
                new Handler(thread.getLooper()).post(() -> {
                    try {
                        // Create a throwaway instance of ChromeTabbedActivity. This will warm up
                        // the chrome ClassLoader, and perform loading of classes used early in
                        // startup in the background.
                        chromeContext.getClassLoader()
                                .loadClass(
                                        "org.chromium.chrome.browser.ChromeTabbedActivity$Preload")
                                .newInstance();
                    } catch (ReflectiveOperationException e) {
                        throw new RuntimeException(e);
                    }
                    thread.quit();
                });
            }

            @Override
            public void runInUiThread(Context chromeContext) {
                // If the chrome module is not enabled or isolated splits are not supported,
                // chromeContext will have the same ClassLoader as the base context, so no need to
                // replace the ClassLoaders here.
                if (!context.getClassLoader().equals(chromeContext.getClassLoader())) {
                    // Replace the application Context's ClassLoader with the chrome ClassLoader,
                    // because the application ClassLoader is expected to be able to access all
                    // chrome classes.
                    BundleUtils.replaceClassLoader(
                            SplitChromeApplication.this, chromeContext.getClassLoader());
                    JNIUtils.setClassLoader(chromeContext.getClassLoader());
                }
            }
        });
    }

    protected Impl createNonBrowserApplication() {
        return new Impl();
    }

    /* package */ static void finishPreload(String name) {
        if (sSplitPreloader != null) {
            sSplitPreloader.wait(name);
        }
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
                        // Some tests pass an activity without a base context.
                        if (activity.getBaseContext() == null) {
                            return;
                        }

                        if (newState != ActivityState.CREATED) {
                            return;
                        }

                        // ClassLoaders are already the same, no workaround needed.
                        if (activity.getClassLoader().equals(
                                    activity.getClass().getClassLoader())) {
                            return;
                        }

                        BundleUtils.replaceClassLoader(
                                activity.getBaseContext(), activity.getClass().getClassLoader());
                    }
                });
    }
}
