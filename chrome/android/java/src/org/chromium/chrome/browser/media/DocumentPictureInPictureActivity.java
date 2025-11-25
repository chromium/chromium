// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.annotation.SuppressLint;
import android.content.Intent;

import org.jni_zero.NativeMethods;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.content_public.browser.WebContents;

@NullMarked
public class DocumentPictureInPictureActivity extends AsyncInitializationActivity {
    public static final String WEB_CONTENTS_KEY =
            "org.chromium.chrome.browser.media.DocumentPictureInPicture.WebContents";
    private WebContents mWebContents;
    private Tab mInitiatorTab;

    @Override
    protected void onPreCreate() {
        super.onPreCreate();

        Intent intent = getIntent();
        intent.setExtrasClassLoader(WebContents.class.getClassLoader());
        WebContents webContents = intent.getParcelableExtra(WEB_CONTENTS_KEY);
        if (webContents == null) {
            finish();
            return;
        }

        mWebContents = webContents;
        WebContents parentWebContents = mWebContents.getDocumentPictureInPictureOpener();
        mInitiatorTab = TabUtils.fromWebContents(parentWebContents);
        if (parentWebContents == null || TabUtils.getActivity(mInitiatorTab) == null) {
            finish();
            return;
        }
    }

    /**
     * @return Whether the document pip WebContents and the initiator tab are both initialized.
     */
    @EnsuresNonNullIf({"mWebContents", "mInitiatorTab"})
    private boolean isContentsInitialized() {
        return mWebContents != null && mInitiatorTab != null;
    }

    @Override
    @SuppressLint("NewAPI")
    public void onStart() {
        super.onStart();
        assert isContentsInitialized();

        if (mInitiatorTab.getWebContents() == null) {
            finish();
            return;
        }

        DocumentPictureInPictureActivityJni.get()
                .onActivityStart(mInitiatorTab.getWebContents(), mWebContents);
    }

    @Override
    protected void triggerLayoutInflation() {
        assert isContentsInitialized();
        onInitialLayoutInflationComplete();
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    protected OneshotSupplier<ProfileProvider> createProfileProvider() {
        // TODO(https://crbug.com/422715286): implement this method.
        return new OneshotSupplierImpl<>();
    }

    @Override
    @SuppressWarnings("NullAway")
    protected final void onDestroy() {
        if (mWebContents != null) {
            mWebContents.destroy();
            mWebContents = null;
        }

        super.onDestroy();
    }

    @NativeMethods
    public interface Natives {
        void onActivityStart(WebContents parentWebContent, WebContents webContents);
    }
}
