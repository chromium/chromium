// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DestinationManager} from './data/destination_manager.js';

/**
 * @fileoverview
 * 'destination-select-controller' defines events and event handlers to
 * correctly consume changes from mojo providers and inform the
 * `destination-select` element to update.
 */

// DestinationSelectController defines functionality used to update the
// `destination-select` element.
export class DestinationSelectController extends EventTarget {
  private destinationManager = DestinationManager.getInstance();

  // Returns whether destination manager has fetched initial destinations.
  shouldShowLoading(): boolean {
    return !this.destinationManager.hasInitialDestinationsLoaded();
  }
}
