// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Minimal TypeScript definitions to satisfy cases where
 * authenticator.js is used from TypeScript files.
 */

import type {PasswordAttributes} from './saml_password_attributes.js';

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
  scrapedSAMLPasswords?: string[];
}

export interface AuthParams {
  authMode: AuthMode;
  clientId: string;
  clientVersion?: string;
  constrained: string;
  doSamlRedirect?: boolean;
  dontResizeNonEmbeddedPages: boolean;
  emailDomain: string;
  email: string;
  enableGaiaActionButtons: boolean;
  enterpriseEnrollmentDomain: string;
  extractSamlPasswordAttributes: boolean;
  flow: string;
  forceDarkMode: boolean;
  frameUrl: URL;
  gaiaPath: string;
  gaiaUrl: string;
  hl: string;
  ignoreCrOSIdpSetting: boolean;
  isDeviceOwner: boolean;
  isLoginPrimaryAccount: boolean;
  isSupervisedUser: boolean;
  needPassword?: boolean;
  platformVersion: string;
  readOnlyEmail: boolean;
  samlAclUrl: string;
  service: string;
  showTos: string;
  ssoProfile?: string;
  urlParameterToAutofillSAMLUsername: string;
  [key: string]: AuthParams[keyof AuthParams];
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

export const SUPPORTED_PARAMS: string[];

type ChangeEvent<T> = CustomEvent<{oldValue: T, newValue: T}>;

export type AuthCompletedEvent = CustomEvent<AuthCompletedCredentials>;
export type AuthDomainChangeEvent = ChangeEvent<string>;
export type AuthFlowChangeEvent = ChangeEvent<AuthFlow>;
export type LoadAbortEvent = CustomEvent<{error_code: number, src: string}>;

export class Authenticator extends EventTarget {
  constructor(webview: HTMLElement|string);
  getAccountsResponse(accounts: string[]): void;
  getDeviceIdResponse(deviceId: string): void;
  load(authMode: AuthMode, data: AuthParams): void;
  sendMessageToWebview(messageType: string, messageData?: string|Object): void;
  setWebviewPartition(newWebviewPartitionName: string): void;
  resetWebview(): void;
  resetStates(): void;
  reload(): void;

  insecureContentBlockedCallback: ((url: string) => void)|null;
  missingGaiaInfoCallback: (() => void)|null;
  samlApiUsedCallback: ((isThirdPartyIdP: boolean) => void)|null;
  recordSamlProviderCallback: ((x509Certificate: string) => void)|null;
}
