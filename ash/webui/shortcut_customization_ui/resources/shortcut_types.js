// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Type aliases for the mojo API.
 *
 * TODO(zentaro): When the fake API is replaced by mojo these can be
 * re-aliased to the corresponding mojo types, or replaced by them.
 */

/**
 * Modifier values are based off of ui::Accelerator. Must be kept in sync with
 * ui::Accelerator and ui::KeyEvent.
 *
 * @enum {number}
 */
export const Modifier = {
  SHIFT: 1 << 1,
  CONTROL: 1 << 2,
  ALT: 1 << 3,
  COMMAND: 1 << 4,
};

/**
 * Enumeration of accelerator sources.
 * @enum {number}
 */
export let AcceleratorSource = {
  kAsh: 0,
  kEventRewriter: 1,
  kBrowser: 2,
  kAndroid: 3,
};

/**
 * Enumeration of accelerator types.
 * @enum {number}
 */
export let AcceleratorType = {
  kDefault: 0,
  kUserDefined: 1,
  kDeprecated: 2,
  kDeveloper: 3,
  kDebug: 4,
};

/**
 * Enumeration of accelerator states.
 * @enum {number}
 */
export let AcceleratorState = {
  kEnabled: 0,
  kDisabledByConflict: 1,
  kDisabledByUser: 2,
};

/**
 * Type alias for AcceleratorKeys.
 * @typedef {{
 *   modifiers: number,
 *   key: number,
 *   key_display: string,
 * }}
 */
export let AcceleratorKeys;

/**
 * Type alias for AcceleratorInfo.
 * @typedef {{
 *   accelerator: !AcceleratorKeys,
 *   type: !AcceleratorType,
 *   state: !AcceleratorState,
 * }}
 */
export let AcceleratorInfo;

/**
 * Type alias for AcceleratorConfig. This is a two level map, with the top
 * level identifying the source of the shortcuts, and second level the integer
 * id for the action with the leaf value being a list of Accelerator Info.
 * @typedef {!Map<!AcceleratorSource, !Map<number, !Array<!AcceleratorInfo>>>}
 */
export let AcceleratorConfig;

/**
 * Type alias for the ShortcutProviderInterface.
 * TODO(zentaro): Replace with a real mojo type when implemented.
 * @typedef {{
 *   getAllAcceleratorConfig: !function(): !Promise<!AcceleratorConfig>,
 * }}
 */
export let ShortcutProviderInterface;
