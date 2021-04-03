// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Information for a parent account.
 * @typedef {{
 *   email: string,
 *   displayName: string,
 *   profileImage: string,
 *   obfuscatedGaiaId: string
 * }}
 */
export let ParentAccount;

/**
 * Failure result of parentSignin call.
 * @typedef {{
 *   isWrongPassword: boolean
 * }}
 */
export let ParentSigninFailureResult;

/**
 * Additional EDU-specific params for 'completeLogin' call.
 * @typedef {{
 *   reAuthProofToken: string,
 *   parentObfuscatedGaiaId: string,
 * }}
 */
export let EduLoginParams;

/**
 * Type of the error screen.
 * @enum {string}
 */
export const EduLoginErrorType = {
  NO_INTERNET: 'NO_INTERNET',
  // All other errors
  CANNOT_ADD_ACCOUNT: 'CANNOT_ADD_ACCOUNT',
};
