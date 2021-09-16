// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export type VendorCapabilitySelectOption = {
  display_name: string,
  type?: string, value: number|string|boolean,
  is_default?: boolean,
};

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
  default: number
};

/**
 * Specifies a custom vendor capability.
 */
export type VendorCapability = {
  id: string,
  display_name: string,
  localized_display_name?: string, type: string,
  select_cap?: SelectCapability,
  typed_value_cap?: TypedValueCapability,
  range_cap?: RangeCapability,
};

type ColorOption = {
  type?: string,
  vendor_id?: string,
  custom_display_name?: string,
  is_default?: boolean,
};

type ColorCapability = {
  option: ColorOption[],
  reset_to_default?: boolean,
};

type CollateCapability = {
  default?: boolean
};

type CopiesCapability = {
  default?: number,
  max?: number
};

type DuplexOption = {
  type?: string,
  is_default?: boolean
};

type DuplexCapability = {
  option: DuplexOption[],
  reset_to_default?: boolean,
};

type PageOrientationOption = {
  type?: string,
  is_default?: boolean,
};

type PageOrientationCapability = {
  option: PageOrientationOption[],
  reset_to_default?: boolean,
};

type MediaSizeOption = {
  type?: string,
  vendor_id?: string,
  custom_display_name?: string,
  is_default?: boolean,
  name?: string,
};

type MediaSizeCapability = {
  option: MediaSizeOption[],
  reset_to_default?: boolean,
};

type DpiOption = {
  vendor_id?: string, horizontal_dpi: number, vertical_dpi: number,
  is_default?: boolean,
};

type DpiCapability = {
  option: DpiOption[],
  reset_to_default?: boolean,
};

type PinCapability = {
  supported?: boolean
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
  pin?: PinCapability,
};

/**
 * The CDD (Cloud Device Description) describes the capabilities of a print
 * destination.
 */
export type Cdd = {
  version: string,
  printer: CddCapabilities,
};
