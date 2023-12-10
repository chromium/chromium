// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

/**
 * Object that is registered through the {@link SuggestionsUiDelegate}, and that will be notified
 * when its owner is destroyed.
 * @see SuggestionsUiDelegate#addDestructionObserver(DestructionObserver)
 */
public interface DestructionObserver {
    void onDestroy();
}
