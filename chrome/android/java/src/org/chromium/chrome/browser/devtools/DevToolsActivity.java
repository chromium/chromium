// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.devtools;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.Intent;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.content_public.browser.WebContents;

/** Activity to show DevTools frontend. */
@NullMarked
public class DevToolsActivity extends BaseCustomTabActivity {
    private static final String WEB_CONTENTS_KEY =
            "org.chromium.chrome.browser.devtools.DevTools.WebContents";

    @CalledByNative
    /** Launches the DevTools activity for the given DevTools frontend. */
    static void launchDevToolsActivity(WebContents webContents) {
        Context context = ContextUtils.getApplicationContext();

        Intent intent = new Intent(context, DevToolsActivity.class);
        intent.addFlags(
                Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT
                        | Intent.FLAG_ACTIVITY_NEW_TASK
                        | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        intent.putExtra(DevToolsActivity.WEB_CONTENTS_KEY, webContents);

        context.startActivity(intent);
    }

    @Override
    protected LaunchCauseMetrics createLaunchCauseMetrics() {
        return new LaunchCauseMetrics(this) {
            @Override
            public @LaunchCause int computeIntentLaunchCause() {
                return LaunchCause.DEV_TOOLS;
            }
        };
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();

        TabModel tabModel = getTabModelSelector().getModel(false);
        Tab existingTab = assumeNonNull(tabModel.getCurrentTabSupplier().get());

        // Attach the given DevTools frontend web contents to the browser window.
        Intent intent = getIntent();
        intent.setExtrasClassLoader(WebContents.class.getClassLoader());
        WebContents webContents = intent.getParcelableExtra(WEB_CONTENTS_KEY);
        ChromeAndroidTask task = getChromeAndroidTaskSupplier().get();
        Profile profile = tabModel.getProfile();
        // NOTE: If this condition is not met, this activity brifly shows up, but quickly disappears
        // because there is no tab in the tab model after the closerTabs() call below.
        if (webContents != null && task != null && profile != null) {
            DevToolsWindowAndroid.attachToBrowser(
                    webContents, task.getOrCreateNativeBrowserWindowPtr(profile));
        }

        // Remove the existing blank page tab.
        tabModel.getTabRemover().closeTabs(TabClosureParams.closeTab(existingTab).build(), false);
    }
}
