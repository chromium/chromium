// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enumeration of color mode restrictions used by Chromium.
 * This has to coincide with |printing::ColorModeRestriction| as defined in
 * printing/backend/printing_restrictions.h
 * @enum {number}
 */
export const ColorModeRestriction = {
  UNSET: 0x0,
  MONOCHROME: 0x1,
  COLOR: 0x2,
};

/**
 * Enumeration of duplex mode restrictions used by Chromium.
 * This has to coincide with |printing::DuplexModeRestriction| as defined in
 * printing/backend/printing_restrictions.h
 * @enum {number}
 */
export const DuplexModeRestriction = {
  UNSET: 0x0,
  SIMPLEX: 0x1,
  LONG_EDGE: 0x2,
  SHORT_EDGE: 0x4,
  DUPLEX: 0x6,
};

/**
 * Enumeration of PIN printing mode restrictions used by Chromium.
 * This has to coincide with |printing::PinModeRestriction| as defined in
 * printing/backend/printing_restrictions.h
 * @enum {number}
 */
export const PinModeRestriction = {
  UNSET: 0,
  PIN: 1,
  NO_PIN: 2,
};

/**
 * Enumeration of background graphics printing mode restrictions used by
 * Chromium.
 * This has to coincide with |printing::BackgroundGraphicsModeRestriction| as
 * defined in printing/backend/printing_restrictions.h
 * @enum {number}
 */
export const BackgroundGraphicsModeRestriction = {
  UNSET: 0,
  ENABLED: 1,
  DISABLED: 2,
};

/**
 * Policies affecting a destination.
 * @typedef {{
 *   allowedColorModes: ?ColorModeRestriction,
 *   allowedDuplexModes: ?DuplexModeRestriction,
 *   allowedPinMode: ?PinModeRestriction,
 *   allowedBackgroundGraphicsMode:
 *       ?BackgroundGraphicsModeRestriction,
 *   defaultColorMode: ?ColorModeRestriction,
 *   defaultDuplexMode: ?DuplexModeRestriction,
 *   defaultPinMode: ?PinModeRestriction,
 *   defaultBackgroundGraphicsMode:
 *       ?BackgroundGraphicsModeRestriction,
 * }}
 */
export let Policies;
