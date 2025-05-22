// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {Coordinate2d} from './coordinate2d.js';
import {Margins} from './margins.js';
import {PrintableArea} from './printable_area.js';
import {Size} from './size.js';

export interface DocumentSettings {
  allPagesHaveCustomSize: boolean;
  allPagesHaveCustomOrientation: boolean;
  hasSelection: boolean;
  isModifiable: boolean;
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

export function createDocumentSettings(): DocumentSettings {
  return {
    allPagesHaveCustomSize: false,
    allPagesHaveCustomOrientation: false,
    hasSelection: false,
    isModifiable: true,
    isScalingDisabled: false,
    fitToPageScaling: 100,
    pageCount: 0,
    title: '',
  };
}

const PrintPreviewDocumentInfoElementBase = WebUiListenerMixinLit(CrLitElement);

export class PrintPreviewDocumentInfoElement extends
    PrintPreviewDocumentInfoElementBase {
  static get is() {
    return 'print-preview-document-info';
  }

  static override get properties() {
    return {
      documentSettings: {
        type: Object,
        notify: true,
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
      },

      /**
       * Printable area of the document in points.
       */
      printableArea: {
        type: Object,
        notify: true,
      },
    };
  }

  accessor documentSettings: DocumentSettings = createDocumentSettings();
  inFlightRequestId: number = -1;
  accessor margins: Margins|null = null;
  accessor pageSize: Size = new Size(612, 792);
  accessor printableArea: PrintableArea =
      new PrintableArea(new Coordinate2d(0, 0), new Size(612, 792));
  private isInitialized_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'page-count-ready',
        (pageCount: number, previewResponseId: number, scaling: number) =>
            this.onPageCountReady_(pageCount, previewResponseId, scaling));
    this.addWebUiListener(
        'page-layout-ready',
        (pageLayout: PageLayoutInfo, allPagesHaveCustomSize: boolean,
         allPagesHaveCustomOrientation: boolean) =>
            this.onPageLayoutReady_(
                pageLayout, allPagesHaveCustomSize,
                allPagesHaveCustomOrientation));
  }

  // Whenever documentSettings needs to be modified need to use
  // cloneAndModify_(), which implements the immutable data pattern, because
  // modifying in-place will not trigger Lit observers.
  private cloneAndModify_(
      callback: (documentSettings: DocumentSettings) => void) {
    const clone = structuredClone(this.documentSettings);
    callback(clone);
    this.documentSettings = clone;
  }

  /**
   * Initializes the state of the data model.
   */
  init(isModifiable: boolean, title: string, hasSelection: boolean) {
    this.isInitialized_ = true;

    this.cloneAndModify_(documentSettings => {
      documentSettings.isModifiable = isModifiable;
      documentSettings.title = title;
      documentSettings.hasSelection = hasSelection;
    });
  }

  /**
   * Updates whether scaling is disabled for the document.
   */
  updateIsScalingDisabled(isScalingDisabled: boolean) {
    if (this.isInitialized_) {
      this.cloneAndModify_(documentSettings => {
        documentSettings.isScalingDisabled = isScalingDisabled;
      });
    }
  }

  /**
   * Called when the page layout of the document is ready. Always occurs
   * as a result of a preview request.
   * @param pageLayout Layout information about the document.
   * @param allPagesHaveCustomSize Whether this document has a custom page size
   *     or style to use for all pages.
   * @param allPagesHaveCustomOrientation Whether this document has a custom
   *     page orientation to use for all pages.
   */
  private onPageLayoutReady_(
      pageLayout: PageLayoutInfo, allPagesHaveCustomSize: boolean,
      allPagesHaveCustomOrientation: boolean) {
    const origin =
        new Coordinate2d(pageLayout.printableAreaX, pageLayout.printableAreaY);
    const size =
        new Size(pageLayout.printableAreaWidth, pageLayout.printableAreaHeight);

    const pageSize = new Size(
        Math.round(
            pageLayout.contentWidth + pageLayout.marginLeft +
            pageLayout.marginRight),
        Math.round(
            pageLayout.contentHeight + pageLayout.marginTop +
            pageLayout.marginBottom));

    // Note that `Margins` stores rounded margin values, which is not
    // appropriate for use with `pageSize` above, as that could cause rounding
    // errors.
    const margins = new Margins(
        pageLayout.marginTop, pageLayout.marginRight, pageLayout.marginBottom,
        pageLayout.marginLeft);

    if (this.isInitialized_) {
      this.printableArea = new PrintableArea(origin, size);
      this.pageSize = pageSize;
      this.cloneAndModify_(documentSettings => {
        documentSettings.allPagesHaveCustomSize = allPagesHaveCustomSize;
        documentSettings.allPagesHaveCustomOrientation =
            allPagesHaveCustomOrientation;
      });
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

    this.cloneAndModify_(documentSettings => {
      documentSettings.pageCount = pageCount;
      documentSettings.fitToPageScaling = fitToPageScaling;
    });
  }
}

customElements.define(
    PrintPreviewDocumentInfoElement.is, PrintPreviewDocumentInfoElement);
