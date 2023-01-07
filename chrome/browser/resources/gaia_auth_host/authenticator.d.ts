// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Minimal TypeScript definitions to satisfy cases where
 * authenticator.js is used from TypeScript files.
 */

import {PasswordAttributes} from './saml_password_attributes.js';

export interface SyncTrustedVaultKey {
  keyMaterial: ArrayBuffer;
  version: number;
}

export interface SyncTrustedRecoveryMethod {
  publicKey: ArrayBuffer;
  type: number;
}

export interface SyncTrustedVaultKeys {
  obfuscatedGaiaId: string;
  encryptionKeys: SyncTrustedVaultKey[];
  trustedRecoveryMethods: SyncTrustedRecoveryMethod[];
}

export interface AuthCompletedCredentials {
  chooseWhatToSync: boolean;
  email: string;
  gaiaId: string;
  passwordAttributes: PasswordAttributes;
  password: string;
  publicSAML: boolean;
  services: any[];
  sessionIndex: string;
  skipForNow: boolean;
  syncTrustedVaultKeys: SyncTrustedVaultKeys;
  trusted: boolean;
  usingSAML: boolean;
  isAvailableInArc?: boolean;
}

export interface AuthParams {
  authMode: AuthMode;
  clientId: string;
  constrained: string;
  dontResizeNonEmbeddedPages: boolean;
  emailDomain: string;
  email: string;
  enableGaiaActionButtons: boolean;
  enterpriseEnrollmentDomain: string;
  extractSamlPasswordAttributes: boolean;
  flow: string;
  gaiaPath: string;
  gaiaUrl: string;
  hl: string;
  ignoreCrOSIdpSetting: boolean;
  isDeviceOwner: boolean;
  isLoginPrimaryAccount: boolean;
  isSupervisedUser: boolean;
  platformVersion: string;
  readOnlyEmail: boolean;
  samlAclUrl: string;
  service: string;
  showTos: string;
  ssoProfile: string;
  urlParameterToAutofillSAMLUsername: string;
}

export enum AuthMode {
  DEFAULT = 0,
  OFFLINE = 1,
  DESKTOP = 2,
}

export enum AuthFlow {
  DEFAULT = 0,
  SAML = 0,
}

export class Authenticator extends EventTarget {
  constructor(webview: HTMLElement|string);
  getAccountsResponse(accounts: string[]): void;
  load(authMode: AuthMode, data: AuthParams): void;
}
