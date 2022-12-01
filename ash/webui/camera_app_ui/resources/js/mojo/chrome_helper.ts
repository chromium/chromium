// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '../assert.js';
import {reportError} from '../error.js';
import {Point} from '../geometry.js';
import {
  ErrorLevel,
  ErrorType,
  MimeType,
} from '../type.js';
import {windowController} from '../window_controller.js';

import {
  CameraAppHelper,
  CameraAppHelperRemote,
  CameraIntentAction,
  CameraUsageOwnershipMonitorCallbackRouter,
  DocumentOutputFormat,
  DocumentScannerReadyState,
  ExternalScreenMonitorCallbackRouter,
  FileMonitorResult,
  Rotation,
  ScreenState,
  ScreenStateMonitorCallbackRouter,
  StorageMonitorCallbackRouter,
  StorageMonitorStatus,
  TabletModeMonitorCallbackRouter,
} from './type.js';
import {wrapEndpoint} from './util.js';

/**
 * The singleton instance of ChromeHelper. Initialized by the first
 * invocation of getInstance().
 */
let instance: ChromeHelper|null = null;

/**
 * Forces casting type from Uint8Array to number[].
 */
function castToNumberArray(data: Uint8Array): number[] {
  return data as unknown as number[];
}

/**
 * Casts from rotation degrees to mojo rotation.
 */
function castToMojoRotation(rotation: number): Rotation {
  switch (rotation) {
    case 0:
      return Rotation.ROTATION_0;
    case 90:
      return Rotation.ROTATION_90;
    case 180:
      return Rotation.ROTATION_180;
    case 270:
      return Rotation.ROTATION_270;
    default:
      assertNotReached(`Invalid rotation ${rotation}`);
  }
}

/**
 * Communicates with Chrome.
 */
export class ChromeHelper {
  /**
   * An interface remote that is used to communicate with Chrome.
   */
  private readonly remote: CameraAppHelperRemote =
      wrapEndpoint(CameraAppHelper.getRemote());

  /**
   * Starts tablet mode monitor monitoring tablet mode state of device.
   *
   * @param onChange Callback called each time when tablet mode state of device
   *     changes with boolean parameter indicating whether device is entering
   *     tablet mode.
   * @return Resolved to initial state of whether device is is in tablet mode.
   */
  async initTabletModeMonitor(onChange: (isTablet: boolean) => void):
      Promise<boolean> {
    const monitorCallbackRouter =
        wrapEndpoint(new TabletModeMonitorCallbackRouter());
    monitorCallbackRouter.update.addListener(onChange);

    const {isTabletMode} = await this.remote.setTabletMonitor(
        monitorCallbackRouter.$.bindNewPipeAndPassRemote());
    return isTabletMode;
  }

  /**
   * Starts monitor monitoring system screen state of device.
   *
   * @param onChange Callback called each time when device screen state changes
   *     with parameter of newly changed value.
   * @return Resolved to initial system screen state.
   */
  async initScreenStateMonitor(onChange: (state: ScreenState) => void):
      Promise<ScreenState> {
    const monitorCallbackRouter =
        wrapEndpoint(new ScreenStateMonitorCallbackRouter());
    monitorCallbackRouter.update.addListener(onChange);

    const {initialState} = await this.remote.setScreenStateMonitor(
        monitorCallbackRouter.$.bindNewPipeAndPassRemote());
    return initialState;
  }

  /**
   * Starts monitor monitoring the existence of external screens.
   *
   * @param onChange Callback called when the existence of external screens
   *     changes.
   * @return Resolved to the initial state.
   */
  async initExternalScreenMonitor(
      onChange: (hasExternalScreen: boolean) => void): Promise<boolean> {
    const monitorCallbackRouter =
        wrapEndpoint(new ExternalScreenMonitorCallbackRouter());
    monitorCallbackRouter.update.addListener(onChange);

    const {hasExternalScreen} = await this.remote.setExternalScreenMonitor(
        monitorCallbackRouter.$.bindNewPipeAndPassRemote());
    return hasExternalScreen;
  }

  /**
   * Checks if the device is under tablet mode currently.
   */
  async isTabletMode(): Promise<boolean> {
    const {isTabletMode} = await this.remote.isTabletMode();
    return isTabletMode;
  }

  /**
   * Starts camera usage monitor.
   */
  async initCameraUsageMonitor(
      exploitUsage: () => Promise<void>,
      releaseUsage: () => Promise<void>): Promise<void> {
    const usageCallbackRouter =
        wrapEndpoint(new CameraUsageOwnershipMonitorCallbackRouter());

    usageCallbackRouter.onCameraUsageOwnershipChanged.addListener(
        async (hasUsage: boolean) => {
          if (hasUsage) {
            await exploitUsage();
          } else {
            await releaseUsage();
          }
        });

    const {isSuccess} = await this.remote.setCameraUsageMonitor(
        usageCallbackRouter.$.bindNewPipeAndPassRemote());
    if (!isSuccess) {
      throw new Error('Failed to set camera usage monitor');
    }

    let {controller} = await this.remote.getWindowStateController();
    controller = wrapEndpoint(controller);
    await windowController.bind(controller);
  }

  /**
   * Triggers the begin of event tracing in Chrome.
   *
   * @param event Name of the event.
   */
  startTracing(event: string): void {
    this.remote.startPerfEventTrace(event);
  }

  /**
   * Triggers the end of event tracing in Chrome.
   *
   * @param event Name of the event.
   */
  stopTracing(event: string): void {
    this.remote.stopPerfEventTrace(event);
  }

  /**
   * Opens the file in Downloads folder by its |name| in gallery.
   *
   * @param name Name of the target file.
   */
  openFileInGallery(name: string): void {
    this.remote.openFileInGallery(name);
  }

  /**
   * Opens the chrome feedback dialog.
   *
   * @param placeholder The text of the placeholder in the description
   *     field.
   */
  openFeedbackDialog(placeholder: string): void {
    this.remote.openFeedbackDialog(placeholder);
  }

  /**
   * Opens the given URL in the browser.
   *
   * @param url The URL to open.
   */
  openUrlInBrowser(url: string): void {
    this.remote.openUrlInBrowser({url: url});
  }

  /**
   * Checks return value from |handleCameraResult|.
   *
   * @param caller Caller identifier.
   */
  private async checkReturn(
      caller: string, value: Promise<{isSuccess: boolean}>): Promise<void> {
    const {isSuccess} = await value;
    if (!isSuccess) {
      reportError(
          ErrorType.HANDLE_CAMERA_RESULT_FAILURE, ErrorLevel.ERROR,
          new Error(`Return not isSuccess from calling intent ${caller}.`));
    }
  }

  /**
   * Notifies ARC++ to finish the intent.
   *
   * @param intentId Intent id of the intent to be finished.
   */
  async finish(intentId: number): Promise<void> {
    const ret =
        this.remote.handleCameraResult(intentId, CameraIntentAction.FINISH, []);
    await this.checkReturn('finish()', ret);
  }

  /**
   * Notifies ARC++ to append data to intent result.
   *
   * @param intentId Intent id of the intent to be appended data to.
   * @param data The data to be appended to intent result.
   */
  async appendData(intentId: number, data: Uint8Array): Promise<void> {
    const ret = this.remote.handleCameraResult(
        intentId, CameraIntentAction.APPEND_DATA, castToNumberArray(data));
    await this.checkReturn('appendData()', ret);
  }

  /**
   * Notifies ARC++ to clear appended intent result data.
   *
   * @param intentId Intent id of the intent to be cleared its result.
   */
  async clearData(intentId: number): Promise<void> {
    const ret = this.remote.handleCameraResult(
        intentId, CameraIntentAction.CLEAR_DATA, []);
    await this.checkReturn('clearData()', ret);
  }

  /**
   * Checks if the logging consent option is enabled.
   */
  async isMetricsAndCrashReportingEnabled(): Promise<boolean> {
    const {isEnabled} = await this.remote.isMetricsAndCrashReportingEnabled();
    return isEnabled;
  }

  /**
   * Sends the broadcast to ARC to notify the new photo/video is captured.
   */
  async sendNewCaptureBroadcast(
      {isVideo, name}: {isVideo: boolean, name: string}): Promise<void> {
    this.remote.sendNewCaptureBroadcast(isVideo, name);
  }

  /**
   * Monitors for the file deletion of the file given by its |name| and triggers
   * |callback| when the file is deleted. Note that a previous monitor request
   * will be canceled once another monitor request is sent.
   *
   * @param name The name of the file to monitor.
   * @param callback Function to trigger when deletion.
   * @return Resolved when the file is deleted or the current monitor is
   *     canceled by future monitor call.
   * @throws When error occurs during monitor.
   */
  async monitorFileDeletion(name: string, callback: () => void): Promise<void> {
    const {result} = await this.remote.monitorFileDeletion(name);
    switch (result) {
      case FileMonitorResult.DELETED:
        callback();
        return;
      case FileMonitorResult.CANCELED:
        // Do nothing if it is canceled by another monitor call.
        return;
      case FileMonitorResult.ERROR:
        throw new Error('Error happens when monitoring file deletion');
      default:
        assertNotReached();
    }
  }

  /**
   * Gets the ready state of the document scanner.
   */
  async getDocumentScannerReadyState():
      Promise<{supported: boolean, ready: boolean}> {
    const {readyState} = await this.remote.getDocumentScannerReadyState();
    return {
      supported: readyState !== DocumentScannerReadyState.NOT_SUPPORTED,
      ready: readyState === DocumentScannerReadyState.SUPPORTED_AND_READY,
    };
  }

  /**
   * Checks the document mode readiness. Returns false if it fails to load.
   */
  async checkDocumentModeReadiness(): Promise<boolean> {
    const {isLoaded} = await this.remote.checkDocumentModeReadiness();
    return isLoaded;
  }

  /**
   * Scans the blob data and returns the detected document corners.
   *
   * @return Promise resolve to positions of document corner. Null for failing
   *     to detected corner positions.
   */
  async scanDocumentCorners(blob: Blob): Promise<Point[]|null> {
    const buffer = new Uint8Array(await blob.arrayBuffer());

    const {corners} =
        await this.remote.scanDocumentCorners(castToNumberArray(buffer));
    if (corners.length === 0) {
      return null;
    }
    return corners.map(({x, y}) => new Point(x, y));
  }

  /**
   * Converts the blob to document given by its |blob| data, |resolution| and
   * target |corners| to crop. The output will be converted according to given
   * |mimeType|.
   */
  async convertToDocument(
      blob: Blob, corners: Point[], rotation: number,
      mimeType: MimeType): Promise<Blob> {
    assert(corners.length === 4, 'Unexpected amount of corners');
    const buffer = new Uint8Array(await blob.arrayBuffer());
    let outputFormat;
    if (mimeType === MimeType.JPEG) {
      outputFormat = DocumentOutputFormat.JPEG;
    } else if (mimeType === MimeType.PDF) {
      outputFormat = DocumentOutputFormat.PDF;
    } else {
      throw new Error(`Output mimetype unsupported: ${mimeType}`);
    }

    const {docData} = await this.remote.convertToDocument(
        castToNumberArray(buffer), corners, castToMojoRotation(rotation),
        outputFormat);
    return new Blob([new Uint8Array(docData)], {type: mimeType});
  }

  /**
   * Converts given |jpegBlobs| to PDF format.
   *
   * @param jpegBlobs Blobs in JPEG format.
   * @return Blob in PDF format.
   */
  async convertToPdf(jpegBlobs: Blob[]): Promise<Blob> {
    const numArrays = await Promise.all(jpegBlobs.map(async (blob) => {
      const buffer = new Uint8Array(await blob.arrayBuffer());
      return castToNumberArray(buffer);
    }));
    const {pdfData} = await this.remote.convertToPdf(numArrays);
    return new Blob([new Uint8Array(pdfData)], {type: MimeType.PDF});
  }

  /**
   * Tries to trigger HaTS survey for CCA.
   */
  maybeTriggerSurvey(): void {
    this.remote.maybeTriggerSurvey();
  }

  async startMonitorStorage(onChange: (status: StorageMonitorStatus) => void):
      Promise<StorageMonitorStatus> {
    const storageCallbackRouter =
        wrapEndpoint(new StorageMonitorCallbackRouter());
    storageCallbackRouter.update.addListener(
        (newStatus: StorageMonitorStatus) => {
          if (newStatus === StorageMonitorStatus.ERROR) {
            throw new Error('Error occurred while monitoring storage.');
          } else if (newStatus !== StorageMonitorStatus.CANCELED) {
            onChange(newStatus);
          }
        });

    const {initialStatus} = await this.remote.startStorageMonitor(
        storageCallbackRouter.$.bindNewPipeAndPassRemote());
    // Should not get canceled status at initial time.
    if (initialStatus === StorageMonitorStatus.ERROR ||
        initialStatus === StorageMonitorStatus.CANCELED) {
      throw new Error('Failed to start storage monitoring.');
    }
    return initialStatus;
  }

  stopMonitorStorage(): void {
    this.remote.stopStorageMonitor();
  }

  openStorageManagement(): void {
    this.remote.openStorageManagement();
  }

  /**
   * Creates a new instance of ChromeHelper if it is not set. Returns the
   *     exist instance.
   *
   * @return The singleton instance.
   */
  static getInstance(): ChromeHelper {
    if (instance === null) {
      instance = new ChromeHelper();
    }
    return instance;
  }
}
