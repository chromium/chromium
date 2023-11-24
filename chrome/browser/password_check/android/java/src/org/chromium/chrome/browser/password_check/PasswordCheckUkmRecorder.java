// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.app.Activity;
import android.content.Intent;

import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/** Helper class for recording password check UKM metrics */
public class PasswordCheckUkmRecorder extends EmptyTabObserver {
    public static final String PASSWORD_CHECK_PACKAGE =
            "org.chromium.chrome.browser.password_check.";
    public static final String PASSWORD_CHANGE_TYPE = "PASSWORD_CHANGE_TYPE";

    /** Creates {@link PasswordCheckUkmRecorder} instance. */
    public static void createForTab(Tab tab) {
        new PasswordCheckUkmRecorder(tab);
    }

    /**
     * Constructs a new PasswordCheckUkmRecorder for a specific tab.
     *
     * @param tab Tab this PasswordCheckUkmRecorder is created for.
     */
    private PasswordCheckUkmRecorder(Tab tab) {
        tab.addObserver(this);
    }

    private Intent getIntent(WebContents webContents) {
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return null;
        Activity activity = window.getActivity().get();

        return (activity != null) ? activity.getIntent() : null;
    }

    private void recordPasswordChange(
            WebContents webContents, @PasswordChangeType int passwordChangeType) {
        new UkmRecorder.Bridge()
                .recordEventWithIntegerMetric(
                        webContents,
                        "PasswordManager.PasswordChangeTriggered",
                        "PasswordChangeType",
                        passwordChangeType);
    }

    @Override
    public void onPageLoadFinished(Tab tab, GURL url) {
        // Record UKMs
        if (tab.getWebContents() == null) {
            return;
        }
        Intent intent = getIntent(tab.getWebContents());
        if (intent == null || intent.getExtras() == null) {
            return;
        }

        if (intent.hasExtra(PASSWORD_CHECK_PACKAGE + PASSWORD_CHANGE_TYPE)) {
            recordPasswordChange(
                    tab.getWebContents(),
                    intent.getExtras().getInt(PASSWORD_CHECK_PACKAGE + PASSWORD_CHANGE_TYPE));
            intent.removeExtra(PASSWORD_CHECK_PACKAGE + PASSWORD_CHANGE_TYPE);
        }
    }

    @Override
    public void onDestroyed(Tab tab) {
        tab.removeObserver(this);
    }
}
