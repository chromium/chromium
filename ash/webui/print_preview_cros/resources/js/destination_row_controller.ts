// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'destination-row-controller' defines events and event handlers to
 * correctly consume changes from mojo providers and inform the
 * `destination-row` element to update. The controller is responsible for
 * tracking destination status updates and informing the UI to update to match.
 */

export class DestinationRowController extends EventTarget {}
