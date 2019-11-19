// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import java.util.List;

/**
 * Interface for direct action containers.
 */
interface AutofillAssistantDirectAction {
    /** Returns a list of name and aliases for this direct action. */
    List<String> getNames();

    /** Returns the list of required argument names. */
    List<String> getRequiredArguments();

    /** Returns the list of optional argument names. */
    List<String> getOptionalArguments();
}
