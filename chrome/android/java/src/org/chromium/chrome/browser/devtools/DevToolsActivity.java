// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.devtools;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.SparseArray;

import androidx.browser.customtabs.CustomTabsIntent;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.AsyncTabParams;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.components.page_info.CertificateViewer;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Activity to show DevTools frontend. */
@NullMarked
public class DevToolsActivity extends BaseCustomTabActivity {
    private static final String WEB_CONTENTS_KEY =
            "org.chromium.chrome.browser.devtools.DevTools.WebContents";

    /**
     * Whether the DevTools frontend web contents belongs to the primary off-the-record profile,
     * i.e. DevTools was opened for an Incognito tab.
     */
    private static final String IS_INCOGNITO_KEY =
            "org.chromium.chrome.browser.devtools.DevTools.IsIncognito";

    private @Nullable CertificateViewer mCertificateViewer;

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
        Profile profile = Profile.fromWebContents(webContents);
        intent.putExtra(IS_INCOGNITO_KEY, profile != null && profile.isPrimaryOtrProfile());

        context.startActivity(intent);
    }

    @CalledByNative
    static void showCertificateViewer(WebContents webContents, byte[][] certChain) {
        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return;
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null || activity.isFinishing() || activity.isDestroyed()) return;
        if (activity instanceof DevToolsActivity devToolsActivity) {
            devToolsActivity.showCertificateChain(certChain);
        }
    }

    private void showCertificateChain(byte[][] certChain) {
        if (mCertificateViewer == null) {
            mCertificateViewer = new CertificateViewer(this);
        }
        mCertificateViewer.showCertificateChain(certChain);
    }

    private boolean isIncognito() {
        return IntentUtils.safeGetBooleanExtra(getIntent(), IS_INCOGNITO_KEY, false);
    }

    private static @Nullable WebContents getWebContentsFromIntent(Intent intent) {
        intent.setExtrasClassLoader(WebContents.class.getClassLoader());
        return IntentUtils.safeGetParcelableExtra(intent, WEB_CONTENTS_KEY);
    }

    private static boolean isTabAwaitingReparentingFor(WebContents webContents) {
        SparseArray<AsyncTabParams> params =
                AsyncTabParamsManagerSingleton.getInstance().getAsyncTabParams();
        for (int i = 0; i < params.size(); i++) {
            Tab tab = params.valueAt(i).getTabToReparent();
            if (tab != null && tab.getWebContents() == webContents) {
                return true;
            }
        }
        return false;
    }

    @Override
    protected @LaunchIntentDispatcher.Action int maybeDispatchLaunchIntent(
            Intent intent, @Nullable Bundle savedInstanceState) {
        WebContents webContents = getWebContentsFromIntent(intent);
        if (webContents == null || webContents.isDestroyed()) {
            return LaunchIntentDispatcher.Action.FINISH_ACTIVITY_REMOVE_TASK;
        }
        if (savedInstanceState != null && !isTabAwaitingReparentingFor(webContents)) {
            return LaunchIntentDispatcher.Action.FINISH_ACTIVITY_REMOVE_TASK;
        }
        return super.maybeDispatchLaunchIntent(intent, savedInstanceState);
    }

    @Override
    protected @Nullable BrowserServicesIntentDataProvider buildIntentDataProvider(
            Intent intent, @CustomTabsIntent.ColorScheme int colorScheme) {
        return new DevToolsIntentDataProvider(
                intent, IntentUtils.safeGetBooleanExtra(intent, IS_INCOGNITO_KEY, false));
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

        TabModel tabModel = getTabModelSelector().getModel(isIncognito());
        Tab existingTab = tabModel.getCurrentTabSupplier().get();
        if (existingTab == null) {
            finish();
            return;
        }

        // Attach the given DevTools frontend web contents to the browser window.
        WebContents webContents = getWebContentsFromIntent(getIntent());
        // When the activity is recreated in-process (e.g. on a theme or display configuration
        // change), the reparented tab already hosts the DevTools frontend WebContents.
        if (webContents != null && existingTab.getWebContents() == webContents) {
            return;
        }

        ChromeAndroidTask task = getChromeAndroidTaskSupplier().get();
        Profile profile = tabModel.getProfile();
        // NOTE: If this condition is not met, this activity briefly shows up, but quickly
        // disappears because there is no tab in the tab model after the closeTabs() call below.
        if (webContents != null && task != null && profile != null) {
            DevToolsWindowAndroid.attachToBrowser(
                    webContents, task.getOrCreateNativeBrowserWindow(profile));
        }

        // Remove the existing blank page tab.
        tabModel.getTabRemover().closeTabs(TabClosureParams.closeTab(existingTab).build(), false);
    }

    @Override
    protected void onDestroyInternal() {
        if (mCertificateViewer != null) {
            mCertificateViewer.dismiss();
            mCertificateViewer = null;
        }
        super.onDestroyInternal();
    }

    @Nullable CertificateViewer getCertificateViewerForTesting() {
        return mCertificateViewer;
    }
}
