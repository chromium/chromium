// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.WebContents;

/**
 * {@link TabObserver} monitoring {@link WebContents} updates to set
 * {@link PartialCustomTabInputMethodManagerWrapper}. This lets the tab detect the event of
 * soft keyboard showing up.
 */
public class PartialCustomTabTabObserver extends EmptyTabObserver {
    private final Callback<Runnable> mShowSoftInputCallback;
    private PartialCustomTabInputMethodWrapper mImmWrapper;
    private Tab mCurrentTab;

    /**
     * @param showSoftInputCallback Callback to invoke when {@link #onShowSoftInput}
     *        is triggered.
     */
    public PartialCustomTabTabObserver(Callback<Runnable> showSoftInputCallback) {
        mShowSoftInputCallback = showSoftInputCallback;
    }

    @Override
    public void onUrlUpdated(Tab tab) {
        if (mImmWrapper == null) {
            mImmWrapper =
                    new PartialCustomTabInputMethodWrapper(
                            tab.getContext(), tab.getWindowAndroid(), mShowSoftInputCallback);
        }
        if (mCurrentTab != tab) {
            updateImmWrapper(tab);
            mCurrentTab = tab;
        }
    }

    @Override
    public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
        updateImmWrapper(tab);
    }

    private void updateImmWrapper(Tab tab) {
        WebContents webContents = tab.getWebContents();
        ImeAdapter imeAdapter = ImeAdapter.fromWebContents(webContents);
        imeAdapter.setInputMethodManagerWrapper(mImmWrapper);
    }
}
