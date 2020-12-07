// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;

import java.lang.reflect.Field;

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
        if (isBrowserProcess()) {
            applyActivityClassLoaderWorkaround();
        }
    }

    protected MainDexApplicationImpl createNonBrowserApplication() {
        return new MainDexApplicationImpl();
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

                        Context baseContext = activity.getBaseContext();
                        while (baseContext instanceof ContextWrapper) {
                            baseContext = ((ContextWrapper) baseContext).getBaseContext();
                        }

                        try {
                            // baseContext should now be an instance of ContextImpl.
                            Field classLoaderField =
                                    baseContext.getClass().getDeclaredField("mClassLoader");
                            classLoaderField.setAccessible(true);
                            classLoaderField.set(baseContext, activity.getClass().getClassLoader());
                        } catch (ReflectiveOperationException e) {
                            throw new RuntimeException("Error fixing Activity ClassLoader.", e);
                        }
                    }
                });
    }
}
