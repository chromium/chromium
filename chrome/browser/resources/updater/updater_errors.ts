// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {UpdaterError} from './event_history.js';
import {loadTimeData} from './i18n_setup.js';

export function getUpdaterErrorDescription(error: UpdaterError): string {
  switch (error.category) {
    case 1:
      return getDownloadErrorDescription(error.code) ??
          loadTimeData.getString('updaterError-download');
    case 2:
      return getUnpackErrorDescription(error.code) ??
          loadTimeData.getString('updaterError-unpack');
    case 3:
      return getInstallErrorDescription(error.code) ??
          loadTimeData.getString('updaterError-install');
    case 4:
      return getServiceErrorDescription(error.code) ??
          loadTimeData.getString('updaterError-unknown');
    case 5:
      return getUpdateCheckErrorDescription(error.code) ??
          loadTimeData.getString('updaterError-updateCheck');
    case 7:
      return loadTimeData.getString('updaterError-installerFailed');
    default:
      return loadTimeData.getString('updaterError-unknown');
  }
}

function getDownloadErrorDescription(code: number): string|undefined {
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
      return undefined;
  }
}

function getUnpackErrorDescription(code: number): string|undefined {
  switch (code) {
    case 2:
    case 4:
      return loadTimeData.getString('updaterError-corrupt');
    default:
      return undefined;
  }
}

function getInstallErrorDescription(code: number): string|undefined {
  switch (code) {
    case 0x80040901:  // GOOPDATEINSTALL_E_INSTALLER_FAILED_START
    case 103:
      return loadTimeData.getString('updaterError-installerFailed');
    default:
      return undefined;
  }
}

function getServiceErrorDescription(code: number): string|undefined {
  switch (code) {
    case 2:
      return loadTimeData.getString('updaterError-disabled');
    default:
      return undefined;
  }
}

function getUpdateCheckErrorDescription(code: number): string|undefined {
  switch (code) {
    case -100:
    case -137:
      return loadTimeData.getString('updaterError-network');
    case -10007:
      return loadTimeData.getString('updaterError-restricted');
    default:
      // Some downloader errors are presented in the updater error check
      // category if they occurred during the update check instead of the
      // download. If a more specific update check error could not be
      // determined, try interpreting the code as a download error.
      return getDownloadErrorDescription(code);
  }
}
