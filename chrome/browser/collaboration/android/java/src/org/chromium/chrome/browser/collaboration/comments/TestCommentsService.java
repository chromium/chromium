// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration.comments;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.collaboration.comments.CommentsService;
import org.chromium.url.GURL;

import java.util.UUID;

/** Test implementation of the CommentsService. */
@NullMarked
class TestCommentsService implements CommentsService {
    @Override
    public boolean isInitialized() {
        return false;
    }

    @Override
    public boolean isEmptyService() {
        return false;
    }

    @Override
    public UUID addComment(
            String collaborationId,
            GURL url,
            String content,
            @Nullable UUID parentCommentId,
            Callback<Boolean> successCallback) {
        return UUID.randomUUID();
    }

    @Override
    public void editComment(UUID commentId, String newContent, Callback<Boolean> successCallback) {}

    @Override
    public void deleteComment(UUID commentId, Callback<Boolean> successCallback) {}

    @Override
    public void queryComments(
            FilterCriteria filterCriteria,
            PaginationCriteria paginationCriteria,
            Callback<QueryResult> callback) {}

    @Override
    public void addObserver(CommentsObserver observer, FilterCriteria filterCriteria) {}

    @Override
    public void removeObserver(CommentsObserver observer) {}
}
