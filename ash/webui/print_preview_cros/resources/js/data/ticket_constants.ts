// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ColorModel, DuplexMode, type PrintTicket} from '../utils/print_preview_cros_app_types.js';

/**
 * @fileoverview
 * 'ticket_constants' contains data structures and other constants related
 * to the print and preview ticket used in multiple files.
 */

// Default based on settings defaults described in createSettings function.
// See: chrome/browser/resources/print_preview/data/model.ts.
export const DEFAULT_PARTIAL_PRINT_TICKET: Partial<PrintTicket> = {
  collate: true,
  color: ColorModel.COLOR,
  copies: 1,
  dpiHorizontal: 0,
  dpiVertical: 0,
  dpiDefault: false,
  duplex: DuplexMode.LONG_EDGE,
};
