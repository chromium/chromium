// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;

import org.chromium.base.BundleUtils;
import org.chromium.base.JNIUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.IdentifierNameString;

/**
 * Application class to use for Chrome when //chrome code is in an isolated split. This class will
 * perform any necessary initialization for non-browser processes without loading code from the
 * chrome split. In the browser process, the necessary logic is loaded from the chrome split using
 * reflection.
 *
 * This class will be used when isolated splits are enabled.
 */
public class SplitChromeApplication extends SplitCompatApplication {
    private static @IdentifierNameString String sImplClassName =
            "org.chromium.chrome.browser.ChromeApplicationImpl";

    @SuppressLint("StaticFieldLeak")
    private static SplitPreloader sSplitPreloader;

    private String mChromeApplicationClassName;

    private Resources mResources;

    public SplitChromeApplication() {
        this(sImplClassName);
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
            setImplSupplier(
                    () -> {
                        Context chromeContext = createChromeContext(this);
                        return (Impl)
                                BundleUtils.newInstance(chromeContext, mChromeApplicationClassName);
                    });
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
            RecordHistogram.recordTimesHistogram(
                    "Android.IsolatedSplits.ContextCreateTime." + name,
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
        sSplitPreloader.preload(
                CHROME_SPLIT_NAME,
                new SplitPreloader.OnComplete() {
                    @Override
                    public void runImmediatelyInBackgroundThread(Context chromeContext) {
                        // A new thread is started here because we do not want to delay returning
                        // the chrome Context, since that slows down startup. This thread must be
                        // a HandlerThread because AsyncInitializationActivity (a base class of
                        // ChromeTabbedActivity) creates a Handler, so needs to have a Looper
                        // prepared.
                        HandlerThread thread = new HandlerThread("ActivityPreload");
                        thread.start();
                        new Handler(thread.getLooper())
                                .post(
                                        () -> {
                                            try {
                                                // Create a throwaway instance of
                                                // ChromeTabbedActivity. This will warm up
                                                // the chrome ClassLoader, and perform loading of
                                                // classes used early in startup in the
                                                // background.
                                                chromeContext
                                                        .getClassLoader()
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
                        // chromeContext will have the same ClassLoader as the base context, so no
                        // need to replace the ClassLoaders here.
                        if (!context.getClassLoader().equals(chromeContext.getClassLoader())) {
                            // Replace the application Context's ClassLoader with the chrome
                            // ClassLoader, because the application ClassLoader is expected to be
                            // able to access all chrome classes.
                            BundleUtils.replaceClassLoader(
                                    SplitChromeApplication.this, chromeContext.getClassLoader());
                            JNIUtils.setClassLoader(chromeContext.getClassLoader());
                            // Resources holds a reference to a ClassLoader. Make our Application's
                            // getResources() return a reference to the Chrome split's resources
                            // since there are a spots where ContextUtils.getApplicationContext()
                            // is used to retrieve resources (https://crbug.com/1287000).
                            mResources = chromeContext.getResources();
                        }
                    }
                });
    }

    @Override
    public Resources getResources() {
        // If the cached resources from the Chrome split are available use those. Note that
        // retrieving resources will use resources from the base split until the Chrome split is
        // fully loaded. We don't want to ensure the Chrome split is loaded here because resources
        // may be accessed early in startup, and forcing a load here will reduce the benefits of
        // preloading the Chrome split in the background.
        if (mResources != null) {
            return mResources;
        }
        return getBaseContext().getResources();
    }

    /** Waits for the specified split to finish preloading if necessary. */
    public static void finishPreload(String name) {
        if (sSplitPreloader != null) {
            sSplitPreloader.wait(name);
        }
    }

    protected Impl createNonBrowserApplication() {
        return new Impl();
    }
}
