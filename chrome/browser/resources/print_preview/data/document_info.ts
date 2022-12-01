// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Coordinate2d} from './coordinate2d.js';
import {CustomMarginsOrientation, Margins} from './margins.js';
import {PrintableArea} from './printable_area.js';
import {Size} from './size.js';

export interface DocumentSettings {
  hasCssMediaStyles: boolean;
  hasSelection: boolean;
  isModifiable: boolean;
  isFromArc: boolean;
  isScalingDisabled: boolean;
  fitToPageScaling: number;
  pageCount: number;
  title: string;
}

export interface PageLayoutInfo {
  marginTop: number;
  marginLeft: number;
  marginBottom: number;
  marginRight: number;
  contentWidth: number;
  contentHeight: number;
  printableAreaX: number;
  printableAreaY: number;
  printableAreaWidth: number;
  printableAreaHeight: number;
}

const PrintPreviewDocumentInfoElementBase = WebUiListenerMixin(PolymerElement);

export class PrintPreviewDocumentInfoElement extends
    PrintPreviewDocumentInfoElementBase {
  static get is() {
    return 'print-preview-document-info';
  }

  static get properties() {
    return {
      documentSettings: {
        type: Object,
        notify: true,
        value() {
          return {
            hasCssMediaStyles: false,
            hasSelection: false,
            isModifiable: true,
            isFromArc: false,
            isScalingDisabled: false,
            fitToPageScaling: 100,
            pageCount: 0,
            title: '',
          };
        },
      },

      inFlightRequestId: {
        type: Number,
        value: -1,
      },

      margins: {
        type: Object,
        notify: true,
      },

      /**
       * Size of the pages of the document in points. Actual page-related
       * information won't be set until preview generation occurs, so use
       * a default value until then.
       */
      pageSize: {
        type: Object,
        notify: true,
        value() {
          return new Size(612, 792);
        },
      },

      /**
       * Printable area of the document in points.
       */
      printableArea: {
        type: Object,
        notify: true,
        value() {
          return new PrintableArea(new Coordinate2d(0, 0), new Size(612, 792));
        },
      },
    };
  }

  documentSettings: DocumentSettings;
  inFlightRequestId: number;
  margins: Margins;
  pageSize: Size;
  printableArea: PrintableArea;
  private isInitialized_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'page-count-ready',
        (pageCount: number, previewResponseId: number, scaling: number) =>
            this.onPageCountReady_(pageCount, previewResponseId, scaling));
    this.addWebUiListener(
        'page-layout-ready',
        (pageLayout: PageLayoutInfo, hasCustomPageSizeStyle: boolean) =>
            this.onPageLayoutReady_(pageLayout, hasCustomPageSizeStyle));
  }

  /**
   * Initializes the state of the data model.
   */
  init(
      isModifiable: boolean, isFromArc: boolean, title: string,
      hasSelection: boolean) {
    this.isInitialized_ = true;
    this.set('documentSettings.isModifiable', isModifiable);
    this.set('documentSettings.isFromArc', isFromArc);
    this.set('documentSettings.title', title);
    this.set('documentSettings.hasSelection', hasSelection);
  }

  /**
   * Updates whether scaling is disabled for the document.
   */
  updateIsScalingDisabled(isScalingDisabled: boolean) {
    if (this.isInitialized_) {
      this.set('documentSettings.isScalingDisabled', isScalingDisabled);
    }
  }

  /**
   * Called when the page layout of the document is ready. Always occurs
   * as a result of a preview request.
   * @param pageLayout Layout information about the document.
   * @param hasCustomPageSizeStyle Whether this document has a custom page size
   *     or style to use.
   */
  private onPageLayoutReady_(
      pageLayout: PageLayoutInfo, hasCustomPageSizeStyle: boolean) {
    const origin =
        new Coordinate2d(pageLayout.printableAreaX, pageLayout.printableAreaY);
    const size =
        new Size(pageLayout.printableAreaWidth, pageLayout.printableAreaHeight);

    const margins = new Margins(
        Math.round(pageLayout.marginTop), Math.round(pageLayout.marginRight),
        Math.round(pageLayout.marginBottom), Math.round(pageLayout.marginLeft));

    const o = CustomMarginsOrientation;
    const pageSize = new Size(
        pageLayout.contentWidth + margins.get(o.LEFT) + margins.get(o.RIGHT),
        pageLayout.contentHeight + margins.get(o.TOP) + margins.get(o.BOTTOM));

    if (this.isInitialized_) {
      this.printableArea = new PrintableArea(origin, size);
      this.pageSize = pageSize;
      this.set('documentSettings.hasCssMediaStyles', hasCustomPageSizeStyle);
      this.margins = margins;
    }
  }

  /**
   * Called when the document page count is received from the native layer.
   * Always occurs as a result of a preview request.
   * @param pageCount The document's page count.
   * @param previewResponseId The request ID for this page count event.
   * @param fitToPageScaling The scaling required to fit the document to page.
   */
  private onPageCountReady_(
      pageCount: number, previewResponseId: number, fitToPageScaling: number) {
    if (this.inFlightRequestId !== previewResponseId || !this.isInitialized_) {
      return;
    }
    this.set('documentSettings.pageCount', pageCount);
    this.set('documentSettings.fitToPageScaling', fitToPageScaling);
  }
}

customElements.define(
    PrintPreviewDocumentInfoElement.is, PrintPreviewDocumentInfoElement);
