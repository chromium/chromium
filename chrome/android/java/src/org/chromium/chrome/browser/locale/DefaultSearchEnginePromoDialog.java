// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.locale;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.search_engines.DefaultSearchEngineDialogHelper;

/**
 * Extends org.chromium.chrome.browser.search_engines.DefaultSearchEnginePromoDialog
 * for downstream build. Should be deleted once the downstream is updated to use
 * org.chromium.chrome.browser.search_engines.DefaultSearchEnginePromoDialog.
 */
public class DefaultSearchEnginePromoDialog
        extends org.chromium.chrome.browser.search_engines.DefaultSearchEnginePromoDialog {
    public DefaultSearchEnginePromoDialog(Activity activity,
            DefaultSearchEngineDialogHelper.Delegate delegate, int dialogType,
            @Nullable Callback<Boolean> onSuccessCallback) {
        super(activity, delegate, dialogType, onSuccessCallback);
    }

    /** Notified about events happening to the dialog. */
    public interface DefaultSearchEnginePromoDialogObserver {
        void onDialogShown(DefaultSearchEnginePromoDialog shownDialog);
    }

    /**
     * Forwards a static instance of the observer to
     * org.chromium.chrome.browser.search_engines.DefaultSearchEnginePromoDialog.
     */
    @VisibleForTesting
    public static void setObserverForTests(DefaultSearchEnginePromoDialogObserver observer) {
        // For tests, |dialog| doesn't need to be passed since it only cares about
        // the invocation of the observer itself.
        org.chromium.chrome.browser.search_engines.DefaultSearchEnginePromoDialog
                .DefaultSearchEnginePromoDialogObserver observer2 =
                dialog -> observer.onDialogShown(null);
        org.chromium.chrome.browser.search_engines.DefaultSearchEnginePromoDialog
                .setObserverForTests2(observer2);
    }
}
