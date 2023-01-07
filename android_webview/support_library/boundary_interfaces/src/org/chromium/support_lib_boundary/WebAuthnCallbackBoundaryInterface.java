// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.content.Intent;

/**
 * Boundary interface for the response to a WebAuthn intent.
 * <p>
 * An instance of this object is given to the embedder in <code>onWebAuthnIntent</code>.
 * Once the result of the intent is known the method in this interface is called so that the
 * embedder can pass the result back into Chromium.
 */
public interface WebAuthnCallbackBoundaryInterface {
    void onResult(int resultCode, Intent intent);
}
