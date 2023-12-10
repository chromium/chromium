// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;

/** Interface between the suggestion surface and the rest of the browser. */
public interface SuggestionsUiDelegate {
    // Dependency injection
    // TODO(dgn): remove these methods once the users have a different way to get a reference
    // to these objects (https://crbug.com/677672)

    /** Convenience method to access the {@link SuggestionsNavigationDelegate}. */
    SuggestionsNavigationDelegate getNavigationDelegate();

    /** Convenience method to access the {@link NativePageHost}. */
    NativePageHost getNativePageHost();

    /** Convenience method to access the {@link ImageFetcher}. */
    ImageFetcher getImageFetcher();

    /** Convenience method to access the {@link SnackbarManager}. */
    SnackbarManager getSnackbarManager();

    // Feature/State checks

    /** Registers a {@link DestructionObserver}, notified when the delegate's host goes away. */
    void addDestructionObserver(DestructionObserver destructionObserver);

    /** @return Whether the suggestions UI is currently visible. */
    boolean isVisible();
}
