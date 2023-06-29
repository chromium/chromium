// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertEnumVariant} from '../../assert.js';
import * as barcodeChip from '../../barcode_chip.js';
import {CameraManager, CameraUI} from '../../device/index.js';
import * as dom from '../../dom.js';
import {sendBarcodeEnabledEvent} from '../../metrics.js';
import {BarcodeScanner} from '../../models/barcode.js';
import {ChromeHelper} from '../../mojo/chrome_helper.js';
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
export class ScanOptions implements CameraUI {
  private readonly scanOptions =
      [...dom.getAll('#scan-modes-group [data-scantype]', HTMLInputElement)];

  private video: PreviewVideo|null = null;

  /**
   * May be null if preview is not ready.
   */
  private barcodeScanner: BarcodeScanner|null = null;

  private readonly documentCornerOverlay: DocumentCornerOverlay;

  private readonly onChangeListeners = new Set<ScanOptionsChangeListener>();

  constructor(private readonly cameraManager: CameraManager) {
    this.cameraManager.registerCameraUI(this);

    this.documentCornerOverlay = new DocumentCornerOverlay(
        (p) => this.cameraManager.setPointOfInterest(p));

    // By default, the checked scan type is barcode unless the document mode is
    // ready.
    dom.get('#scan-barcode', HTMLInputElement).checked = true;

    (async () => {
      const {supported} =
          await ChromeHelper.getInstance().getDocumentScannerReadyState();
      dom.get('#scan-document-option', HTMLElement).hidden = !supported;
    })();

    for (const option of this.scanOptions) {
      option.addEventListener('click', (evt) => {
        if (state.get(state.State.CAMERA_CONFIGURING)) {
          evt.preventDefault();
        }
      });
      option.addEventListener('change', () => {
        if (option.checked) {
          this.switchToScanType(this.getToggledScanOption());
        }
      });
    }
  }

  async checkDocumentModeReadiness(): Promise<boolean> {
    const isLoaded =
        await ChromeHelper.getInstance().checkDocumentModeReadiness();
    if (isLoaded) {
      this.onDocumentModeReady();
    }
    return isLoaded;
  }

  onDocumentModeReady(): void {
    const docModeOption = dom.get('#scan-document-option', HTMLDivElement);
    docModeOption.classList.remove('disabled');

    const docBtn = dom.get('#scan-document', HTMLInputElement);
    docBtn.disabled = false;
    if (!state.get(Mode.SCAN)) {
      docBtn.checked = true;
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

    const video = this.cameraManager.getPreviewVideo();
    this.video = video;
    this.barcodeScanner = new BarcodeScanner(video.video, (value) => {
      barcodeChip.show(value);
    });
    const {deviceId} = video.getVideoSettings();
    this.documentCornerOverlay.attach(deviceId);
    const scanType = this.getToggledScanOption();
    (async () => {
      await video.onExpired.wait();
      this.detachPreview();
    })();
    await this.switchToScanType(scanType);
    this.checkDocumentModeReadiness();
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

  isDocumentModeEnabled(): boolean {
    return this.documentCornerOverlay.isEnabled();
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
    barcodeChip.dismiss();
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
}
