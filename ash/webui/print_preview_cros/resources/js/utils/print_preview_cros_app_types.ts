// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

/**
 * @fileoverview
 * 'print_preview_cros_app_types' contains app specific and mojo placeholder
 * types.
 */

// Common data for displaying and filtering print destinations.
export interface Destination {
  // ID can be the printer name or ID depending on the originating type of
  // printer.
  id: string;

  // Display name from printer.
  displayName: string;
}

export interface PrintRequestOutcome {
  success: boolean;
  error?: string;
}

// PrintTicket represents the data required to start print job. Ticket will be
// used to create a settings dictionary with fields matching the existing Chrome
// preview print settings for reusability.
// TODO(b/323421684): Add missing required settings to start print job.
export interface PrintTicket {
  // ID used to map a CrOS preview session to the responsible PrintViewManager
  // and related web contents.
  printPreviewId: UnguessableToken;
}

// Immutable session configuration details for the current CrOS preview request.
export interface SessionContext {
  // ID used to map a CrOS preview session to the responsible PrintViewManager
  // and related web contents.
  printPreviewId: UnguessableToken;
}

// Placeholder for PrintPreviewPageHandler mojo interface.
export interface PrintPreviewPageHandler {
  // Completes initialization on the backend and provides immutable
  // configuration details for the current CrOS preview request in the form of
  // a SessionContext.
  startSession(): Promise<SessionContext>;

  // Start the print job and close the window. Needs to wait for result to
  // display error messaging if starting the print job fails.
  print(): Promise<PrintRequestOutcome>;

  // Cancel the print preview and close the window.
  cancel(): void;
}

// Placeholder for the DestinationProvider mojo interface.
export interface DestinationProvider {
  // Retrieve a list of local print destinations; usually provided by CUPS.
  getLocalDestinations(): Promise<Destination[]>;
}
