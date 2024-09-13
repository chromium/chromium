// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The data model for a tab group changes as soon as a collaboration id is created, and operations
 * like delete/leave need to be aware of this. However visually we do not want to show any change to
 * the user until there's at least one other user in the group. This enum combines these
 * configurations allowing us to pass around a single state.
 */
@IntDef({
    GroupSharedState.NOT_SHARED,
    GroupSharedState.COLLABORATION_ONLY,
    GroupSharedState.HAS_OTHER_USERS,
})
@Retention(RetentionPolicy.SOURCE)
@interface GroupSharedState {
    int NOT_SHARED = 0;
    int COLLABORATION_ONLY = 1;
    int HAS_OTHER_USERS = 2;
}
