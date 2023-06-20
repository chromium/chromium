// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import org.chromium.chrome.browser.readaloud.external.ReadAloud;

/** Interface for providing the external ReadAloud object. */
public interface ReadAloudHooks {
    /**
     * Returns a ReadAloud instance. Callers should first check to make sure the ReadAloud feature
     * is available and the page is readable.
     */
    ReadAloud getReadAloud();
}
