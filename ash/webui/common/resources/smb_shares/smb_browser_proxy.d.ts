// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export enum SmbMountResult {
  SUCCESS = 0,
  UNKNOWN_FAILURE = 1,
  AUTHENTICATION_FAILED = 2,
  NOT_FOUND = 3,
  UNSUPPORTED_DEVICE = 4,
  MOUNT_EXISTS = 5,
  INVALID_URL = 6,
  INVALID_OPERATION = 7,
  DBUS_PARSE_FAILED = 8,
  OUT_OF_MEMORY = 9,
  ABORTED = 10,
  IO_ERROR = 11,
  TOO_MANY_OPENED = 12,
  INVALID_SSO_URL = 13,
  INVALID_USERNAME = 14,
}

export enum SmbAuthMethod {
  KERBEROS = 'kerberos',
  CREDENTIALS = 'credentials',
}

export interface SmbBrowserProxy {
  smbMount(
      smbUrl: string, smbName: string, username: string, password: string,
      authMethod: string, shouldOpenFileManagerAfterMount: boolean,
      saveCredentials: boolean): Promise<SmbMountResult>;
  startDiscovery(): void;
  updateCredentials(mountId: string, username: string, password: string): void;
  hasAnySmbMountedBefore(): Promise<boolean>;
}

declare class SmbBrowserProxyImpl implements SmbBrowserProxy {
  static getInstance(): SmbBrowserProxy;
  static setInstance(instance: SmbBrowserProxy): void;
  smbMount(
      smbUrl: string, smbName: string, username: string, password: string,
      authMethod: string, shouldOpenFileManagerAfterMount: boolean,
      saveCredentials: boolean): Promise<SmbMountResult>;
  startDiscovery(): void;
  updateCredentials(mountId: string, username: string, password: string): void;
  hasAnySmbMountedBefore(): Promise<boolean>;
}

export {SmbBrowserProxyImpl};
