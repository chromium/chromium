// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as AcceleratorTypes from 'chrome://resources/mojo/ui/base/accelerators/mojom/accelerator.mojom-webui.js';

import * as AcceleratorInfoTypes from '../mojom-webui/ash/public/mojom/accelerator_info.mojom-webui.js';

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

/**
 * In TypeScript, an enum is both a value and type,
 * so we have to export each one separately.
 */
/** Enumeration of accelerator sources. */
export type AcceleratorSource = AcceleratorInfoTypes.AcceleratorSource;
export const AcceleratorSource = AcceleratorInfoTypes.AcceleratorSource;

/** Enumeration of accelerator types. */
export type AcceleratorType = AcceleratorInfoTypes.AcceleratorType;
export const AcceleratorType = AcceleratorInfoTypes.AcceleratorType;

/** Enumeration of accelerator states. */
export type AcceleratorState = AcceleratorInfoTypes.AcceleratorState;
export const AcceleratorState = AcceleratorInfoTypes.AcceleratorState;

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

/**
 * Type alias for Accelerator.
 *
 * The Pick utility type is used here because only `keyCode` and `modifiers`
 * are necessary for this app.
 */
export type Accelerator =
    Pick<AcceleratorTypes.Accelerator, 'keyCode'|'modifiers'>;

/**
 * Type alias for AcceleratorInfo.
 *
 * The utility type Omit and the intersection type operator (&) are used here
 * to replace the types of `accelerator` and `keyDisplay` with more accurate
 * types.
 */
export type AcceleratorInfo =
    Omit<AcceleratorInfoTypes.AcceleratorInfo, 'accelerator'|'keyDisplay'>&
    {accelerator: Accelerator, keyDisplay: string};

/**
 * Type alias for AcceleratorConfig. This is a two level map, with the top
 * level identifying the source of the shortcuts, and second level the integer
 * id for the action with the leaf value being a list of Accelerator Info.
 */
export type AcceleratorConfig = {
  [source in AcceleratorSource]?: {[actionId: number]: AcceleratorInfo[]}
};

/** Enumeration of accelerator subcategory. */
export type AcceleratorSubcategory =
    AcceleratorInfoTypes.AcceleratorSubcategory;
export const AcceleratorSubcategory =
    AcceleratorInfoTypes.AcceleratorSubcategory;

/** Enumeration of accelerator category. */
export type AcceleratorCategory = AcceleratorInfoTypes.AcceleratorCategory;
export const AcceleratorCategory = AcceleratorInfoTypes.AcceleratorCategory;

/** Enumeration of layout styles.*/
export type LayoutStyle = AcceleratorInfoTypes.AcceleratorLayoutStyle;
export const LayoutStyle = AcceleratorInfoTypes.AcceleratorLayoutStyle;

/**
 * Type alias for LayoutInfo. This describes one row (corresponding to an
 * AcceleratorRow) within the layout hierarchy. The `category`, `subCategory`,
 * and `description` properties are resource ID's that resolve to localized
 * strings.
 *
 * The `category` property provides grouping for the left navigation panel, and
 * the `subCategory` provides grouping for a section within a page.
 *
 * The `source` and `action` properties provide a lookup key into
 * AcceleratorConfig to determine the list of accelerators.
 *
 * The `style` property is an enum that allows for customization for special
 * cases. In most cases this will be `kDefault`.
 *
 * The utility type Omit and the intersection type operator (&) to replace the
 * types of `description` and `style` with more accurate types.
 */
export type LayoutInfo =
    Omit<AcceleratorInfoTypes.AcceleratorLayoutInfo, 'description'|'style'>&
    {description: string, style: LayoutStyle};

/** Type alias for an array of LayoutItem. */
export type LayoutInfoList = LayoutInfo[];

/**
 * Type alias for the ShortcutProviderInterface.
 * TODO(zentaro): Replace with a real mojo type when implemented.
 */
export interface ShortcutProviderInterface {
  getAccelerators(): Promise<{config: AcceleratorConfig}>;
  getLayoutInfo(): Promise<LayoutInfoList>;
  isMutable(source: AcceleratorSource): Promise<{isMutable: boolean}>;
  removeAccelerator(
      source: AcceleratorSource, action: number,
      accelerator: Accelerator): Promise<AcceleratorConfigResult>;
  replaceAccelerator(
      source: AcceleratorSource, action: number, oldAccelerator: Accelerator,
      newAccelerator: Accelerator): Promise<AcceleratorConfigResult>;
  addUserAccelerator(
      source: AcceleratorSource, action: number,
      accelerator: Accelerator): Promise<AcceleratorConfigResult>;
}
