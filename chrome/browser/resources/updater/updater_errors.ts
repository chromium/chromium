// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {UpdaterError} from './event_history.js';
import {loadTimeData} from './i18n_setup.js';

export function getUpdaterErrorDescription(error: UpdaterError): string {
  switch (error.category) {
    case 1:
      return getDownloadErrorDescription(error.code);
    case 2:
      return getUnpackErrorDescription(error.code);
    case 3:
      return getInstallErrorDescription(error.code);
    case 4:
      return getServiceErrorDescription(error.code);
    case 5:
      return getUpdateCheckErrorDescription(error.code);
    case 7:
      return getInstallerErrorDescription(error.code);
    default:
      return loadTimeData.getString('updaterError-unknown');
  }
}

function getDownloadErrorDescription(code: number): string {
  switch (code) {
    case 0x80072EE2:  // ERROR_WINHTTP_TIMEOUT
    case 0x80072EE7:  // ERROR_WINHTTP_NAME_NOT_RESOLVED
    case 0x80072EFD:  // ERROR_WINHTTP_CANNOT_CONNECT
    case 0x80072EFE:  // ERROR_WINHTTP_CONNECTION_ABORTED
    case -1005:       // NSURLErrorNetworkConnectionLost
    case -1001:       // NSURLErrorTimedOut
    case -1004:       // NSURLErrorCannotConnectToHost
    case -1200:       // NSURLErrorSecureConnectionFailed
    case 503:
      return loadTimeData.getString('updaterError-network');
    case 12:
      return loadTimeData.getString('updaterError-corrupt');
    case 13:
      return loadTimeData.getString('updaterError-diskFull');
    case 403:
    case 407:
      return loadTimeData.getString('updaterError-accessDenied');
    default:
      return loadTimeData.getString('updaterError-download');
  }
}

function getUnpackErrorDescription(code: number): string {
  switch (code) {
    case 2:
    case 4:
      return loadTimeData.getString('updaterError-corrupt');
    default:
      return loadTimeData.getString('updaterError-unpack');
  }
}

function getInstallErrorDescription(code: number): string {
  switch (code) {
    case 0x80040901:  // GOOPDATEINSTALL_E_INSTALLER_FAILED_START
    case 103:
      return loadTimeData.getString('updaterError-installerFailed');
    default:
      return loadTimeData.getString('updaterError-install');
  }
}

function getServiceErrorDescription(code: number): string {
  switch (code) {
    case 2:
      return loadTimeData.getString('updaterError-disabled');
    default:
      return loadTimeData.getString('updaterError-unknown');
  }
}

function getInstallerErrorDescription(_code: number): string {
  return loadTimeData.getString('updaterError-installerFailed');
}

function getUpdateCheckErrorDescription(code: number): string {
  switch (code) {
    case -100:
    case -137:
      return loadTimeData.getString('updaterError-network');
    case -10007:
      return loadTimeData.getString('updaterError-restricted');
    default:
      return loadTimeData.getString('updaterError-updateCheck');
  }
}
