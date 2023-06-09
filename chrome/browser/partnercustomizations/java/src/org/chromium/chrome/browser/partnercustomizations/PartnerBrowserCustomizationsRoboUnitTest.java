// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.Handler;
import android.os.Looper;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizationsRoboUnitTest.ShadowCustomizationProviderDelegate;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.ShadowGURL;

/**
 * Unit tests for {@link PartnerBrowserCustomizations}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowPostTask.class, ShadowGURL.class,
                ShadowCustomizationProviderDelegate.class})
public class PartnerBrowserCustomizationsRoboUnitTest {
    @Before
    public void setup() {
        ShadowCustomizationProviderDelegate.sHomepage = JUnitTestGURLs.EXAMPLE_URL;
        ShadowPostTask.setTestImpl(new ShadowPostTask.TestImpl() {
            final Handler mHandler = new Handler(Looper.getMainLooper());
            @Override
            public void postDelayedTask(@TaskTraits int taskTraits, Runnable task, long delay) {
                mHandler.post(task);
            }
        });
    }

    @After
    public void tearDown() {
        PartnerBrowserCustomizations.destroy();
    }

    @Test
    public void initializeAsyncOneAtATime() {
        PartnerBrowserCustomizations.getInstance().initializeAsync(
                ContextUtils.getApplicationContext());
        // Run one task, so AsyncTask#doInBackground is run, but not #onFinalized.
        ShadowLooper.runMainLooperOneTask();
        assertFalse("The homepage refreshed, but result is not yet posted on UI thread.",
                PartnerBrowserCustomizations.getInstance().isInitialized());

        // Assuming homepage is changed during 1st #initializeAsync, and another #initializeAsync is
        // triggered. The 2nd #initializeAsync is ignored since there's one already in the process.
        ShadowCustomizationProviderDelegate.sHomepage = JUnitTestGURLs.GOOGLE_URL;
        PartnerBrowserCustomizations.getInstance().initializeAsync(
                ContextUtils.getApplicationContext());
        assertFalse("#initializeAsync should be in progress.",
                PartnerBrowserCustomizations.getInstance().isInitialized());

        ShadowLooper.idleMainLooper();
        assertTrue("#initializeAsync should be done.",
                PartnerBrowserCustomizations.getInstance().isInitialized());
        assertEquals("Homepage should be set via 1st initializeAsync.", JUnitTestGURLs.EXAMPLE_URL,
                PartnerBrowserCustomizations.getInstance().getHomePageUrl().getSpec());

        PartnerBrowserCustomizations.getInstance().initializeAsync(
                ContextUtils.getApplicationContext());
        assertFalse("3rd #initializeAsync should be in progress.",
                PartnerBrowserCustomizations.getInstance().isInitialized());

        ShadowLooper.idleMainLooper();
        assertTrue("3rd #initializeAsync should be done.",
                PartnerBrowserCustomizations.getInstance().isInitialized());
        assertEquals("Homepage should refreshed by 3rd #initializeAsync.",
                JUnitTestGURLs.GOOGLE_URL,
                PartnerBrowserCustomizations.getInstance().getHomePageUrl().getSpec());
    }

    /**
     * Convenient shadow class to set homepage provided by partner during robo tests.
     */
    @Implements(CustomizationProviderDelegateUpstreamImpl.class)
    public static class ShadowCustomizationProviderDelegate {
        static String sHomepage;

        public ShadowCustomizationProviderDelegate() {}

        @Implementation
        @Nullable
        /** Returns the homepage string or null if none is available. */
        protected String getHomepage() {
            return sHomepage;
        }
        @Implementation
        /** Returns whether incognito mode is disabled. */
        protected boolean isIncognitoModeDisabled() {
            return false;
        }
        @Implementation
        /** Returns whether bookmark editing is disabled. */
        protected boolean isBookmarksEditingDisabled() {
            return false;
        }
    }
}
