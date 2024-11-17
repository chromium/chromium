// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * This file contains overrides that are used by `cca.py dev` and `cca.py
 * bundle`. This file is type checked when building Chrome, but the output are
 * not included in built Chrome to save space. All larger override should be
 * placed here.
 */

import {assertNotReached} from './assert.js';
import {Point} from './geometry.js';
import * as localDev from './local_dev.js';
import {getCameraDirectory, getObjectURL} from './models/file_system.js';
import {ChromeHelper, getInstanceImpl} from './mojo/chrome_helper.js';
import {
  EventsSenderRemote,
  LidState,
  OcrResult,
  PdfBuilderRemote,
  ScreenState,
  StorageMonitorStatus,
  WifiConfig,
} from './mojo/type.js';
import {fakeEndpoint} from './mojo/util.js';
import {expandPath} from './util.js';

export class ChromeHelperFake extends ChromeHelper {
  // This class contains methods that overrides ChromeHelper, and async-ness
  // should follow the ChromeHelper class. We can manually write
  // Promise.resolve(...) instead but it's more verbose without much gain.
  /* eslint-disable @typescript-eslint/require-await */
  override async initTabletModeMonitor(_onChange: (isTablet: boolean) => void):
      Promise<boolean> {
    return false;
  }

  override async initScreenStateMonitor(
      _onChange: (state: ScreenState) => void): Promise<ScreenState> {
    return ScreenState.kOn;
  }

  override async initExternalScreenMonitor(
      _onChange: (hasExternalScreen: boolean) => void): Promise<boolean> {
    return false;
  }

  override async isTabletMode(): Promise<boolean> {
    return false;
  }

  override async initCameraWindowController(): Promise<void> {
    /* Do nothing. */
  }

  override startTracing(_event: string): void {
    /* Do nothing. */
  }

  override stopTracing(_event: string): void {
    /* Do nothing. */
  }

  override async openFileInGallery(name: string): Promise<void> {
    const cameraDir = getCameraDirectory();
    const file = await cameraDir.getFile(name);
    if (file === null) {
      return;
    }
    const objectUrl = await getObjectURL(file);
    const newTabWindow = window.open(objectUrl, '_blank');
    newTabWindow?.addEventListener('load', () => {
      // The unload handler is fired immediately since the window.open
      // triggered unload event on the initial empty page. See
      // https://stackoverflow.com/q/7476660
      newTabWindow?.addEventListener('unload', () => {
        URL.revokeObjectURL(objectUrl);
      });
    });
  }

  override openFeedbackDialog(_placeholder: string): void {
    alert('Feedback dialog.');
  }

  override openUrlInBrowser(url: string): void {
    window.open(url, '_blank', 'noopener,noreferrer');
  }

  override async finish(_intentId: number): Promise<void> {
    /* Do nothing. */
  }

  override async appendData(_intentId: number, _data: Uint8Array):
      Promise<void> {
    /* Do nothing. */
  }

  override async clearData(_intentId: number): Promise<void> {
    /* Do nothing. */
  }

  override async isMetricsAndCrashReportingEnabled(): Promise<boolean> {
    return false;
  }

  override sendNewCaptureBroadcast(_args: {isVideo: boolean, name: string}):
      void {
    /* Do nothing. */
  }

  override async monitorFileDeletion(_name: string, _callback: () => void):
      Promise<void> {
    /* Do nothing. */
  }

  override async isDocumentScannerSupported(): Promise<boolean> {
    return false;
  }

  override async checkDocumentModeReadiness(): Promise<boolean> {
    return false;
  }


  override async scanDocumentCorners(_blob: Blob): Promise<Point[]|null> {
    return null;
  }

  override async convertToDocument(
      _blob: Blob, _corners: Point[], _rotation: number): Promise<Blob> {
    assertNotReached();
  }

  override maybeTriggerSurvey(): void {
    /* Do nothing. */
  }

  override async startMonitorStorage(
      _onChange: (status: StorageMonitorStatus) => void):
      Promise<StorageMonitorStatus> {
    return StorageMonitorStatus.kNormal;
  }

  override stopMonitorStorage(): void {
    /* Do nothing. */
  }

  override openStorageManagement(): void {
    /* Do nothing. */
  }

  override openWifiDialog(_config: WifiConfig): void {
    /* Do nothing. */
  }

  override async initLidStateMonitor(_onChange: (lidStatus: LidState) => void):
      Promise<LidState> {
    return LidState.kNotPresent;
  }

  override async initSwPrivacySwitchMonitor(
      _onChange: (is_sw_privacy_switch_on: boolean) => void): Promise<boolean> {
    return false;
  }

  override async getEventsSender(): Promise<EventsSenderRemote> {
    return fakeEndpoint();
  }

  override async initScreenLockedMonitor(
      _onChange: (isScreenLocked: boolean) => void): Promise<boolean> {
    return false;
  }

  override async renderPdfAsImage(_pdf: Blob): Promise<Blob> {
    return new Blob();
  }

  override async performOcr(_jpeg: Blob): Promise<OcrResult> {
    return {lines: []};
  }

  override createPdfBuilder(): PdfBuilderRemote {
    assertNotReached();
  }
  /* eslint-enable @typescript-eslint/require-await */
}

localDev.setOverride(getInstanceImpl, () => new ChromeHelperFake());

// This file is included from /views/main.html, so remove the last two parts in
// URL to get the base path.
const basePath = window.location.pathname.split('/').slice(0, -2).join('/');
localDev.setOverride(expandPath, (path) => {
  return basePath + path;
});
