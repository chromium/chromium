// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import static org.chromium.chrome.browser.base.SplitCompatApplication.CHROME_SPLIT_NAME;

import android.app.Activity;
import android.app.AppComponentFactory;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.ContentProvider;
import android.content.Context;
import android.content.Intent;
import android.os.Build;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * There are some cases where the ClassLoader for components in the chrome split does not match the
 * ClassLoader from the application Context. This can cause issues, such as ClassCastExceptions when
 * trying to cast between the two ClassLoaders. This class attempts to workaround this bug by
 * explicitly setting the activity's ClassLoader. See b/172602571 for more details.
 *
 * <p>Note: this workaround is not needed for services, since they always uses the base module's
 * ClassLoader, see b/169196314 for more details.
 */
@RequiresApi(Build.VERSION_CODES.P)
public class SplitCompatAppComponentFactory extends AppComponentFactory {
    private static final String TAG = "SplitCompat";

    @IntDef({
        ProcessCreationReason.UNINITIALIZED,
        ProcessCreationReason.PENDING,
        ProcessCreationReason.ACTIVITY,
        ProcessCreationReason.SERVICE,
        ProcessCreationReason.CONTENT_PROVIDER,
        ProcessCreationReason.BROADCAST_RECEIVER,
        ProcessCreationReason.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ProcessCreationReason {
        int UNINITIALIZED = -2;
        int PENDING = -1;
        int ACTIVITY = 0;
        int SERVICE = 1;
        int CONTENT_PROVIDER = 2;
        int BROADCAST_RECEIVER = 3;
        int NUM_ENTRIES = 4;
    }

    private static @ProcessCreationReason int sProcessCreationReason =
            ProcessCreationReason.UNINITIALIZED;

    @NonNull
    @Override
    public Activity instantiateActivity(
            @NonNull ClassLoader cl, @NonNull String className, Intent intent)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        setProcessCreationReason(ProcessCreationReason.ACTIVITY);

        // Activities will not call createContextForSplit() which will normally ensure the preload
        // is finished, so we have to manually ensure that here.
        SplitChromeApplication.finishPreload(CHROME_SPLIT_NAME);
        return super.instantiateActivity(getComponentClassLoader(cl, className), className, intent);
    }

    @NonNull
    @Override
    public ContentProvider instantiateProvider(@NonNull ClassLoader cl, @NonNull String className)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        // Android always initializes all ContentProviders when it initializes the Application,
        // however the process could be started for other reasons. Set to pending for now.
        if (sProcessCreationReason == ProcessCreationReason.UNINITIALIZED) {
            sProcessCreationReason = ProcessCreationReason.PENDING;
        }
        return super.instantiateProvider(getComponentClassLoader(cl, className), className);
    }

    @NonNull
    @Override
    public BroadcastReceiver instantiateReceiver(
            @NonNull ClassLoader cl, @NonNull String className, Intent intent)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        setProcessCreationReason(ProcessCreationReason.BROADCAST_RECEIVER);

        // Receivers call createContextForSplit() on the ContextImpl, which will not run the logic
        // in SplitChromeApplication which makes sure the preload has finished.
        SplitChromeApplication.finishPreload(CHROME_SPLIT_NAME);
        return super.instantiateReceiver(getComponentClassLoader(cl, className), className, intent);
    }

    @NonNull
    @Override
    public Service instantiateService(
            @NonNull ClassLoader cl, @NonNull String className, Intent intent)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        setProcessCreationReason(ProcessCreationReason.SERVICE);
        return super.instantiateService(cl, className, intent);
    }

    private static void setProcessCreationReason(@ProcessCreationReason int reason) {
        if (sProcessCreationReason > ProcessCreationReason.PENDING) return;
        sProcessCreationReason = reason;
        if (!SplitCompatApplication.isBrowserProcess()) return;
        RecordHistogram.recordEnumeratedHistogram(
                "Startup.Android.BrowserProcessCreationReason",
                reason,
                ProcessCreationReason.NUM_ENTRIES);
    }

    public static @ProcessCreationReason int getProcessCreationReason() {
        // Determining the ProcessCreationReason is delayed until the first use. Previously it was
        // set in a task immediately posted from instantiateProvider() because content providers are
        // always instantiated early. However, since Android 14+ on Pixel 6+ such task happens to
        // run before other components instantiate. This does not allow the task to discover other
        // creation reasons.
        if (sProcessCreationReason <= ProcessCreationReason.PENDING) {
            setProcessCreationReason(ProcessCreationReason.CONTENT_PROVIDER);
        }
        return sProcessCreationReason;
    }

    private static ClassLoader getComponentClassLoader(ClassLoader cl, String className) {
        Context appContext = ContextUtils.getApplicationContext();
        if (appContext == null) {
            Log.e(TAG, "Unexpected null Context when instantiating component: %s", className);
            return cl;
        }

        ClassLoader baseClassLoader = SplitCompatAppComponentFactory.class.getClassLoader();
        ClassLoader chromeClassLoader = appContext.getClassLoader();
        if (!cl.equals(chromeClassLoader)
                && !BundleUtils.canLoadClass(baseClassLoader, className)
                && BundleUtils.canLoadClass(chromeClassLoader, className)) {
            Log.w(TAG, "Mismatched ClassLoaders between Application and component: %s", className);
            return chromeClassLoader;
        }

        return cl;
    }
}
