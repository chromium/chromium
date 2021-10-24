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
export const AcceleratorSource = {
  kAsh: 0,
  kEventRewriter: 1,
  kBrowser: 2,
  kAndroid: 3,
};

/**
 * Enumeration of accelerator types.
 * @enum {number}
 */
export const AcceleratorType = {
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
export const AcceleratorState = {
  kEnabled: 0,
  kDisabledByConflict: 1,
  kDisabledByUser: 2,
};

/**
 * Enumeration of accelerator config results from adding/replacing/removing an
 * accelerator.
 * @enum {number}
 */
export const AcceleratorConfigResult = {
  kSuccess: 0,
  kActionLocked: 1,
  kAcceleratorLocked: 2,
  kConflict: 3,
  kNotFound: 4,
  kDuplicate: 5,
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
 *   locked: boolean,
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
 * Enumeration of layout styles.
 * @enum {number}
 */
export const LayoutStyle = {
  kDefault: 0,
};

/**
 * Type alias for LayoutInfo. This describes one row (corresponding to an
 * AcceleratorRow) within the layout hierarchy. The category, sub-category,
 * and description are resource ID's that resolve to localized strings.
 *
 * The category provides grouping for the left navigation panel, and the
 * sub-category provides grouping for a section within a page.
 *
 * The source and action provide a lookup key into AcceleratorConfig
 * to determine the list of accelerators.
 *
 * The layout_style is an enum that allows for customization for special
 * cases. In most cases this will be kDefault.
 * @typedef {{
 *   category: number,
 *   sub_category: number,
 *   description: number,
 *   layout_style: !LayoutStyle,
 *   source: !AcceleratorSource,
 *   action: number,
 * }}
 */
export let LayoutInfo;

/**
 * Type alias for an array of LayoutItem.
 * @typedef {!Array<!LayoutInfo>}
 */
export let LayoutInfoList;

/**
 * Type alias for the ShortcutProviderInterface.
 * TODO(zentaro): Replace with a real mojo type when implemented.
 * @typedef {{
 *   getAllAcceleratorConfig: !function(): !Promise<!AcceleratorConfig>,
 *   getLayoutInfo: !function(): !Promise<LayoutInfoList>,
 *   isMutable: !function(!AcceleratorSource): !Promise<boolean>,
 * }}
 */
export let ShortcutProviderInterface;
