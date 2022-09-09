// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {FittingType, NamedDestinationMessageData, Point} from './constants.js';
import {Size} from './viewport.js';

export interface OpenPdfParams {
  url?: string;
  zoom?: number;
  view?: FittingType;
  viewPosition?: number;
  position?: Point;
  page?: number;
}

type GetNamedDestinationCallback = (name: string) =>
    Promise<NamedDestinationMessageData>;

// Parses the open pdf parameters passed in the url to set initial viewport
// settings for opening the pdf.
export class OpenPdfParamsParser {
  private getNamedDestinationCallback_: GetNamedDestinationCallback;
  private viewportDimensions_?: Size;

  /**
   * @param getNamedDestinationCallback Function called to fetch information for
   *     a named destination.
   */
  constructor(getNamedDestinationCallback: GetNamedDestinationCallback) {
    this.getNamedDestinationCallback_ = getNamedDestinationCallback;
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
    const zoomFactor = parseFloat(paramValueSplit[0]) / 100;
    if (Number.isNaN(zoomFactor)) {
      return {};
    }

    // Handle #zoom=scale.
    if (paramValueSplit.length === 1) {
      return {'zoom': zoomFactor};
    }

    // Handle #zoom=scale,left,top.
    const position = {
      x: parseFloat(paramValueSplit[1]),
      y: parseFloat(paramValueSplit[2]),
    };
    return {'position': position, 'zoom': zoomFactor};
  }

  /**
   * Parse view parameter of open PDF parameters. The PDF should be opened at
   * the specified fitting type mode and position.
   * @return Map with view parameters (view and viewPosition).
   */
  private parseViewParam_(paramValue: string): OpenPdfParams {
    const viewModeComponents = paramValue.toLowerCase().split(',');
    if (viewModeComponents.length < 1) {
      return {};
    }

    const params: OpenPdfParams = {};
    const viewMode = viewModeComponents[0];
    let acceptsPositionParam;
    if (viewMode === 'fit') {
      params['view'] = FittingType.FIT_TO_PAGE;
      acceptsPositionParam = false;
    } else if (viewMode === 'fith') {
      params['view'] = FittingType.FIT_TO_WIDTH;
      acceptsPositionParam = true;
    } else if (viewMode === 'fitv') {
      params['view'] = FittingType.FIT_TO_HEIGHT;
      acceptsPositionParam = true;
    }

    if (!acceptsPositionParam || viewModeComponents.length < 2) {
      return params;
    }

    const position = parseFloat(viewModeComponents[1]);
    if (!Number.isNaN(position)) {
      params['viewPosition'] = position;
    }

    return params;
  }

  /**
   * Parse view parameters which come from nameddest.
   * @return Map with view parameters.
   */
  private parseNameddestViewParam_(paramValue: string): OpenPdfParams {
    const viewModeComponents = paramValue.toLowerCase().split(',');
    const viewMode = viewModeComponents[0];
    const params: OpenPdfParams = {};

    if (viewMode === 'xyz' && viewModeComponents.length === 4) {
      const x = parseFloat(viewModeComponents[1]);
      const y = parseFloat(viewModeComponents[2]);
      const zoom = parseFloat(viewModeComponents[3]);

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

    if (viewMode === 'fitr' && viewModeComponents.length === 5) {
      assert(this.viewportDimensions_ !== undefined);
      let x1 = parseFloat(viewModeComponents[1]);
      let y1 = parseFloat(viewModeComponents[2]);
      let x2 = parseFloat(viewModeComponents[3]);
      let y2 = parseFloat(viewModeComponents[4]);
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

    return this.parseViewParam_(paramValue);
  }

  /** Parse the parameters encoded in the fragment of a URL. */
  private parseUrlParams_(url: string): URLSearchParams {
    const hash = new URL(url).hash;
    const params = new URLSearchParams(hash.substring(1));

    // Handle the case of http://foo.com/bar#NAMEDDEST. This is not
    // explicitly mentioned except by example in the Adobe
    // "PDF Open Parameters" document.
    if (Array.from(params).length === 1) {
      const key = Array.from(params.keys())[0];
      if (params.get(key) === '') {
        params.append('nameddest', key);
        params.delete(key);
      }
    }

    return params;
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
    return this.parseUrlParams_(url).get('toolbar') !== '0';
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

    if (urlParams.has('page')) {
      // |pageNumber| is 1-based, but goToPage() take a zero-based page index.
      const pageNumber = parseInt(urlParams.get('page')!, 10);
      if (!Number.isNaN(pageNumber) && pageNumber > 0) {
        params['page'] = pageNumber - 1;
      }
    }

    if (urlParams.has('view')) {
      Object.assign(params, this.parseViewParam_(urlParams.get('view')!));
    }

    if (urlParams.has('zoom')) {
      Object.assign(params, this.parseZoomParam_(urlParams.get('zoom')!));
    }

    if (params.page === undefined && urlParams.has('nameddest')) {
      const data =
          await this.getNamedDestinationCallback_(urlParams.get('nameddest')!);

      if (data.pageNumber !== -1) {
        params.page = data.pageNumber;
      }

      if (data.namedDestinationView) {
        Object.assign(
            params, this.parseNameddestViewParam_(data.namedDestinationView));
      }

      return params;
    }

    return Promise.resolve(params);
  }
}
