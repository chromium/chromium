// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.view.contentcapture.ContentCaptureSession;

import org.chromium.build.annotations.NullMarked;

/** Interface to call unreleased Android APIs that are guarded by aconfig flags. */
@NullMarked
public interface AconfigFlaggedApiDelegate {
    /** Call ContentCaptureSession.flush() if supported, otherwise no-op. */
    default void flushContentCaptureSession(ContentCaptureSession session) {}
}
