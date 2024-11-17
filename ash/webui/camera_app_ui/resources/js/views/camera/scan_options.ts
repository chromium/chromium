// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertEnumVariant} from '../../assert.js';
import {queuedAsyncCallback} from '../../async_job_queue.js';
import {CameraManager, CameraUi} from '../../device/index.js';
import * as dom from '../../dom.js';
import {sendBarcodeEnabledEvent} from '../../metrics.js';
import {BarcodeScanner} from '../../models/barcode.js';
import {ChromeHelper} from '../../mojo/chrome_helper.js';
import * as scannerChip from '../../scanner_chip.js';
import * as state from '../../state.js';
import {Mode, PreviewVideo} from '../../type.js';

import {DocumentCornerOverlay} from './document_corner_overlay.js';

enum ScanType {
  BARCODE = 'barcode',
  DOCUMENT = 'document',
}

/**
 * Gets the scan type from element's data-scantype.
 */
function getScanTypeFromElement(el: HTMLInputElement): ScanType {
  return assertEnumVariant(ScanType, el.dataset['scantype']);
}

/**
 * Gets HTMLInputElements that has the given scan type.
 */
function getElementFromScanType(type: ScanType): HTMLInputElement {
  return dom.get(`input[data-scantype=${type}]`, HTMLInputElement);
}

type ScanOptionsChangeListener = () => void;

/**
 * Controller for the scan options of Camera view.
 */
export class ScanOptions implements CameraUi {
  private readonly scanOptions =
      [...dom.getAll('#scan-modes-group [data-scantype]', HTMLInputElement)];

  private video: PreviewVideo|null = null;

  /**
   * May be null if preview is not ready.
   */
  private barcodeScanner: BarcodeScanner|null = null;

  private readonly documentCornerOverlay: DocumentCornerOverlay;

  private readonly onChangeListeners = new Set<ScanOptionsChangeListener>();

  private readonly updateDocumentModeStatus =
      queuedAsyncCallback('keepLatest', async () => {
        await this.checkDocumentModeReadiness();
      });

  private readonly documentModeOptionWrapper =
      dom.get('#scan-document-option', HTMLDivElement);

  constructor(private readonly cameraManager: CameraManager) {
    this.cameraManager.registerCameraUi(this);

    this.documentCornerOverlay = new DocumentCornerOverlay(
        (p) => this.cameraManager.setPointOfInterest(p));

    // By default, the checked scan type is barcode unless the document mode is
    // ready.
    dom.get('#scan-barcode', HTMLInputElement).checked = true;

    // TODO(pihsun): Move this outside of the constructor.
    void (async () => {
      const supported =
          await ChromeHelper.getInstance().isDocumentScannerSupported();
      this.documentModeOptionWrapper.hidden = !supported;
    })();

    for (const option of this.scanOptions) {
      option.addEventListener('click', (evt) => {
        if (state.get(state.State.CAMERA_CONFIGURING)) {
          evt.preventDefault();
        }
      });
      option.addEventListener('change', async () => {
        if (option.checked) {
          await this.switchToScanType(this.getToggledScanOption());
        }
      });
    }
  }

  async checkDocumentModeReadiness(): Promise<void> {
    const isLoaded =
        await ChromeHelper.getInstance().checkDocumentModeReadiness();
    if (isLoaded) {
      this.onDocumentModeReady();
    }
  }

  onDocumentModeReady(): void {
    if (this.documentModeEnabled()) {
      return;
    }
    this.documentModeOptionWrapper.classList.remove('disabled');
    const inputElement = getElementFromScanType(ScanType.DOCUMENT);
    inputElement.disabled = false;
    // Avoid UI jump when in Scan mode. `this.switchToScanType()` isn't used
    // because we only want to set the default option instead of setting up the
    // mode.
    if (!state.get(Mode.SCAN)) {
      inputElement.checked = true;
    }
  }

  /**
   * Adds a listener for scan options change.
   */
  addOnChangeListener(listener: ScanOptionsChangeListener): void {
    this.onChangeListeners.add(listener);
  }

  /**
   * Whether preview is attached to scan frame source.
   */
  private previewAvailable(): boolean {
    return this.video?.isExpired() === false;
  }

  // Overrides |CameraUI|.
  async onUpdateConfig(): Promise<void> {
    assert(!this.previewAvailable());
    if (state.get(Mode.SCAN)) {
      const video = this.cameraManager.getPreviewVideo();
      this.video = video;
      this.barcodeScanner = new BarcodeScanner(video.video, (value) => {
        scannerChip.showBarcodeContent(value);
      });
      const {deviceId} = video.getVideoSettings();
      this.documentCornerOverlay.attach(deviceId);
      const scanType = this.getToggledScanOption();
      // Not awaiting here since this is for teardown after preview video
      // expires.
      void (async () => {
        await video.onExpired.wait();
        this.detachPreview();
      })();
      await this.switchToScanType(scanType);
    }
    if (!this.documentModeEnabled()) {
      this.updateDocumentModeStatus();
    }
  }

  /**
   * @return Returns scan type of checked radio buttons in scan type option
   *     groups.
   */
  private getToggledScanOption(): ScanType {
    const checkedEl = this.scanOptions.find(({checked}) => checked);
    assert(checkedEl !== undefined);
    return getScanTypeFromElement(checkedEl);
  }

  /**
   * Updates the option UI and starts or stops the corresponding scanner
   * according to given |scanType|.
   */
  private async switchToScanType(scanType: ScanType) {
    if (!this.previewAvailable()) {
      return;
    }
    assert(this.barcodeScanner !== null);

    getElementFromScanType(scanType).checked = true;
    if (state.get(Mode.SCAN) && scanType === ScanType.BARCODE) {
      sendBarcodeEnabledEvent();
      this.barcodeScanner.start();
      state.set(state.State.ENABLE_SCAN_BARCODE, true);
    } else {
      this.stopBarcodeScanner();
    }

    if (state.get(Mode.SCAN) && scanType === ScanType.DOCUMENT) {
      await this.documentCornerOverlay.start();
      state.set(state.State.ENABLE_SCAN_DOCUMENT, true);
    } else {
      this.documentCornerOverlay.stop();
    }

    for (const listener of this.onChangeListeners) {
      listener();
    }
  }

  private stopBarcodeScanner() {
    assert(this.barcodeScanner !== null);
    this.barcodeScanner.stop();
    scannerChip.dismiss();
    state.set(state.State.ENABLE_SCAN_BARCODE, false);
  }

  /**
   * Stops all scanner and detaches from current preview.
   */
  private detachPreview(): void {
    if (this.barcodeScanner !== null) {
      this.stopBarcodeScanner();
      this.barcodeScanner = null;
    }
    this.documentCornerOverlay.detach();
  }

  private documentModeEnabled(): boolean {
    const disabled =
        this.documentModeOptionWrapper.classList.contains('disabled');
    return !disabled;
  }
}
