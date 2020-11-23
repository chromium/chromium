// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   display_name: (string),
 *   type: (string | undefined),
 *   value: (number | string | boolean),
 *   is_default: (boolean | undefined),
 * }}
 */
export let VendorCapabilitySelectOption;

/**
 * Same as cloud_devices::printer::TypedValueVendorCapability::ValueType.
 * @enum {string}
 */
export const VendorCapabilityValueType = {
  BOOLEAN: 'BOOLEAN',
  FLOAT: 'FLOAT',
  INTEGER: 'INTEGER',
  STRING: 'STRING',
};

/**
 * Specifies a custom vendor capability.
 * @typedef {{
 *   id: (string),
 *   display_name: (string),
 *   localized_display_name: (string | undefined),
 *   type: (string),
 *   select_cap: ({
 *     option: (Array<!VendorCapabilitySelectOption>|undefined),
 *   }|undefined),
 *   typed_value_cap: ({
 *     default: (number | string | boolean | undefined),
 *     value_type: (VendorCapabilityValueType | undefined),
 *   }|undefined),
 *   range_cap: ({
 *     default: (number),
 *   }),
 * }}
 */
export let VendorCapability;

/**
 * Capabilities of a print destination represented in a CDD.
 * Pin capability is not a part of standard CDD description and is defined
 * only on Chrome OS.
 *
 * @typedef {{
 *   vendor_capability: (Array<!VendorCapability>|undefined),
 *   collate: ({default: (boolean|undefined)}|undefined),
 *   color: ({
 *     option: !Array<{
 *       type: (string|undefined),
 *       vendor_id: (string|undefined),
 *       custom_display_name: (string|undefined),
 *       is_default: (boolean|undefined)
 *     }>
 *   }|undefined),
 *   copies: ({default: (number|undefined),
 *             max: (number|undefined)}|undefined),
 *   duplex: ({option: !Array<{type: (string|undefined),
 *                             is_default: (boolean|undefined)}>}|undefined),
 *   page_orientation: ({
 *     option: !Array<{type: (string|undefined),
 *                      is_default: (boolean|undefined)}>
 *   }|undefined),
 *   media_size: ({
 *     option: !Array<{
 *       type: (string|undefined),
 *       vendor_id: (string|undefined),
 *       custom_display_name: (string|undefined),
 *       is_default: (boolean|undefined),
 *       name: (string|undefined),
 *     }>
 *   }|undefined),
 *   dpi: ({
 *     option: !Array<{
 *       vendor_id: (string|undefined),
 *       horizontal_dpi: number,
 *       vertical_dpi: number,
 *       is_default: (boolean|undefined)
 *     }>
 *   }|undefined),
 *   pin: ({supported: (boolean|undefined)}|undefined)
 * }}
 */
export let CddCapabilities;

/**
 * The CDD (Cloud Device Description) describes the capabilities of a print
 * destination.
 *
 * @typedef {{
 *   version: string,
 *   printer: !CddCapabilities,
 * }}
 */
export let Cdd;
