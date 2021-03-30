// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

/**
 * Boundary interface for AwContents.addDocumentStartJavascript().
 *
 * TODO(ctzsm): Delete this interface once we've updated the APKs on
 * the AndroidX bots and move the remove method to ScriptHandlerBoundaryInterface.
 */
public interface ScriptReferenceBoundaryInterface {
    void remove();
}
