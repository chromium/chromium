// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '../assert.js';
import {reportError} from '../error.js';
import {Point} from '../geometry.js';
import * as localDev from '../local_dev.js';
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
  ToteMetricFormat,
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
  // This cast is to workaround that the generated mojo binding only accepts
  // number[], but actually can be passed Uint8Array (which also supports
  // indexing via [] and length).
  // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
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

export abstract class ChromeHelper {
  /**
   * Starts monitoring tablet mode state of device.
   *
   * @param onChange Callback called each time when tablet mode state of device
   *     changes with boolean parameter indicating whether device is entering
   *     tablet mode.
   * @return Resolved to initial tablet mode state of device.
   */
  abstract initTabletModeMonitor(onChange: (isTablet: boolean) => void):
      Promise<boolean>;

  /**
   * Starts monitoring screen state of device.
   *
   * @param onChange Callback called each time when screen state of device
   *     changes with parameter of newly changed value.
   * @return Resolved to initial screen state of device.
   */
  abstract initScreenStateMonitor(onChange: (state: ScreenState) => void):
      Promise<ScreenState>;

  /**
   * Starts monitoring the existence of external screens.
   *
   * @param onChange Callback called each time when the existence of external
   *     screens changes.
   * @return Resolved to the initial state of external screens existence.
   */
  abstract initExternalScreenMonitor(
      onChange: (hasExternalScreen: boolean) => void): Promise<boolean>;

  abstract isTabletMode(): Promise<boolean>;

  /**
   * Initializes the camera window controller and bootstraps the mojo
   * communication to get window states.
   */
  abstract initCameraWindowController(): Promise<void>;

  /**
   * Starts event tracing of `event` in Chrome.
   */
  abstract startTracing(event: string): void;

  /**
   * Stops event tracing of `event` in Chrome.
   */
  abstract stopTracing(event: string): void;

  /**
   * Opens the file in Downloads folder by its `name` in gallery.
   */
  abstract openFileInGallery(name: string): void;

  /**
   * Opens the chrome feedback dialog.
   *
   * @param placeholder The text of the placeholder in the description
   *     field.
   */
  abstract openFeedbackDialog(placeholder: string): void;

  /**
   * Opens the given `url` in the browser.
   */
  abstract openUrlInBrowser(url: string): void;

  /**
   * Notifies ARC++ to finish the intent of given `intendId`.
   */
  abstract finish(intentId: number): Promise<void>;

  /**
   * Notifies ARC++ to append `data` to intent of the given `intentId`.
   *
   * @param intentId Intent id of the intent which `data` appends to.
   * @param data The data to be appended to intent result.
   */
  abstract appendData(intentId: number, data: Uint8Array): Promise<void>;

  /**
   * Notifies ARC++ to clear appended intent result data.
   *
   * @param intentId Intent id of the intent whose results to be cleared.
   */
  abstract clearData(intentId: number): Promise<void>;

  /**
   * Returns if the logging consent option is enabled.
   */
  abstract isMetricsAndCrashReportingEnabled(): Promise<boolean>;

  /**
   * Sends the broadcast to ARC to notify the new photo/video is captured.
   */
  abstract sendNewCaptureBroadcast(args: {isVideo: boolean, name: string}):
      void;

  /**
   * Notifies Tote client when a photo/pdf/video/gif is captured.
   */
  abstract notifyTote(format: ToteMetricFormat, name: string): void;

  /**
   * Monitors for the file deletion of the file given by its `name` and
   * triggers `callback` when the file is deleted. Note that a previous
   * monitor request will be canceled once another monitor request is sent.
   *
   * @return Resolved when the file is deleted or the current monitor is
   *     canceled by future monitor call.
   * @throws When error occurs during monitor.
   */
  abstract monitorFileDeletion(name: string, callback: () => void):
      Promise<void>;

  abstract getDocumentScannerReadyState():
      Promise<{supported: boolean, ready: boolean}>;

  /**
   * Checks the document mode readiness. Returns false if it fails to load.
   */
  abstract checkDocumentModeReadiness(): Promise<boolean>;

  /**
   * Scans the `blob` data and returns the detected document corners.
   *
   * @return Promise resolve to positions of document corner. Null for failing
   *     to detected corner positions.
   */
  abstract scanDocumentCorners(blob: Blob): Promise<Point[]|null>;

  /**
   * Converts the blob to document given by its `blob` data, `rotation` and
   * target `corners` to crop. The output will be converted according to given
   * `mimeType`.
   */
  abstract convertToDocument(
      blob: Blob, corners: Point[], rotation: number,
      mimeType: MimeType): Promise<Blob>;

  /**
   * Converts given `jpegBlobs` to PDF format.
   *
   * @return Blob in PDF format.
   */
  abstract convertToPdf(jpegBlobs: Blob[]): Promise<Blob>;

  /**
   * Tries to trigger HaTS survey for CCA.
   */
  abstract maybeTriggerSurvey(): void;

  abstract startMonitorStorage(
      onChange: (status: StorageMonitorStatus) => void):
      Promise<StorageMonitorStatus>;

  abstract stopMonitorStorage(): void;

  abstract openStorageManagement(): void;

  /**
   * Creates a new instance of ChromeHelper if it is not set. Returns the
   *     existing instance.
   *
   * @return The singleton instance.
   */
  static getInstance(): ChromeHelper {
    if (instance === null) {
      instance = getInstanceImpl();
    }
    return instance;
  }
}

export const getInstanceImpl =
    localDev.overridableFunction((): ChromeHelper => new ChromeHelperImpl());

/**
 * Communicates with Chrome.
 */
class ChromeHelperImpl extends ChromeHelper {
  /**
   * An interface remote that is used to communicate with Chrome.
   */
  private readonly remote: CameraAppHelperRemote =
      wrapEndpoint(CameraAppHelper.getRemote());

  override async initTabletModeMonitor(onChange: (isTablet: boolean) => void):
      Promise<boolean> {
    const monitorCallbackRouter =
        wrapEndpoint(new TabletModeMonitorCallbackRouter());
    monitorCallbackRouter.update.addListener(onChange);

    const {isTabletMode} = await this.remote.setTabletMonitor(
        monitorCallbackRouter.$.bindNewPipeAndPassRemote());
    return isTabletMode;
  }

  override async initScreenStateMonitor(onChange: (state: ScreenState) => void):
      Promise<ScreenState> {
    const monitorCallbackRouter =
        wrapEndpoint(new ScreenStateMonitorCallbackRouter());
    monitorCallbackRouter.update.addListener(onChange);

    const {initialState} = await this.remote.setScreenStateMonitor(
        monitorCallbackRouter.$.bindNewPipeAndPassRemote());
    return initialState;
  }

  override async initExternalScreenMonitor(
      onChange: (hasExternalScreen: boolean) => void): Promise<boolean> {
    const monitorCallbackRouter =
        wrapEndpoint(new ExternalScreenMonitorCallbackRouter());
    monitorCallbackRouter.update.addListener(onChange);

    const {hasExternalScreen} = await this.remote.setExternalScreenMonitor(
        monitorCallbackRouter.$.bindNewPipeAndPassRemote());
    return hasExternalScreen;
  }

  override async isTabletMode(): Promise<boolean> {
    const {isTabletMode} = await this.remote.isTabletMode();
    return isTabletMode;
  }

  override async initCameraWindowController(): Promise<void> {
    let {controller} = await this.remote.getWindowStateController();
    controller = wrapEndpoint(controller);
    await windowController.bind(controller);
  }

  override startTracing(event: string): void {
    this.remote.startPerfEventTrace(event);
  }

  override stopTracing(event: string): void {
    this.remote.stopPerfEventTrace(event);
  }

  override openFileInGallery(name: string): void {
    this.remote.openFileInGallery(name);
  }

  override openFeedbackDialog(placeholder: string): void {
    this.remote.openFeedbackDialog(placeholder);
  }

  override openUrlInBrowser(url: string): void {
    this.remote.openUrlInBrowser({url: url});
  }

  private async checkReturn(
      caller: string, value: Promise<{isSuccess: boolean}>): Promise<void> {
    const {isSuccess} = await value;
    if (!isSuccess) {
      reportError(
          ErrorType.HANDLE_CAMERA_RESULT_FAILURE, ErrorLevel.ERROR,
          new Error(`Return not isSuccess from calling intent ${caller}.`));
    }
  }

  override async finish(intentId: number): Promise<void> {
    const ret =
        this.remote.handleCameraResult(intentId, CameraIntentAction.FINISH, []);
    await this.checkReturn('finish()', ret);
  }

  override async appendData(intentId: number, data: Uint8Array): Promise<void> {
    const ret = this.remote.handleCameraResult(
        intentId, CameraIntentAction.APPEND_DATA, castToNumberArray(data));
    await this.checkReturn('appendData()', ret);
  }

  override async clearData(intentId: number): Promise<void> {
    const ret = this.remote.handleCameraResult(
        intentId, CameraIntentAction.CLEAR_DATA, []);
    await this.checkReturn('clearData()', ret);
  }

  override async isMetricsAndCrashReportingEnabled(): Promise<boolean> {
    const {isEnabled} = await this.remote.isMetricsAndCrashReportingEnabled();
    return isEnabled;
  }

  override sendNewCaptureBroadcast({isVideo, name}:
                                       {isVideo: boolean, name: string}): void {
    this.remote.sendNewCaptureBroadcast(isVideo, name);
  }

  override notifyTote(format: ToteMetricFormat, name: string): void {
    this.remote.notifyTote(format, name);
  }

  override async monitorFileDeletion(name: string, callback: () => void):
      Promise<void> {
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

  override async getDocumentScannerReadyState():
      Promise<{supported: boolean, ready: boolean}> {
    const {readyState} = await this.remote.getDocumentScannerReadyState();
    return {
      supported: readyState !== DocumentScannerReadyState.NOT_SUPPORTED,
      ready: readyState === DocumentScannerReadyState.SUPPORTED_AND_READY,
    };
  }

  override async checkDocumentModeReadiness(): Promise<boolean> {
    const {isLoaded} = await this.remote.checkDocumentModeReadiness();
    return isLoaded;
  }

  override async scanDocumentCorners(blob: Blob): Promise<Point[]|null> {
    const buffer = new Uint8Array(await blob.arrayBuffer());

    const {corners} =
        await this.remote.scanDocumentCorners(castToNumberArray(buffer));
    if (corners.length === 0) {
      return null;
    }
    return corners.map(({x, y}) => new Point(x, y));
  }

  override async convertToDocument(
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

  override async convertToPdf(jpegBlobs: Blob[]): Promise<Blob> {
    const numArrays = await Promise.all(jpegBlobs.map(async (blob) => {
      const buffer = new Uint8Array(await blob.arrayBuffer());
      return castToNumberArray(buffer);
    }));
    const {pdfData} = await this.remote.convertToPdf(numArrays);
    return new Blob([new Uint8Array(pdfData)], {type: MimeType.PDF});
  }

  override maybeTriggerSurvey(): void {
    this.remote.maybeTriggerSurvey();
  }

  override async startMonitorStorage(
      onChange: (status: StorageMonitorStatus) => void):
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

  override stopMonitorStorage(): void {
    this.remote.stopStorageMonitor();
  }

  override openStorageManagement(): void {
    this.remote.openStorageManagement();
  }
}
