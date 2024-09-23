// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import * as AcceleratorTypes from 'chrome://resources/mojo/ui/base/accelerators/mojom/accelerator.mojom-webui.js';

import * as AcceleratorConfigurationTypes from '../mojom-webui/accelerator_configuration.mojom-webui.js';
import * as AcceleratorInfoTypes from '../mojom-webui/accelerator_info.mojom-webui.js';
import * as MetaKeyTypes from '../mojom-webui/meta_key.mojom-webui.js';
import {SearchHandler, SearchHandlerInterface, SearchResult, SearchResultsAvailabilityObserverRemote} from '../mojom-webui/search.mojom-webui.js';
import {AcceleratorConfigurationProviderInterface, AcceleratorResultData, AcceleratorsUpdatedObserverRemote, UserAction} from '../mojom-webui/shortcut_customization.mojom-webui.js';


/**
 * @fileoverview
 * Type aliases for the mojo API.
 */

/**
 * Modifier values are based off of ui::Accelerator. Must be kept in sync with
 * ui::Accelerator and ui::KeyEvent.
 */
export enum Modifier {
  NONE = 0,
  SHIFT = 1 << 1,
  CONTROL = 1 << 2,
  ALT = 1 << 3,
  COMMAND = 1 << 4,
  FN_KEY = 1 << 5,
}

/**
 * The actions that can be done in the accelerator edit dialog. These should
 * be consistent with the representation in
 * `ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom`.
 */
export enum EditAction {
  NONE = 0,
  ADD = 1 << 0,
  EDIT = 1 << 1,
  REMOVE = 1 << 2,
  RESET = 1 << 3,
}

export type TextAcceleratorPart = AcceleratorInfoTypes.TextAcceleratorPart;
export type TextAcceleratorPartType =
    AcceleratorInfoTypes.TextAcceleratorPartType;
export const TextAcceleratorPartType =
    AcceleratorInfoTypes.TextAcceleratorPartType;

/**
 * A string of the form `{source}-{action_id}`.
 * This concatenation uniquely identifies one {@link Accelerator}.
 */
export type AcceleratorId = string;

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

export type AcceleratorKeyState = AcceleratorTypes.AcceleratorKeyState;
export const AcceleratorKeyState = AcceleratorTypes.AcceleratorKeyState;

/**
 * Enumeration of accelerator config results from adding/replacing/removing an
 * accelerator.
 */
export type AcceleratorConfigResult =
    AcceleratorConfigurationTypes.AcceleratorConfigResult;
export const AcceleratorConfigResult =
    AcceleratorConfigurationTypes.AcceleratorConfigResult;

/**
 * Enumeration of meta key denoting all the possible options deducable from
 * the users keyboard. Used to show the correct key to the user in the settings
 * UI.
 */
export type MetaKey = MetaKeyTypes.MetaKey;
export const MetaKey = MetaKeyTypes.MetaKey;

/**
 * Type alias for Accelerator.
 *
 * The Pick utility type is used here because only `keyCode`, `modifiers`, and
 * `keyState` are necessary for this app.
 */
export type Accelerator =
    Pick<AcceleratorTypes.Accelerator, 'keyCode'|'modifiers'|'keyState'>;

export type MojoAccelerator = AcceleratorTypes.Accelerator;

/**
 * Type alias for AcceleratorInfo.
 *
 * The utility type Omit and the intersection type operator (&) are used here
 * to replace the types of `accelerator` and `keyDisplay` with more accurate
 * types.
 */
export type StandardAcceleratorInfo =
    Omit<AcceleratorInfoTypes.AcceleratorInfo, 'layoutProperties'>&{
      layoutProperties: {
        standardAccelerator: {
          accelerator: Accelerator,
          keyDisplay: string,
          originalAccelerator?: Accelerator,
        },
      },
    };

export type TextAcceleratorInfo = Omit<
    AcceleratorInfoTypes.AcceleratorInfo,
    'layoutProperties'|'acceleratorLocked'>&{
  layoutProperties: {
    textAccelerator: {parts: AcceleratorInfoTypes.TextAcceleratorPart[]},
  },
};

export type AcceleratorInfo = TextAcceleratorInfo|StandardAcceleratorInfo;

/**
 * Type alias for the Mojo version of AcceleratorInfo.
 */
export type MojoAcceleratorInfo = AcceleratorInfoTypes.AcceleratorInfo;

/**
 * This generic AcceleratorConfig is used to represent both the raw
 * MojoAcceleratorConfig (which comes from the backend) and the sanitized
 * AcceleratorConfig (which is used throughout the app).
 *
 * This is a two level map, with the top level identifying the source of the
 * shortcuts, and second level the integer id for the action with the leaf value
 * being a list of AcceleratorInfo.
 */
export type GenericAcceleratorConfig<AccelInfoType> = {
  [source in AcceleratorSource]?: {[actionId: number]: AccelInfoType[]}
};

/** Type alias for AcceleratorConfig. */
export type AcceleratorConfig = GenericAcceleratorConfig<AcceleratorInfo>;

/** Type alias for MojoAcceleratorConfig. */
export type MojoAcceleratorConfig =
    GenericAcceleratorConfig<MojoAcceleratorInfo>;

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

export type ShortcutSearchHandler = SearchHandler;
export const ShortcutSearchHandler = SearchHandler;

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

/**
 * Alias for the Mojo version of AcceleratorLayoutInfo so that
 * it's clearer throughout the app whether we're using the Mojo
 * version or the app version ({@link LayoutInfo}).
 */
export type MojoLayoutInfo = AcceleratorInfoTypes.AcceleratorLayoutInfo;

/**
 * Type alias for the Mojo version of SearchResult.
 */
export type MojoSearchResult = SearchResult;

/**
 * Type alias for the ShortcutSearchHandlerInterface.
 */
export interface ShortcutSearchHandlerInterface extends SearchHandlerInterface {
  search(query: String16, maxNumResults: number):
      Promise<{results: MojoSearchResult[]}>;
  addSearchResultsAvailabilityObserver(
      observer: SearchResultsAvailabilityObserverRemote): void;
}

/**
 * Type alias for the ShortcutProviderInterface.
 */
export interface ShortcutProviderInterface extends
    AcceleratorConfigurationProviderInterface {
  getAccelerators(): Promise<{config: MojoAcceleratorConfig}>;
  getAcceleratorLayoutInfos(): Promise<{layoutInfos: MojoLayoutInfo[]}>;
  getDefaultAcceleratorsForId(action: number):
      Promise<{accelerators: Accelerator[]}>;
  isMutable(source: AcceleratorSource): Promise<{isMutable: boolean}>;
  getMetaKeyToDisplay(): Promise<{metaKey: MetaKey}>;
  addAccelerator(
      source: AcceleratorSource, action: number,
      accelerator: Accelerator): Promise<{result: AcceleratorResultData}>;
  removeAccelerator(
      source: AcceleratorSource, action: number,
      accelerator: Accelerator): Promise<{result: AcceleratorResultData}>;
  replaceAccelerator(
      source: AcceleratorSource, action: number, oldAccelerator: Accelerator,
      newAccelerator: Accelerator): Promise<{result: AcceleratorResultData}>;
  addObserver(observer: AcceleratorsUpdatedObserverRemote): void;
  restoreDefault(source: AcceleratorSource, actionId: number):
      Promise<{result: AcceleratorResultData}>;
  restoreAllDefaults(): Promise<{result: AcceleratorResultData}>;
  preventProcessingAccelerators(preventProcessingAccelerators: boolean):
      Promise<void>;
  recordUserAction(userAction: UserAction): void;
}
