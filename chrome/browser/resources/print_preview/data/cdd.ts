// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

interface OptionWithDefault {
  is_default?: boolean;
}

export interface LocalizedString {
  locale: string;
  value: string;
}

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

interface SelectCapability {
  option?: VendorCapabilitySelectOption[];
}

interface TypedValueCapability {
  default?: number|string|boolean;
  value_type?: VendorCapabilityValueType;
}

interface RangeCapability {
  default: number;
}

/**
 * Specifies a custom vendor capability.
 */
export interface VendorCapability {
  id: string;
  display_name?: string;
  display_name_localized?: LocalizedString[];
  type: string;
  select_cap?: SelectCapability;
  typed_value_cap?: TypedValueCapability;
  range_cap?: RangeCapability;
}

export interface CapabilityWithReset {
  reset_to_default?: boolean;
  option: OptionWithDefault[];
}

export type ColorOption = {
  type?: string,
  vendor_id?: string,
  custom_display_name?: string,
}&OptionWithDefault;

export type ColorCapability = {
  option: ColorOption[],
}&CapabilityWithReset;

interface CollateCapability {
  default?: boolean;
}

export interface CopiesCapability {
  default?: number;
  max?: number;
}

export type DuplexOption = {
  type?: string,
}&OptionWithDefault;

type DuplexCapability = {
  option: DuplexOption[],
}&CapabilityWithReset;

export type PageOrientationOption = {
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
  imageable_area_left_microns?: number,
  imageable_area_bottom_microns?: number,
  imageable_area_right_microns?: number,
  imageable_area_top_microns?: number,
  has_borderless_variant?: boolean,
}&SelectOption;

export type MediaSizeCapability = {
  option: MediaSizeOption[],
}&CapabilityWithReset;

export type MediaTypeOption = {
  vendor_id: string,
}&SelectOption;

export type MediaTypeCapability = {
  option: MediaTypeOption[],
}&CapabilityWithReset;

export type DpiOption = {
  vendor_id?: string, horizontal_dpi: number, vertical_dpi: number,
}&OptionWithDefault;

export type DpiCapability = {
  option: DpiOption[],
}&CapabilityWithReset;

interface PinCapability {
  supported?: boolean;
}


/**
 * Capabilities of a print destination represented in a CDD.
 * Pin capability is not a part of standard CDD description and is defined
 * only on Chrome OS.
 */
export interface CddCapabilities {
  vendor_capability?: VendorCapability[];
  collate?: CollateCapability;
  color?: ColorCapability;
  copies?: CopiesCapability;
  duplex?: DuplexCapability;
  page_orientation?: PageOrientationCapability;
  media_size?: MediaSizeCapability;
  media_type?: MediaTypeCapability;
  dpi?: DpiCapability;
  // <if expr="is_chromeos">
  pin?: PinCapability;
  // </if>
}

/**
 * The CDD (Cloud Device Description) describes the capabilities of a print
 * destination.
 */
export interface Cdd {
  version: string;
  printer: CddCapabilities;
}
