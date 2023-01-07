// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.lifetime;

/** Interface for items that require a controlled clean up. */
public interface Destroyable {
    /** Cleans up resources held by the implementing object. */
    void destroy();
}
