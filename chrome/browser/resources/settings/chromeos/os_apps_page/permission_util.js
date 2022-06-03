// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';

import {PermissionType, PermissionValue, TriState} from './permission_constants.js';

/**
 * @param {PermissionType} permissionType
 * @param {PermissionValue} value
 * @param {boolean} isManaged
 * @return {!apps.mojom.Permission}
 */
export function createPermission(permissionType, value, isManaged) {
  return {
    permissionType,
    value,
    isManaged,
  };
}

/**
 * @param {TriState} value
 * @returns {PermissionValue}
 */
export function createTriStatePermissionValue(value) {
  return {tristateValue: value};
}

/**
 * @param {PermissionValue} permissionValue
 * @returns {TriState}
 */
export function getTriStatePermissionValue(permissionValue) {
  assert(isTriStateValue(permissionValue));
  return permissionValue['tristateValue'];
}

/**
 * @param {boolean} value
 * @returns {PermissionValue}
 */
export function createBoolPermissionValue(value) {
  return {boolValue: value};
}

/**
 * @param {PermissionValue} permissionValue
 * @returns {boolean}
 */
export function getBoolPermissionValue(permissionValue) {
  assert(isBoolValue(permissionValue));
  return permissionValue['boolValue'];
}

/**
 * @param {PermissionValue} permissionValue
 * @returns {boolean}
 */
export function isTriStateValue(permissionValue) {
  return permissionValue['tristateValue'] !== undefined &&
      permissionValue['boolValue'] === undefined;
}

/**
 * @param {PermissionValue} permissionValue
 * @returns {boolean}
 */
export function isBoolValue(permissionValue) {
  return permissionValue['boolValue'] !== undefined &&
      permissionValue['tristateValue'] === undefined;
}

/**
 * @param {PermissionType} permissionType
 * @param {boolean} value
 * @param {boolean} isManaged
 * @return {!apps.mojom.Permission}
 */
export function createBoolPermission(permissionType, value, isManaged) {
  return createPermission(
      permissionType, createBoolPermissionValue(value), isManaged);
}

/**
 * @param {PermissionType} permissionType
 * @param {TriState} value
 * @param {boolean} isManaged
 * @return {!apps.mojom.Permission}
 */
export function createTriStatePermission(permissionType, value, isManaged) {
  return createPermission(
      permissionType, createTriStatePermissionValue(value), isManaged);
}

/**
 * @param {PermissionValue} permissionValue
 * @returns {boolean}
 */
export function isPermissionEnabled(permissionValue) {
  if (isBoolValue(permissionValue)) {
    return getBoolPermissionValue(permissionValue);
  } else if (isTriStateValue(permissionValue)) {
    return getTriStatePermissionValue(permissionValue) === TriState.kAllow;
  } else {
    assertNotReached();
  }
}
