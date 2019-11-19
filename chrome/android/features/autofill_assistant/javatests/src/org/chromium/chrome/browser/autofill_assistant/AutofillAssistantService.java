// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.chrome.browser.autofill_assistant.proto.ActionsResponseProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportsScriptResponseProto;

/**
 * Interface for a Java-side autofill assistant service.
 */
public interface AutofillAssistantService {
    /**
     * Get scripts for a given {@code url}, which should be a valid URL.
     * @return the response proto of the service.
     */
    SupportsScriptResponseProto getScriptsForUrl(String url);

    /**
     * Get actions.
     * @return the response proto of the service.
     */
    ActionsResponseProto getActions(
            String scriptPath, String url, byte[] globalPayload, byte[] scriptPayload);

    /**
     * Get next sequence of actions according to server payloads in previous response.
     * @return the response proto of the service.
     */
    ActionsResponseProto getNextActions(byte[] globalPayload, byte[] scriptPayload);
}
