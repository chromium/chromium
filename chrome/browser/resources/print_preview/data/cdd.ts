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
  value: string,
  display_name?: string,
  display_name_localized?: LocalizedString[],
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

/**
 * Values matching the types of duplex in a CDD.
 */
export enum DuplexType {
  NO_DUPLEX = 'NO_DUPLEX',
  LONG_EDGE = 'LONG_EDGE',
  SHORT_EDGE = 'SHORT_EDGE',
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

export enum VendorCapabilityType {
  RANGE = 'RANGE',
  SELECT = 'SELECT',
  TYPED_VALUE = 'TYPED_VALUE',
  UNKNOWN = 'UNKNOWN',
}

interface VendorCapabilityBase {
  type: VendorCapabilityType;
  id: string;
  display_name?: string;
  display_name_localized?: LocalizedString[];
}

interface VendorCapabilityUnknown extends VendorCapabilityBase {
  type: VendorCapabilityType.UNKNOWN;
  id: '';
}

interface VendorCapabilityRange extends VendorCapabilityBase {
  type: VendorCapabilityType.RANGE;
  range_cap: RangeCapability;
}

interface VendorCapabilitySelect extends VendorCapabilityBase {
  type: VendorCapabilityType.SELECT;
  select_cap: SelectCapability;
}

interface VendorCapabilityTypedValue extends VendorCapabilityBase {
  type: VendorCapabilityType.TYPED_VALUE;
  typed_value_cap: TypedValueCapability;
}

/**
 * Specifies a custom vendor capability.
 */
export type VendorCapability = VendorCapabilityUnknown|VendorCapabilityRange|
    VendorCapabilitySelect|VendorCapabilityTypedValue;

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
  dpi?: DpiCapability;
}

/**
 * The CDD (Cloud Device Description) describes the capabilities of a print
 * destination.
 */
export interface Cdd {
  version: string;
  printer: CddCapabilities;
}
