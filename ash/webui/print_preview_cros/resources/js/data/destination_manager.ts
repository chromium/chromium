// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination} from '../utils/print_preview_cros_app_types.js';

import {PDF_DESTINATION} from './destination_constants.js';

/**
 * @fileoverview
 * 'destination_manager' responsible for storing data related to available print
 * destinations as well as signaling updates to subscribed listeners. Manager
 * follows the singleton design pattern to enable access to a shared instance
 * across the app.
 */

export class DestinationManager extends EventTarget {
  private static instance: DestinationManager|null = null;

  static getInstance(): DestinationManager {
    if (DestinationManager.instance === null) {
      DestinationManager.instance = new DestinationManager();
    }

    return DestinationManager.instance;
  }

  static resetInstanceForTesting(): void {
    DestinationManager.instance = null;
  }

  // Non-static properties:
  private destinations: Destination[] = [
    // Digital destinations can be added at creation and will be removed if not
    // supported by policy.
    PDF_DESTINATION,
  ];

  // TODO(b/323585997): Returns true if initial fetch has returned
  // and there are valid destinations available in the destination
  // cache.
  hasInitialDestinationsLoaded(): boolean {
    return false;
  }

  // Retrieve a list of all known destinations.
  getDestinations(): Destination[] {
    return this.destinations;
  }
}
