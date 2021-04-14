// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions of all types related to output.
 * TODO: move other output types here.
 */

goog.provide('OutputContextOrder');

/**
 * The ordering of contextual output.
 * @enum {string}
 */
OutputContextOrder = {
  // The (ancestor) context comes before the node output.
  FIRST: 'first',
  // The (ancestor) context comes before the node output when moving forward,
  // after when moving backward.
  DIRECTED: 'directed',
  // The (ancestor) context comes after the node output.
  LAST: 'last'
};
