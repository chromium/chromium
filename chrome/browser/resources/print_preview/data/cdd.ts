// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

type OptionWithDefault = {
  is_default?: boolean,
};

export type LocalizedString = {
  locale: string,
  value: string,
};

export type VendorCapabilitySelectOption = {
  display_name?: string,
  display_name_localized?: LocalizedString[], value: number|string|boolean,
}&OptionWithDefault;

/**
 * Same as cloud_devices::printer::TypedValueVendorCapability::ValueType.
 */
export enum VendorCapabilityValueType {
  BOOLEAN = 'BOOLEAN',
  FLOAT = 'FLOAT',
  INTEGER = 'INTEGER',
  STRING = 'STRING',
}

type SelectCapability = {
  option?: VendorCapabilitySelectOption[],
};

type TypedValueCapability = {
  default?: number|string|boolean,
  value_type?: VendorCapabilityValueType,
};

type RangeCapability = {
  default: number,
};

/**
 * Specifies a custom vendor capability.
 */
export type VendorCapability = {
  id: string,
  display_name?: string,
  display_name_localized?: LocalizedString[], type: string,
  select_cap?: SelectCapability,
  typed_value_cap?: TypedValueCapability,
  range_cap?: RangeCapability,
};

export type CapabilityWithReset = {
  reset_to_default?: boolean, option: OptionWithDefault[],
};

export type ColorOption = {
  type?: string,
  vendor_id?: string,
  custom_display_name?: string,
}&OptionWithDefault;

export type ColorCapability = {
  option: ColorOption[],
}&CapabilityWithReset;

type CollateCapability = {
  default?: boolean,
};

export type CopiesCapability = {
  default?: number,
  max?: number,
};

export type DuplexOption = {
  type?: string,
}&OptionWithDefault;

type DuplexCapability = {
  option: DuplexOption[],
}&CapabilityWithReset;

type PageOrientationOption = {
  type?: string,
}&OptionWithDefault;

type PageOrientationCapability = {
  option: PageOrientationOption[],
}&CapabilityWithReset;

export type SelectOption = {
  custom_display_name?: string,
  custom_display_name_localized?: LocalizedString[],
  name?: string,
}&OptionWithDefault;

export type MediaSizeOption = {
  type?: string,
  vendor_id?: string, height_microns: number, width_microns: number,
}&SelectOption;

export type MediaSizeCapability = {
  option: MediaSizeOption[],
}&CapabilityWithReset;

export type DpiOption = {
  vendor_id?: string, horizontal_dpi: number, vertical_dpi: number,
}&OptionWithDefault;

export type DpiCapability = {
  option: DpiOption[],
}&CapabilityWithReset;

type PinCapability = {
  supported?: boolean,
};


/**
 * Capabilities of a print destination represented in a CDD.
 * Pin capability is not a part of standard CDD description and is defined
 * only on Chrome OS.
 */
export type CddCapabilities = {
  vendor_capability?: VendorCapability[],
  collate?: CollateCapability,
  color?: ColorCapability,
  copies?: CopiesCapability,
  duplex?: DuplexCapability,
  page_orientation?: PageOrientationCapability,
  media_size?: MediaSizeCapability,
  dpi?: DpiCapability,
  // <if expr="chromeos_ash or chromeos_lacros">
  pin?: PinCapability,
  // </if>
};

/**
 * The CDD (Cloud Device Description) describes the capabilities of a print
 * destination.
 */
export type Cdd = {
  version: string,
  printer: CddCapabilities,
};
