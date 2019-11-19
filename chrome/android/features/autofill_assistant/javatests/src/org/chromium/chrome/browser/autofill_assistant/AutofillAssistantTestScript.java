// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;

import java.util.List;

/**
 * This class represents a script along with its set of actions. It is intended to be used in tests
 * as a replacement for actual scripts (returned from a remote server).
 */
public class AutofillAssistantTestScript {
    private final SupportedScriptProto mSupportedScript;
    private final List<ActionProto> mActions;

    public AutofillAssistantTestScript(
            SupportedScriptProto supportedScript, List<ActionProto> actions) {
        mSupportedScript = supportedScript;
        mActions = actions;
    }

    public SupportedScriptProto getSupportedScript() {
        return mSupportedScript;
    }

    public List<ActionProto> getActions() {
        return mActions;
    }
}
