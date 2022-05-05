// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.content.Intent;
import android.view.View;

import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.omnibox.BackKeyBehaviorDelegate;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Queries the user's default search engine and shows autocomplete suggestions. */
public class SearchActivity extends AsyncInitializationActivity
        implements SnackbarManageable, BackKeyBehaviorDelegate, UrlFocusChangeListener {

    @Override
    protected boolean isStartedUpCorrectly(Intent intent) {
        return super.isStartedUpCorrectly(intent);
    }

    @Override
    protected boolean shouldDelayBrowserStartup() {
        return true;
    }

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return null;
    }

    @Override
    protected ModalDialogManager createModalDialogManager() {
        return null;
    }

    @Override
    protected void triggerLayoutInflation() {
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();
    }

    // OverrideBackKeyBehaviorDelegate implementation.
    @Override
    public boolean handleBackKeyPressed() {
        return true;
    }

    @Override
    protected View getViewToBeDrawnBeforeInitializingNative() {
        return null;
    }

    @Override
    public void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
    }

    @Override
    public void onPause() {
        super.onPause();
    }

    @Override
    public void onResume() {
        super.onResume();
    }

    @Override
    public SnackbarManager getSnackbarManager() {
        return null;
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
    }

}
