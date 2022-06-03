// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Message definitions passed over the Projector app privileged/unprivileged
 * pipe.
 */

/**
 * Enum for tool types supported in the Ink engine.
 *
 * See http://go/ink-tools for details on each tool.
 *
 * @enum {string}
 */
export const AnnotatorToolType = {
  MARKER: 'marker',
  PEN: 'pen',
  HIGHLIGHTER: 'highlighter',
  ERASER: 'eraser',
};



/**
 * Enum for projector error types supported.
 * @enum {string}
 */
export const ProjectorError = {
  NONE: 'NONE',
  TOKEN_FETCH_FAILURE: 'TOKEN_FETCH_FAILURE',
  TOKEN_FETCH_ALREADY_IN_PROGRESS: 'TOKEN_FETCH_ALREADY_IN_PROGRESS',
  OTHER: 'OTHER',
};
