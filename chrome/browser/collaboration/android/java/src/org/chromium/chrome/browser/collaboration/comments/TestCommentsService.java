// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration.comments;

import org.chromium.components.collaboration.comments.CommentsService;

/** Test implementation of the CommentsService. */
class TestCommentsService implements CommentsService {
    @Override
    public boolean isInitialized() {
        return false;
    }

    @Override
    public boolean isEmptyService() {
        return false;
    }
}
