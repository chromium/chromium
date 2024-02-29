// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'print_preview_cros_app_types' contains app specific and mojo placeholder
 * types.
 */

export interface PrintRequestOutcome {
  success: boolean;
  error?: string;
}

// Placeholder for PrintPreviewPageHandler mojo interface.
export interface PrintPreviewPageHandler {
  // Start the print job and close the window. Needs to wait for result to
  // display error messaging if starting the print job fails.
  print(): Promise<PrintRequestOutcome>;

  // Cancel the print preview and close the window.
  cancel(): void;
}
