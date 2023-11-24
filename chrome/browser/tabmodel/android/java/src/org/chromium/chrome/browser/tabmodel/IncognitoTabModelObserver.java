// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

/** An observer of {@link IncognitoTabModel} that receives events relevant to incognito tabs. */
public interface IncognitoTabModelObserver {
    /** A delegate to control whether to show or hide the Incognito re-auth dialog. */
    interface IncognitoReauthDialogDelegate {
        /**
         * An event which is fired the last when the {@link TabModel} changed to regular in order
         * to ensure we hide the re-auth dialog in the end, to avoid leaking any trace from the
         * previous Incognito {@link TabModel}
         */
        void onAfterRegularTabModelChanged();

        /**
         * An event which is fired the earliest when the {@link TabModel} is selected to incognito
         * in order to ensure we show the re-auth dialog fast.
         */
        void onBeforeIncognitoTabModelSelected();
    }

    /** Called when the first tab of the {@link IncognitoTabModel} is created. */
    default void wasFirstTabCreated() {}

    /** Called when the last tab of the {@link IncognitoTabModel} is closed. */
    default void didBecomeEmpty() {}
}
