// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import type {NamedDestinationMessageData, Point, Rect} from './constants.js';
import {FittingType} from './constants.js';
import type {Size} from './viewport.js';

export interface OpenPdfParams {
  boundingBox?: Rect;
  page?: number;
  position?: Point;
  url?: string;
  view?: FittingType;
  viewPosition?: number;
  zoom?: number;
}

export enum ViewMode {
  FIT = 'fit',
  FIT_B = 'fitb',
  FIT_BH = 'fitbh',
  FIT_BV = 'fitbv',
  FIT_H = 'fith',
  FIT_R = 'fitr',
  FIT_V = 'fitv',
  XYZ = 'xyz',
}

type GetNamedDestinationCallback = (name: string) =>
    Promise<NamedDestinationMessageData>;

type GetPageBoundingBoxCallback = (page: number) => Promise<Rect>;

// Parses the open pdf parameters passed in the url to set initial viewport
// settings for opening the pdf.
export class OpenPdfParamsParser {
  private getNamedDestinationCallback_: GetNamedDestinationCallback;
  private getPageBoundingBoxCallback_: GetPageBoundingBoxCallback;
  private pageCount_?: number;
  private viewportDimensions_?: Size;

  /**
   * @param getNamedDestinationCallback Function called to fetch information for
   *     a named destination.
   * @param getPageBoundingBoxCallback Function called to fetch information for
   *     a page's bounding box.
   */
  constructor(
      getNamedDestinationCallback: GetNamedDestinationCallback,
      getPageBoundingBoxCallback: GetPageBoundingBoxCallback) {
    this.getNamedDestinationCallback_ = getNamedDestinationCallback;
    this.getPageBoundingBoxCallback_ = getPageBoundingBoxCallback;
  }

  /**
   * Calculate the zoom level needed for making viewport focus on a rectangular
   * area in the PDF document.
   * @param size The dimensions of the rectangular area to be focused on.
   * @return The zoom level needed for focusing on the rectangular area. A zoom
   *     level of 0 indicates that the zoom level cannot be calculated with the
   *     given information.
   */
  private calculateRectZoomLevel_(size: Size): number {
    if (size.height === 0 || size.width === 0) {
      return 0;
    }

    assert(this.viewportDimensions_);
    return Math.min(
        this.viewportDimensions_.height / size.height,
        this.viewportDimensions_.width / size.width);
  }

  /**
   * Parse zoom parameter of open PDF parameters. The PDF should be opened at
   * the specified zoom level.
   * @return Map with zoom parameters (zoom and position).
   */
  private parseZoomParam_(paramValue: string): OpenPdfParams {
    const paramValueSplit = paramValue.split(',');
    if (paramValueSplit.length !== 1 && paramValueSplit.length !== 3) {
      return {};
    }

    // User scale of 100 means zoom value of 100% i.e. zoom factor of 1.0.
    const zoomFactor = parseFloat(paramValueSplit[0]!) / 100;
    if (Number.isNaN(zoomFactor)) {
      return {};
    }

    // Handle #zoom=scale.
    if (paramValueSplit.length === 1) {
      return {'zoom': zoomFactor};
    }

    // Handle #zoom=scale,left,top.
    const position = {
      x: parseFloat(paramValueSplit[1]!),
      y: parseFloat(paramValueSplit[2]!),
    };
    return {'position': position, 'zoom': zoomFactor};
  }

  /**
   * Parse view parameter of open PDF parameters. The PDF should be opened at
   * the specified fitting type mode and position.
   * @param paramValue Params to parse.
   * @param pageNumber Page number for bounding box, if there is a fit bounding
   *     box param. `pageNumber` is 1-indexed and must be bounded by 1 and the
   *     number of pages in the PDF, inclusive.
   * @return Map with view parameters (view and viewPosition).
   */
  private async parseViewParam_(paramValue: string, pageNumber: number):
      Promise<OpenPdfParams> {
    assert(pageNumber > 0);
    if (this.pageCount_) {
      assert(pageNumber <= this.pageCount_);
    }

    const viewModeComponents = paramValue.toLowerCase().split(',');
    if (viewModeComponents.length === 0) {
      return {};
    }

    const params: OpenPdfParams = {};
    const viewMode = viewModeComponents[0]!;
    let acceptsPositionParam = false;

    // Note that `pageNumber` is 1-indexed, but PDF Viewer is 0-indexed.
    switch (viewMode) {
      case ViewMode.FIT:
        params['view'] = FittingType.FIT_TO_PAGE;
        break;
      case ViewMode.FIT_H:
        params['view'] = FittingType.FIT_TO_WIDTH;
        acceptsPositionParam = true;
        break;
      case ViewMode.FIT_V:
        params['view'] = FittingType.FIT_TO_HEIGHT;
        acceptsPositionParam = true;
        break;
      case ViewMode.FIT_B:
        if (this.pageCount_) {
          params['view'] = FittingType.FIT_TO_BOUNDING_BOX;
          params['boundingBox'] =
              await this.getPageBoundingBoxCallback_(pageNumber - 1);
        }
        break;
      case ViewMode.FIT_BH:
        if (this.pageCount_) {
          params['view'] = FittingType.FIT_TO_BOUNDING_BOX_WIDTH;
          params['boundingBox'] =
              await this.getPageBoundingBoxCallback_(pageNumber - 1);
          acceptsPositionParam = true;
        }
        break;
      case ViewMode.FIT_BV:
        if (this.pageCount_) {
          params['view'] = FittingType.FIT_TO_BOUNDING_BOX_HEIGHT;
          params['boundingBox'] =
              await this.getPageBoundingBoxCallback_(pageNumber - 1);
          acceptsPositionParam = true;
        }
        break;
      case ViewMode.FIT_R:
      case ViewMode.XYZ:
        // Should have already been handled in `parseNameddestViewParam_()`.
        break;
      default:
        // Invalid view parameter, do nothing.
    }

    if (!acceptsPositionParam || viewModeComponents.length === 1) {
      return params;
    }

    const position = parseFloat(viewModeComponents[1]!);
    if (!Number.isNaN(position)) {
      params['viewPosition'] = position;
    }

    return params;
  }

  /**
   * Parse view parameters which come from nameddest.
   * @param paramValue Params to parse.
   * @param pageNumber Page number for bounding box, if there is a fit bounding
   *     box param.
   * @return Map with view parameters.
   */
  private async parseNameddestViewParam_(
      paramValue: string, pageNumber: number): Promise<OpenPdfParams> {
    const viewModeComponents = paramValue.toLowerCase().split(',');
    assert(viewModeComponents.length > 0);
    const viewMode = viewModeComponents[0]!;
    const params: OpenPdfParams = {};

    if (viewMode === ViewMode.XYZ && viewModeComponents.length === 4) {
      const x = parseFloat(viewModeComponents[1]!);
      const y = parseFloat(viewModeComponents[2]!);
      const zoom = parseFloat(viewModeComponents[3]!);

      // If zoom is originally 0 for the XYZ view, it is guaranteed to be
      // transformed into "null" by the backend.
      assert(zoom !== 0);

      if (!Number.isNaN(zoom)) {
        params['zoom'] = zoom;
      }
      if (!Number.isNaN(x) || !Number.isNaN(y)) {
        params['position'] = {x: x, y: y};
      }
      return params;
    }

    if (viewMode === ViewMode.FIT_R && viewModeComponents.length === 5) {
      assert(this.viewportDimensions_ !== undefined);
      let x1 = parseFloat(viewModeComponents[1]!);
      let y1 = parseFloat(viewModeComponents[2]!);
      let x2 = parseFloat(viewModeComponents[3]!);
      let y2 = parseFloat(viewModeComponents[4]!);
      if (!Number.isNaN(x1) && !Number.isNaN(y1) && !Number.isNaN(x2) &&
          !Number.isNaN(y2)) {
        if (x1 > x2) {
          [x1, x2] = [x2, x1];
        }
        if (y1 > y2) {
          [y1, y2] = [y2, y1];
        }
        const rectSize = {width: x2 - x1, height: y2 - y1};
        params['position'] = {x: x1, y: y1};
        const zoom = this.calculateRectZoomLevel_(rectSize);
        if (zoom !== 0) {
          params['zoom'] = zoom;
        }
      }
      return params;
    }

    return this.parseViewParam_(paramValue, pageNumber);
  }

  /** Parse the parameters encoded in the fragment of a URL. */
  private parseUrlParams_(url: string): URLSearchParams {
    const hash = new URL(url).hash;
    const params = new URLSearchParams(hash.substring(1));

    // Handle the case of http://foo.com/bar#NAMEDDEST. This is not
    // explicitly mentioned except by example in the Adobe
    // "PDF Open Parameters" document.
    if (Array.from(params).length === 1) {
      const key = Array.from(params.keys())[0]!;
      if (params.get(key) === '') {
        params.append('nameddest', key);
        params.delete(key);
      }
    }

    return params;
  }

  /** Store the number of pages. */
  setPageCount(pageCount: number) {
    this.pageCount_ = pageCount;
  }

  /** Store current viewport's dimensions. */
  setViewportDimensions(dimensions: Size) {
    this.viewportDimensions_ = dimensions;
  }

  /**
   * @param url that needs to be parsed.
   * @return Whether the toolbar UI element should be shown.
   */
  shouldShowToolbar(url: string): boolean {
    const urlParams = this.parseUrlParams_(url);
    const navpanes = urlParams.get('navpanes');
    const toolbar = urlParams.get('toolbar');

    // If navpanes is set to '1', then the toolbar must be shown, regardless of
    // the value of toolbar.
    return navpanes === '1' || toolbar !== '0';
  }

  /**
   * @param url that needs to be parsed.
   * @param sidenavCollapsed the default sidenav state if there are no
   *     overriding open parameters.
   * @return Whether the sidenav UI element should be shown.
   */
  shouldShowSidenav(url: string, sidenavCollapsed: boolean): boolean {
    const urlParams = this.parseUrlParams_(url);
    const navpanes = urlParams.get('navpanes');
    const toolbar = urlParams.get('toolbar');

    // If there are no relevant open parameters, default to the original value.
    if (navpanes === null && toolbar === null) {
      return !sidenavCollapsed;
    }

    return navpanes === '1';
  }

  /**
   * Parse PDF url parameters. These parameters are mentioned in the url
   * and specify actions to be performed when opening pdf files.
   * See http://www.adobe.com/content/dam/Adobe/en/devnet/acrobat/
   * pdfs/pdf_open_parameters.pdf for details.
   * @param url that needs to be parsed.
   */
  async getViewportFromUrlParams(url: string): Promise<OpenPdfParams> {
    const params: OpenPdfParams = {url};

    const urlParams = this.parseUrlParams_(url);

    // `pageNumber` is 1-based.
    let pageNumber = 1;
    if (urlParams.has('page')) {
      pageNumber = parseInt(urlParams.get('page')!, 10);
      if (!Number.isNaN(pageNumber) && this.pageCount_) {
        // If necessary, clip `pageNumber` to stay within bounds.
        if (pageNumber < 1) {
          pageNumber = 1;
        } else if (pageNumber > this.pageCount_) {
          pageNumber = this.pageCount_;
        }
        // goToPage() takes a zero-based page index.
        params['page'] = pageNumber - 1;
      }
    }

    if (urlParams.has('view')) {
      Object.assign(
          params,
          await this.parseViewParam_(urlParams.get('view')!, pageNumber));
    }

    if (urlParams.has('zoom')) {
      Object.assign(params, this.parseZoomParam_(urlParams.get('zoom')!));
    }

    if (params.page === undefined && urlParams.has('nameddest')) {
      const data =
          await this.getNamedDestinationCallback_(urlParams.get('nameddest')!);

      if (data.pageNumber !== -1) {
        params.page = data.pageNumber;
        pageNumber = data.pageNumber;
      }

      if (data.namedDestinationView) {
        Object.assign(
            params,
            await this.parseNameddestViewParam_(
                data.namedDestinationView, pageNumber!));
      }
      return params;
    }
    return params;
  }
}
