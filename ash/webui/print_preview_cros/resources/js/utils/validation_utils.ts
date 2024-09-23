// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DestinationManager} from '../data/destination_manager.js';

/**
 * @fileoverview
 * 'validation_utils' contains helper functions for validating changes to
 * ticket values used in the print ticket manager and capability controllers.
 */

// Ensures provided destination ID exists in destination manager.
export function isValidDestination(destinationId: string): boolean {
  const destinationManager = DestinationManager.getInstance();
  return destinationManager.isSessionInitialized() &&
      destinationManager.destinationExists(destinationId);
}
