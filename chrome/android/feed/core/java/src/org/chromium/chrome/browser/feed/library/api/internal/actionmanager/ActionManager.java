// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.actionmanager;

import androidx.annotation.Nullable;

import org.chromium.base.Consumer;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.wire.ActionPayloadProto.ActionPayload;

import java.util.List;

/** Allows Stream to notify the Feed library of actions taken */
public interface ActionManager extends ViewActionManager {
    public static enum UploadActionType { MISC, CLICK, VIEW }

    /**
     * Dismiss content for the content ID in the session, along with executing the provided stream
     * data operations on the session.
     *
     * @param contentIds The content IDs for the feature being dismissed. These are recorded and
     *         sent to the server in subsequent requests.
     * @param streamDataOperations Any stream data operations that should be applied to the session
     *     (e.g. removing a cluster when the content is removed)
     * @param sessionId The current session id
     */
    void dismissLocal(List<String> contentIds, List<StreamDataOperation> streamDataOperations,
            @Nullable String sessionId);

    /**
     * Executes the provided stream data operations on the session.
     *
     * @param contentId The id for the content that triggered this action.
     * @param streamDataOperations Any stream data operations that should be applied to the session
     *     (e.g. removing a cluster when the content is removed)
     * @param sessionId The current session id
     */
    void dismiss(List<StreamDataOperation> streamDataOperations, @Nullable String sessionId);

    /**
     * Issues a request to record a set of actions, with the consumer being called back with the
     * resulting {@link ConsistencyToken}.
     */
    void createAndUploadAction(
            String contentId, ActionPayload payload, UploadActionType actionType);

    /**
     * Issues a request to record a single action and store it for future upload.
     */
    void createAndStoreAction(String contentId, ActionPayload payload, UploadActionType actionType);

    /**
     * Issues a request to record a set of action and update the url with consistency token with the
     * consumer being called on the main thread with the resulting url.
     */
    void uploadAllActionsAndUpdateUrl(
            String url, String consistencyTokenQueryParamName, Consumer<String> consumer);

    /**
     * Sets the bit that determines whether clicks and views can be uploaded when the notice card is
     * present.
     */
    void setCanUploadClicksAndViewsWhenNoticeCardIsPresent(boolean canUploadClicksAndViews);
}
