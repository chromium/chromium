// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, PrinterType} from '../utils/print_preview_cros_app_types.js';

/**
 * @fileoverview
 * 'destination_constants' contains data structures and other constants related
 * to destinations used in multiple files.
 */

// TODO(b/323585997): Replace display name with localized string.
export const PDF_DESTINATION: Destination = {
  id: 'SAVE_AS_PDF',
  displayName: 'Save as PDF',
  printerType: PrinterType.PDF_PRINTER,
  printerStatusReason: null,
};
