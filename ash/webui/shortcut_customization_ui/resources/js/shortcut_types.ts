// Copyright 2021 The Chromium Authors
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
 */
export enum Modifier {
  SHIFT = 1 << 1,
  CONTROL = 1 << 2,
  ALT = 1 << 3,
  COMMAND = 1 << 4,
}

/** Enumeration of accelerator sources. */
export enum AcceleratorSource {
  ASH,
  EVENT_REWRITER,
  BROWSER,
  ANDROID,
}

/** Enumeration of accelerator types. */
export enum AcceleratorType {
  DEFAULT,
  USER_DEFINED,
  DEPRECATED,
  DEVELOPER,
  DEBUG,
}

/** Enumeration of accelerator states. */
export enum AcceleratorState {
  ENABLED,
  DISABLED_BY_CONFLICT,
  DISABLED_BY_USER,
}

/**
 * Enumeration of accelerator config results from adding/replacing/removing an
 * accelerator.
 */
export enum AcceleratorConfigResult {
  SUCCESS,
  ACTION_LOCKED,
  ACCELERATOR_LOCKED,
  CONFLICT,
  NOT_FOUND,
  DUPLICATE,
}

export interface AcceleratorKeys {
  modifiers: number;
  key: number;
  keyDisplay: string;
}

export interface AcceleratorInfo {
  accelerator: AcceleratorKeys;
  type: AcceleratorType;
  state: AcceleratorState;
  locked: boolean;
}

/**
 * Type alias for AcceleratorConfig. This is a two level map, with the top
 * level identifying the source of the shortcuts, and second level the integer
 * id for the action with the leaf value being a list of Accelerator Info.
 */
export type AcceleratorConfig =
    Map<AcceleratorSource, Map<number, AcceleratorInfo[]>>;

/** Enumeration of layout styles.*/
export enum LayoutStyle {
  DEFAULT
}

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
 */
export interface LayoutInfo {
  category: number;
  sub_category: number;
  description: number;
  layout_style: LayoutStyle;
  source: AcceleratorSource;
  action: number;
}

/** Type alias for an array of LayoutItem. */
export type LayoutInfoList = LayoutInfo[];

/**
 * Type alias for the ShortcutProviderInterface.
 * TODO(zentaro): Replace with a real mojo type when implemented.
 */
export interface ShortcutProviderInterface {
  getAllAcceleratorConfig(): Promise<AcceleratorConfig>;
  getLayoutInfo(): Promise<LayoutInfoList>;
  isMutable(source: AcceleratorSource): Promise<boolean>;
  removeAccelerator(
      source: AcceleratorSource, action: number,
      accelerator: AcceleratorKeys): Promise<AcceleratorConfigResult>;
  replaceAccelerator(
      source: AcceleratorSource, action: number,
      oldAccelerator: AcceleratorKeys,
      newAccelerator: AcceleratorKeys): Promise<AcceleratorConfigResult>;
  addUserAccelerator(
      source: AcceleratorSource, action: number,
      accelerator: AcceleratorKeys): Promise<AcceleratorConfigResult>;
}
