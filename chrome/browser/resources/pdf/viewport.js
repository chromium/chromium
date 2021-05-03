// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {$, hasKeyModifiers} from 'chrome://resources/js/util.m.js';

import {FittingType, Point} from './constants.js';
import {GestureDetector, PinchEventDetail} from './gesture_detector.js';
import {InactiveZoomManager, ZoomManager} from './zoom_manager.js';

/**
 * @typedef {{
 *   width: number,
 *   height: number,
 *   layoutOptions: (!LayoutOptions|undefined),
 *   pageDimensions: Array<ViewportRect>,
 * }}
 */
let DocumentDimensions;

/**
 * @typedef {{
 *   defaultPageOrientation: number,
 *   twoUpViewEnabled: boolean,
 * }}
 */
export let LayoutOptions;

/** @typedef {{x: (number|undefined), y: (number|undefined)}} */
export let PartialPoint;

/** @typedef {{width: number, height: number}} */
export let Size;

/** @typedef {{x: number, y: number, width: number, height: number}} */
let ViewportRect;

/**
 * @param {!ViewportRect} rect1
 * @param {!ViewportRect} rect2
 * @return {number} The area of the intersection of the rects
 */
function getIntersectionArea(rect1, rect2) {
  const left = Math.max(rect1.x, rect2.x);
  const top = Math.max(rect1.y, rect2.y);
  const right = Math.min(rect1.x + rect1.width, rect2.x + rect2.width);
  const bottom = Math.min(rect1.y + rect1.height, rect2.y + rect2.height);

  if (left >= right || top >= bottom) {
    return 0;
  }

  return (right - left) * (bottom - top);
}

/**
 * @param {!Point} p1
 * @param {!Point} p2
 * @return {!Point} The vector between the two points.
 */
function vectorDelta(p1, p2) {
  return {x: p2.x - p1.x, y: p2.y - p1.y};
}

export class Viewport {
  /**
   * @param {!HTMLElement} scrollParent
   * @param {!HTMLDivElement} sizer The element which represents the size of the
   *     document in the viewport
   * @param {!HTMLDivElement} content The element which is the parent of the
   *     plugin in the viewer.
   * @param {number} scrollbarWidth The width of scrollbars on the page
   * @param {number} defaultZoom The default zoom level.
   */
  constructor(scrollParent, sizer, content, scrollbarWidth, defaultZoom) {
    /** @private {!HTMLElement} */
    this.window_ = scrollParent;

    /** @private {!HTMLDivElement} */
    this.sizer_ = sizer;

    /** @private {!HTMLDivElement} */
    this.content_ = content;

    /** @private {number} */
    this.scrollbarWidth_ = scrollbarWidth;

    /** @private {number} */
    this.defaultZoom_ = defaultZoom;

    /** @private {function():void} */
    this.viewportChangedCallback_ = function() {};

    /** @private {function():void} */
    this.beforeZoomCallback_ = function() {};

    /** @private {function():void} */
    this.afterZoomCallback_ = function() {};

    /** @private {function(boolean):void} */
    this.userInitiatedCallback_ = function() {};

    /** @private {boolean} */
    this.allowedToChangeZoom_ = false;

    /** @private {number} */
    this.internalZoom_ = 1;

    /**
     * Predefined zoom factors to be used when zooming in/out. These are in
     * ascending order.
     * @private {!Array<number>}
     */
    this.presetZoomFactors_ = [];

    /** @private {?ZoomManager} */
    this.zoomManager_ = null;

    /** @private {?DocumentDimensions} */
    this.documentDimensions_ = null;

    /** @private {Array<ViewportRect>} */
    this.pageDimensions_ = [];

    /** @private {!FittingType} */
    this.fittingType_ = FittingType.NONE;

    /** @private {number} */
    this.prevScale_ = 1;

    /** @private {!PinchPhase} */
    this.pinchPhase_ = PinchPhase.NONE;

    /** @private {?Point} */
    this.pinchPanVector_ = null;

    /** @private {?Point} */
    this.pinchCenter_ = null;

    /** @private {?Point} */
    this.firstPinchCenterInFrame_ = null;

    /** @private {?Point} */
    this.oldCenterInContent_ = null;

    /** @private {boolean} */
    this.keepContentCentered_ = false;

    /** @private {!EventTracker} */
    this.tracker_ = new EventTracker();

    /** @private {!GestureDetector} */
    this.gestureDetector_ = new GestureDetector(this.content_);

    /** @private {boolean} */
    this.sentPinchEvent_ = false;

    this.gestureDetector_.getEventTarget().addEventListener(
        'pinchstart',
        e => this.onPinchStart_(
            /** @type {!CustomEvent<!PinchEventDetail>} */ (e)));
    this.gestureDetector_.getEventTarget().addEventListener(
        'pinchupdate',
        e => this.onPinchUpdate_(
            /** @type {!CustomEvent<!PinchEventDetail>} */ (e)));
    this.gestureDetector_.getEventTarget().addEventListener(
        'pinchend',
        e => this.onPinchEnd_(
            /** @type {!CustomEvent<!PinchEventDetail>} */ (e)));

    // Set to a default zoom manager - used in tests.
    this.setZoomManager(new InactiveZoomManager(this.getZoom.bind(this), 1));

    // Print Preview
    if (this.window_ === document.documentElement ||
        // Necessary check since during testing a fake DOM element is used.
        !(this.window_ instanceof HTMLElement)) {
      window.addEventListener('scroll', this.updateViewport_.bind(this));
      // The following line is only used in tests, since they expect
      // |scrollCallback| to be called on the mock |window_| object (legacy).
      this.window_.scrollCallback = this.updateViewport_.bind(this);
      window.addEventListener('resize', this.resizeWrapper_.bind(this));
      // The following line is only used in tests, since they expect
      // |resizeCallback| to be called on the mock |window_| object (legacy).
      this.window_.resizeCallback = this.resizeWrapper_.bind(this);
    } else {
      // Standard PDF viewer
      this.window_.addEventListener('scroll', this.updateViewport_.bind(this));
      const resizeObserver = new ResizeObserver(_ => this.resizeWrapper_());
      const target = this.window_.parentElement;
      assert(target.id === 'main');
      resizeObserver.observe(target);
    }

    document.body.addEventListener(
        'change-zoom', e => this.setZoom(e.detail.zoom));
  }

  /** @param {function():void} viewportChangedCallback */
  setViewportChangedCallback(viewportChangedCallback) {
    this.viewportChangedCallback_ = viewportChangedCallback;
  }

  /** @param {function():void} beforeZoomCallback */
  setBeforeZoomCallback(beforeZoomCallback) {
    this.beforeZoomCallback_ = beforeZoomCallback;
  }

  /** @param {function():void} afterZoomCallback */
  setAfterZoomCallback(afterZoomCallback) {
    this.afterZoomCallback_ = afterZoomCallback;
  }

  /** @param {function(boolean):void} userInitiatedCallback */
  setUserInitiatedCallback(userInitiatedCallback) {
    this.userInitiatedCallback_ = userInitiatedCallback;
  }

  /**
   * @return {number} The number of clockwise 90-degree rotations that have been
   *     applied.
   */
  getClockwiseRotations() {
    const options = this.getLayoutOptions();
    return options ? options.defaultPageOrientation : 0;
  }

  /** @return {boolean} Whether viewport is in two-up view mode. */
  twoUpViewEnabled() {
    const options = this.getLayoutOptions();
    return !!options && options.twoUpViewEnabled;
  }

  /**
   * Clamps the zoom factor (or page scale factor) to be within the limits.
   * @param {number} factor The zoom/scale factor.
   * @return {number} The factor clamped within the limits.
   * @private
   */
  clampZoom_(factor) {
    return Math.max(
        this.presetZoomFactors_[0],
        Math.min(
            factor,
            this.presetZoomFactors_[this.presetZoomFactors_.length - 1]));
  }

  /**
   * @param {!Array<number>} factors Array containing zoom/scale factors.
   */
  setZoomFactorRange(factors) {
    assert(factors.length !== 0);
    this.presetZoomFactors_ = factors;
  }

  /**
   * Converts a page position (e.g. the location of a bookmark) to a screen
   * position.
   * @param {number} page
   * @param {!Point} point The position on `page`.
   * @return {!Point} The screen position.
   */
  convertPageToScreen(page, point) {
    const dimensions = this.getPageInsetDimensions(page);

    // width & height are already rotated.
    const height = dimensions.height;
    const width = dimensions.width;

    // TODO(dpapad): Use the no-arg constructor when
    // https://github.com/google/closure-compiler/issues/3768 is fixed.
    const matrix = new DOMMatrix([1, 0, 0, 1, 0, 0]);

    const rotation = this.getClockwiseRotations() * 90;
    // Set origin for rotation.
    if (rotation === 90) {
      matrix.translateSelf(width, 0);
    } else if (rotation === 180) {
      matrix.translateSelf(width, height);
    } else if (rotation === 270) {
      matrix.translateSelf(0, height);
    }
    matrix.rotateSelf(0, 0, rotation);

    // Invert Y position with respect to height as page coordinates are
    // measured from the bottom left.
    matrix.translateSelf(0, height);
    matrix.scaleSelf(1, -1);

    const pointsToPixels = 96 / 72;
    const result = matrix.transformPoint(
        new DOMPoint(point.x * pointsToPixels, point.y * pointsToPixels));
    return {
      x: result.x + PAGE_SHADOW.left,
      y: result.y + PAGE_SHADOW.top,
    };
  }


  /**
   * Returns the zoomed and rounded document dimensions for the given zoom.
   * Rounding is necessary when interacting with the renderer which tends to
   * operate in integral values (for example for determining if scrollbars
   * should be shown).
   * @param {number} zoom The zoom to use to compute the scaled dimensions.
   * @return {?Size} Scaled 'width' and 'height' of the document.
   * @private
   */
  getZoomedDocumentDimensions_(zoom) {
    if (!this.documentDimensions_) {
      return null;
    }
    return {
      width: Math.round(this.documentDimensions_.width * zoom),
      height: Math.round(this.documentDimensions_.height * zoom)
    };
  }

  /** @return {!Size} A dictionary with the 'width'/'height' of the document. */
  getDocumentDimensions() {
    return {
      width: this.documentDimensions_.width,
      height: this.documentDimensions_.height
    };
  }

  /**
   * @return {!LayoutOptions|undefined} A dictionary carrying layout options
   *     from the plugin.
   */
  getLayoutOptions() {
    return this.documentDimensions_ ? this.documentDimensions_.layoutOptions :
                                      undefined;
  }

  /**
   * @return {!ViewportRect} ViewportRect for the viewport given current zoom.
   * @private
   */
  getViewportRect_() {
    const zoom = this.getZoom();
    // Zoom can be 0 in the case of a PDF that is in a hidden iframe. Avoid
    // returning undefined values in this case. See https://crbug.com/1202725.
    if (zoom === 0) {
      return {
        x: 0,
        y: 0,
        width: 0,
        height: 0,
      };
    }
    return {
      x: this.position.x / zoom,
      y: this.position.y / zoom,
      width: this.size.width / zoom,
      height: this.size.height / zoom
    };
  }

  /**
   * @param {number} zoom Zoom to compute scrollbars for
   * @return {{horizontal: boolean, vertical: boolean}} Whether horizontal or
   *     vertical scrollbars are needed.
   * Public so tests can call it directly.
   */
  documentNeedsScrollbars(zoom) {
    const zoomedDimensions = this.getZoomedDocumentDimensions_(zoom);
    if (!zoomedDimensions) {
      return {horizontal: false, vertical: false};
    }

    return {
      horizontal: zoomedDimensions.width > this.window_.offsetWidth,
      vertical: zoomedDimensions.height > this.window_.offsetHeight
    };
  }

  /**
   * @return {!{horizontal: boolean, vertical: boolean}} Whether horizontal and
   *     vertical scrollbars are needed.
   */
  documentHasScrollbars() {
    return this.documentNeedsScrollbars(this.getZoom());
  }

  /**
   * Helper function called when the zoomed document size changes. Updates the
   * sizer's width and height.
   * @private
   */
  contentSizeChanged_() {
    const zoomedDimensions = this.getZoomedDocumentDimensions_(this.getZoom());
    if (zoomedDimensions) {
      this.sizer_.style.width = zoomedDimensions.width + 'px';
      this.sizer_.style.height = zoomedDimensions.height + 'px';
    }
  }

  /**
   * @param {!Point} coordinateInFrame
   * @return {!Point} Coordinate converted to plugin coordinates.
   * @private
   */
  frameToPluginCoordinate_(coordinateInFrame) {
    const container = this.content_.querySelector('#plugin');
    return {
      x: coordinateInFrame.x - container.getBoundingClientRect().left,
      y: coordinateInFrame.y - container.getBoundingClientRect().top
    };
  }

  /**
   * Called when the viewport should be updated.
   * @private
   */
  updateViewport_() {
    this.viewportChangedCallback_();
  }

  /**
   * Called when the browser window size changes.
   * @private
   */
  resizeWrapper_() {
    this.userInitiatedCallback_(false);
    this.resize_();
    this.userInitiatedCallback_(true);
  }

  /**
   * Called when the viewport size changes.
   * @private
   */
  resize_() {
    // Force fit-to-height when resizing happens as a result of entering full
    // screen mode.
    if (document.fullscreenElement !== null) {
      this.fittingType_ = FittingType.FIT_TO_HEIGHT;
      this.window_.dispatchEvent(
          new CustomEvent('fitting-type-changed-for-testing'));
    }

    if (this.fittingType_ === FittingType.FIT_TO_PAGE) {
      this.fitToPageInternal_(false);
    } else if (this.fittingType_ === FittingType.FIT_TO_WIDTH) {
      this.fitToWidth();
    } else if (this.fittingType_ === FittingType.FIT_TO_HEIGHT) {
      this.fitToHeightInternal_(document.fullscreenElement !== null);
    } else if (this.internalZoom_ === 0) {
      this.fitToNone();
    } else {
      this.updateViewport_();
    }
  }

  /** @return {!Point} The scroll position of the viewport. */
  get position() {
    return {x: this.window_.scrollLeft, y: this.window_.scrollTop};
  }

  /**
   * Scroll the viewport to the specified position.
   * @param {!Point} position The position to scroll to.
   */
  set position(position) {
    this.window_.scrollTo(position.x, position.y);
  }

  /** @return {!Size} the size of the viewport excluding scrollbars. */
  get size() {
    return {
      width: this.window_.offsetWidth,
      height: this.window_.offsetHeight,
    };
  }

  /** @return {number} The current zoom. */
  getZoom() {
    return this.zoomManager_.applyBrowserZoom(this.internalZoom_);
  }

  /** @return {!Array<number>} The preset zoom factors. */
  get presetZoomFactors() {
    return this.presetZoomFactors_;
  }

  /** @param {!ZoomManager} manager */
  setZoomManager(manager) {
    this.resetTracker();
    this.zoomManager_ = manager;
    this.tracker_.add(
        this.zoomManager_.getEventTarget(), 'set-zoom',
        e => this.setZoom(e.detail));
    this.tracker_.add(
        this.zoomManager_.getEventTarget(), 'update-zoom-from-browser',
        this.updateZoomFromBrowserChange_.bind(this));
  }

  /**
   * @return {!PinchPhase} The phase of the current pinch gesture for
   *    the viewport.
   */
  get pinchPhase() {
    return this.pinchPhase_;
  }

  /**
   * @return {?Point} The panning caused by the current pinch gesture (as
   *    the deltas of the x and y coordinates).
   */
  get pinchPanVector() {
    return this.pinchPanVector_;
  }

  /**
   * @return {?Point} The coordinates of the center of the current pinch
   *     gesture.
   */
  get pinchCenter() {
    return this.pinchCenter_;
  }

  /**
   * Used to wrap a function that might perform zooming on the viewport. This is
   * required so that we can notify the plugin that zooming is in progress
   * so that while zooming is taking place it can stop reacting to scroll events
   * from the viewport. This is to avoid flickering.
   * @param {function():void} f Function to wrap
   * @private
   */
  mightZoom_(f) {
    this.beforeZoomCallback_();
    this.allowedToChangeZoom_ = true;
    f();
    this.allowedToChangeZoom_ = false;
    this.afterZoomCallback_();
    this.zoomManager_.onPdfZoomChange();
  }

  /**
   * @param {number} newZoom The zoom level to set.
   * @private
   */
  setZoomInternal_(newZoom) {
    assert(
        this.allowedToChangeZoom_,
        'Called Viewport.setZoomInternal_ without calling ' +
            'Viewport.mightZoom_.');
    // Record the scroll position (relative to the top-left of the window).
    let zoom = this.getZoom();
    const currentScrollPos = {
      x: this.position.x / zoom,
      y: this.position.y / zoom
    };

    this.internalZoom_ = newZoom;
    this.contentSizeChanged_();
    // Scroll to the scaled scroll position.
    zoom = this.getZoom();
    this.position = {
      x: currentScrollPos.x * zoom,
      y: currentScrollPos.y * zoom
    };
  }

  /**
   * Sets the zoom of the viewport.
   * Same as setZoomInternal_ but for pinch zoom we have some more operations.
   * @param {number} scaleDelta The zoom delta.
   * @param {!Point} center The pinch center in content coordinates.
   * @private
   */
  setPinchZoomInternal_(scaleDelta, center) {
    assert(
        this.allowedToChangeZoom_,
        'Called Viewport.setPinchZoomInternal_ without calling ' +
            'Viewport.mightZoom_.');
    this.internalZoom_ = this.clampZoom_(this.internalZoom_ * scaleDelta);

    const newCenterInContent = this.frameToContent_(center);
    const delta = {
      x: (newCenterInContent.x - this.oldCenterInContent_.x),
      y: (newCenterInContent.y - this.oldCenterInContent_.y)
    };

    // Record the scroll position (relative to the pinch center).
    const zoom = this.getZoom();
    const currentScrollPos = {
      x: this.position.x - delta.x * zoom,
      y: this.position.y - delta.y * zoom
    };

    this.contentSizeChanged_();
    // Scroll to the scaled scroll position.
    this.position = {x: currentScrollPos.x, y: currentScrollPos.y};
  }

  /**
   *  Converts a point from frame to content coordinates.
   *  @param {!Point} framePoint The frame coordinates.
   *  @return {!Point} The content coordinates.
   *  @private
   */
  frameToContent_(framePoint) {
    // TODO(mcnee) Add a helper Point class to avoid duplicating operations
    // on plain {x,y} objects.
    const zoom = this.getZoom();
    return {
      x: (framePoint.x + this.position.x) / zoom,
      y: (framePoint.y + this.position.y) / zoom
    };
  }

  /** @param {number} newZoom The zoom level to zoom to. */
  setZoom(newZoom) {
    this.fittingType_ = FittingType.NONE;
    this.mightZoom_(() => {
      this.setZoomInternal_(this.clampZoom_(newZoom));
      this.updateViewport_();
    });
  }

  /**
   * @param {!CustomEvent<number>} e Event containing the old browser zoom.
   * @private
   */
  updateZoomFromBrowserChange_(e) {
    const oldBrowserZoom = e.detail;
    this.mightZoom_(() => {
      // Record the scroll position (relative to the top-left of the window).
      const oldZoom = oldBrowserZoom * this.internalZoom_;
      const currentScrollPos = {
        x: this.position.x / oldZoom,
        y: this.position.y / oldZoom
      };
      this.contentSizeChanged_();
      const newZoom = this.getZoom();
      // Scroll to the scaled scroll position.
      this.position = {
        x: currentScrollPos.x * newZoom,
        y: currentScrollPos.y * newZoom
      };
      this.updateViewport_();
    });
  }

  /** @return {number} The width of scrollbars in the viewport in pixels. */
  get scrollbarWidth() {
    return this.scrollbarWidth_;
  }

  /** @return {FittingType} The fitting type the viewport is currently in. */
  get fittingType() {
    return this.fittingType_;
  }

  /**
   * @param {number} index
   * @return {number} The y coordinate of the bottom of the given page.
   * @private
   */
  getPageBottom_(index) {
    return this.pageDimensions_[index].y + this.pageDimensions_[index].height;
  }

  /**
   * Get the page at a given y position. If there are multiple pages
   * overlapping the given y-coordinate, return the page with the smallest
   * index.
   * @param {number} y The y-coordinate to get the page at.
   * @return {number} The index of a page overlapping the given y-coordinate.
   * @private
   */
  getPageAtY_(y) {
    assert(y >= 0);

    // Drop decimal part of |y| otherwise it can appear as larger than the
    // bottom of the last page in the document (even without the presence of a
    // horizontal scrollbar).
    y = Math.floor(y);

    let min = 0;
    let max = this.pageDimensions_.length - 1;
    if (max === min) {
      return min;
    }

    while (max >= min) {
      const page = min + Math.floor((max - min) / 2);
      // There might be a gap between the pages, in which case use the bottom
      // of the previous page as the top for finding the page.
      const top = page > 0 ? this.getPageBottom_(page - 1) : 0;
      const bottom = this.getPageBottom_(page);

      if (top <= y && y <= bottom) {
        return page;
      }

      // If the search reached the last page just return that page. |y| is
      // larger than the last page's |bottom|, which can happen either because a
      // horizontal scrollbar exists, or the document is zoomed out enough for
      // free space to exist at the bottom.
      if (page === this.pageDimensions_.length - 1) {
        return page;
      }

      if (top > y) {
        max = page - 1;
      } else {
        min = page + 1;
      }
    }

    // Should always return within the while loop above.
    assertNotReached('Could not find page for Y position: ' + y);
    return -1;
  }

  /**
   * Return the last page visible in the viewport. Returns the last index of the
   * document if the viewport is below the document.
   * @param {!ViewportRect} viewportRect
   * @return {number} The highest index of the pages visible in the viewport.
   * @private
   */
  getLastPageInViewport_(viewportRect) {
    const pageAtY = this.getPageAtY_(viewportRect.y + viewportRect.height);

    if (!this.twoUpViewEnabled() || pageAtY % 2 === 1 ||
        pageAtY + 1 >= this.pageDimensions_.length) {
      return pageAtY;
    }

    const nextPage = this.pageDimensions_[pageAtY + 1];
    return getIntersectionArea(viewportRect, nextPage) > 0 ? pageAtY + 1 :
                                                             pageAtY;
  }

  /**
   * @param {!Point} point
   * @return {boolean} Whether |point| (in screen coordinates) is inside a page
   */
  isPointInsidePage(point) {
    const zoom = this.getZoom();
    const size = this.size;
    const position = this.position;
    const page = this.getPageAtY_((position.y + point.y) / zoom);
    const pageWidth = this.pageDimensions_[page].width * zoom;
    const documentWidth = this.getDocumentDimensions().width * zoom;

    const outerWidth = Math.max(size.width, documentWidth);

    if (pageWidth >= outerWidth) {
      return true;
    }

    const x = point.x + position.x;

    const minX = (outerWidth - pageWidth) / 2;
    const maxX = outerWidth - minX;
    return x >= minX && x <= maxX;
  }

  /**
   * @return {number} The index of the page with the greatest proportion of its
   *     area in the current viewport.
   */
  getMostVisiblePage() {
    const viewportRect = this.getViewportRect_();

    const firstVisiblePage = this.getPageAtY_(viewportRect.y);
    const lastPossibleVisiblePage = this.getLastPageInViewport_(viewportRect);
    assert(firstVisiblePage <= lastPossibleVisiblePage);
    if (firstVisiblePage === lastPossibleVisiblePage) {
      return firstVisiblePage;
    }

    let mostVisiblePage = firstVisiblePage;
    let largestIntersection = 0;

    for (let i = firstVisiblePage; i < lastPossibleVisiblePage + 1; i++) {
      const pageArea =
          this.pageDimensions_[i].width * this.pageDimensions_[i].height;

      // TODO(thestig): check whether we can remove this check.
      if (pageArea <= 0) {
        continue;
      }

      const pageIntersectionArea =
          getIntersectionArea(this.pageDimensions_[i], viewportRect) / pageArea;

      if (pageIntersectionArea > largestIntersection) {
        mostVisiblePage = i;
        largestIntersection = pageIntersectionArea;
      }
    }

    return mostVisiblePage;
  }

  /**
   * Compute the zoom level for fit-to-page, fit-to-width or fit-to-height.
   * At least one of {fitWidth, fitHeight} must be true.
   * @param {!Size} pageDimensions The dimensions of a given page in px.
   * @param {boolean} fitWidth Whether the whole width of the page needs to be
   *     in the viewport.
   * @param {boolean} fitHeight Whether the whole height of the page needs to be
   *     in the viewport.
   * @return {number} The internal zoom to set
   * @private
   */
  computeFittingZoom_(pageDimensions, fitWidth, fitHeight) {
    assert(
        fitWidth || fitHeight,
        'Invalid parameters. At least one of fitWidth and fitHeight must be ' +
            'true.');

    // First compute the zoom without scrollbars.
    let zoom = this.computeFittingZoomGivenDimensions_(
        fitWidth, fitHeight, this.window_.offsetWidth,
        this.window_.offsetHeight, pageDimensions.width, pageDimensions.height);

    // Check if there needs to be any scrollbars.
    const needsScrollbars = this.documentNeedsScrollbars(zoom);

    // If the document fits, just return the zoom.
    if (!needsScrollbars.horizontal && !needsScrollbars.vertical) {
      return zoom;
    }

    const zoomedDimensions = this.getZoomedDocumentDimensions_(zoom);

    // Check if adding a scrollbar will result in needing the other scrollbar.
    const scrollbarWidth = this.scrollbarWidth_;
    if (needsScrollbars.horizontal &&
        zoomedDimensions.height > this.window_.offsetHeight - scrollbarWidth) {
      needsScrollbars.vertical = true;
    }
    if (needsScrollbars.vertical &&
        zoomedDimensions.width > this.window_.offsetWidth - scrollbarWidth) {
      needsScrollbars.horizontal = true;
    }

    // Compute available window space.
    const windowWithScrollbars = {
      width: this.window_.offsetWidth,
      height: this.window_.offsetHeight,
    };
    if (needsScrollbars.horizontal) {
      windowWithScrollbars.height -= scrollbarWidth;
    }
    if (needsScrollbars.vertical) {
      windowWithScrollbars.width -= scrollbarWidth;
    }

    // Recompute the zoom.
    zoom = this.computeFittingZoomGivenDimensions_(
        fitWidth, fitHeight, windowWithScrollbars.width,
        windowWithScrollbars.height, pageDimensions.width,
        pageDimensions.height);

    return this.zoomManager_.internalZoomComponent(zoom);
  }

  /**
   * Compute a zoom level given the dimensions to fit and the actual numbers
   * in those dimensions.
   * @param {boolean} fitWidth Whether to constrain the page width to the
   *     window.
   * @param {boolean} fitHeight Whether to constrain the page height to the
   *     window.
   * @param {number} windowWidth Width of the window in px.
   * @param {number} windowHeight Height of the window in px.
   * @param {number} pageWidth Width of the page in px.
   * @param {number} pageHeight Height of the page in px.
   * @return {number} The internal zoom to set
   * @private
   */
  computeFittingZoomGivenDimensions_(
      fitWidth, fitHeight, windowWidth, windowHeight, pageWidth, pageHeight) {
    // Assumes at least one of {fitWidth, fitHeight} is set.
    let zoomWidth;
    let zoomHeight;

    if (fitWidth) {
      zoomWidth = windowWidth / pageWidth;
    }

    if (fitHeight) {
      zoomHeight = windowHeight / pageHeight;
    }

    let zoom;
    if (!fitWidth && fitHeight) {
      zoom = zoomHeight;
    } else if (fitWidth && !fitHeight) {
      zoom = zoomWidth;
    } else {
      // Assume fitWidth && fitHeight
      zoom = Math.min(zoomWidth, zoomHeight);
    }

    return Math.max(zoom, 0);
  }

  /** Zoom the viewport so that the page width consumes the entire viewport. */
  fitToWidth() {
    this.mightZoom_(() => {
      this.fittingType_ = FittingType.FIT_TO_WIDTH;
      if (!this.documentDimensions_) {
        return;
      }
      // When computing fit-to-width, the maximum width of a page in the
      // document is used, which is equal to the size of the document width.
      this.setZoomInternal_(
          this.computeFittingZoom_(this.documentDimensions_, true, false));
      this.updateViewport_();
    });
  }

  /**
   * Zoom the viewport so that the page height consumes the entire viewport.
   * @param {boolean} scrollToTopOfPage Set to true if the viewport should be
   *     scrolled to the top of the current page. Set to false if the viewport
   *     should remain at the current scroll position.
   * @private
   */
  fitToHeightInternal_(scrollToTopOfPage) {
    this.mightZoom_(() => {
      this.fittingType_ = FittingType.FIT_TO_HEIGHT;
      if (!this.documentDimensions_) {
        return;
      }
      const page = this.getMostVisiblePage();
      // When computing fit-to-height, the maximum height of the current page
      // is used.
      const dimensions = {
        width: 0,
        height: this.pageDimensions_[page].height,
      };
      this.setZoomInternal_(this.computeFittingZoom_(dimensions, false, true));
      if (scrollToTopOfPage) {
        this.position = {
          x: 0,
          y: this.pageDimensions_[page].y * this.getZoom(),
        };
      }
      this.updateViewport_();
    });
  }

  /** Zoom the viewport so that the page height consumes the entire viewport. */
  fitToHeight() {
    this.fitToHeightInternal_(true);
  }

  /**
   * Zoom the viewport so that a page consumes as much as possible of the it.
   * @param {boolean} scrollToTopOfPage Whether the viewport should be scrolled
   *     to the top of the current page. If false, the viewport will remain at
   *     the current scroll position.
   * @private
   */
  fitToPageInternal_(scrollToTopOfPage) {
    this.mightZoom_(() => {
      this.fittingType_ = FittingType.FIT_TO_PAGE;
      if (!this.documentDimensions_) {
        return;
      }
      const page = this.getMostVisiblePage();
      // Fit to the current page's height and the widest page's width.
      const dimensions = {
        width: this.documentDimensions_.width,
        height: this.pageDimensions_[page].height,
      };
      this.setZoomInternal_(this.computeFittingZoom_(dimensions, true, true));
      if (scrollToTopOfPage) {
        this.position = {
          x: 0,
          y: this.pageDimensions_[page].y * this.getZoom(),
        };
      }
      this.updateViewport_();
    });
  }

  /**
   * Zoom the viewport so that a page consumes the entire viewport. Also scrolls
   * the viewport to the top of the current page.
   */
  fitToPage() {
    this.fitToPageInternal_(true);
  }

  /** Zoom the viewport to the default zoom. */
  fitToNone() {
    this.mightZoom_(() => {
      this.fittingType_ = FittingType.NONE;
      if (!this.documentDimensions_) {
        return;
      }
      this.setZoomInternal_(Math.min(
          this.defaultZoom_,
          this.computeFittingZoom_(this.documentDimensions_, true, false)));
      this.updateViewport_();
    });
  }

  /** Zoom out to the next predefined zoom level. */
  zoomOut() {
    this.mightZoom_(() => {
      this.fittingType_ = FittingType.NONE;
      let nextZoom = this.presetZoomFactors_[0];
      for (let i = 0; i < this.presetZoomFactors_.length; i++) {
        if (this.presetZoomFactors_[i] < this.internalZoom_) {
          nextZoom = this.presetZoomFactors_[i];
        }
      }
      this.setZoomInternal_(nextZoom);
      this.updateViewport_();
    });
  }

  /** Zoom in to the next predefined zoom level. */
  zoomIn() {
    this.mightZoom_(() => {
      this.fittingType_ = FittingType.NONE;
      const maxZoomIndex = this.presetZoomFactors_.length - 1;
      let nextZoom = this.presetZoomFactors_[maxZoomIndex];
      for (let i = maxZoomIndex; i >= 0; i--) {
        if (this.presetZoomFactors_[i] > this.internalZoom_) {
          nextZoom = this.presetZoomFactors_[i];
        }
      }
      this.setZoomInternal_(nextZoom);
      this.updateViewport_();
    });
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  pageUpDownSpaceHandler_(e) {
    const direction =
        e.key === 'PageUp' || (e.key === ' ' && e.shiftKey) ? -1 : 1;
    // Go to the previous/next page if we are fit-to-page or fit-to-height.
    if (this.isPagedMode_()) {
      direction === 1 ? this.goToNextPage() : this.goToPreviousPage();
      // Since we do the movement of the page.
      e.preventDefault();
    } else if (
        /** @type {!{fromScriptingAPI: (boolean|undefined)}} */ (e)
            .fromScriptingAPI) {
      this.position.y += direction * this.size.height;
    }
  }

  /**
   * @param {!KeyboardEvent} e
   * @param {boolean} formFieldFocused
   * @private
   */
  arrowLeftHandler_(e, formFieldFocused) {
    if (hasKeyModifiers(e)) {
      return;
    }

    // Go to the previous page if there are no horizontal scrollbars and
    // no form field is focused.
    if (!(this.documentHasScrollbars().horizontal || formFieldFocused)) {
      this.goToPreviousPage();
      // Since we do the movement of the page.
      e.preventDefault();
    } else if (
        /** @type {!{fromScriptingAPI: (boolean|undefined)}} */ (e)
            .fromScriptingAPI) {
      this.position.x -= SCROLL_INCREMENT;
    }
  }

  /**
   * @param {!KeyboardEvent} e
   * @param {boolean} formFieldFocused
   * @private
   */
  arrowRightHandler_(e, formFieldFocused) {
    if (hasKeyModifiers(e)) {
      return;
    }

    // Go to the next page if there are no horizontal scrollbars and no
    // form field is focused.
    if (!(this.documentHasScrollbars().horizontal || formFieldFocused)) {
      this.goToNextPage();
      // Since we do the movement of the page.
      e.preventDefault();
    } else if (
        /** @type {!{fromScriptingAPI: (boolean|undefined)}} */ (e)
            .fromScriptingAPI) {
      this.position.x += SCROLL_INCREMENT;
    }
  }

  /**
   * @param {!KeyboardEvent} e
   * @param {boolean} formFieldFocused
   * @private
   */
  arrowUpDownHandler_(e, formFieldFocused) {
    if (hasKeyModifiers(e)) {
      return;
    }

    // Go to the previous/next page if Presentation mode is on and no form field
    // is focused.
    if (!(document.fullscreenElement === null || formFieldFocused)) {
      e.key === 'ArrowDown' ? this.goToNextPage() : this.goToPreviousPage();
      e.preventDefault();
    } else if (
        /** @type {!{fromScriptingAPI: (boolean|undefined)}} */ (e)
            .fromScriptingAPI) {
      const direction = e.key === 'ArrowDown' ? 1 : -1;
      this.position.y += direction * SCROLL_INCREMENT;
    }
  }

  /**
   * Handle certain directional key events.
   * @param {!KeyboardEvent} e the event to handle.
   * @param {boolean} formFieldFocused Whether a form field is currently
   *     focused.
   * @return {boolean} Whether the event was handled.
   */
  handleDirectionalKeyEvent(e, formFieldFocused) {
    switch (e.key) {
      case ' ':
      case 'PageUp':
      case 'PageDown':
        this.pageUpDownSpaceHandler_(e);
        return true;
      case 'ArrowLeft':
        this.arrowLeftHandler_(e, formFieldFocused);
        return true;
      case 'ArrowDown':
      case 'ArrowUp':
        this.arrowUpDownHandler_(e, formFieldFocused);
        return true;
      case 'ArrowRight':
        this.arrowRightHandler_(e, formFieldFocused);
        return true;
      default:
        return false;
    }
  }

  /**
   * Go to the next page. If the document is in two-up view, go to the left page
   * of the next row. Public for tests.
   */
  goToNextPage() {
    const currentPage = this.getMostVisiblePage();
    const nextPageOffset =
        (this.twoUpViewEnabled() && currentPage % 2 === 0) ? 2 : 1;
    this.goToPage(currentPage + nextPageOffset);
  }

  /**
   * Go to the previous page. If the document is in two-up view, go to the left
   * page of the previous row. Public for tests.
   */
  goToPreviousPage() {
    const currentPage = this.getMostVisiblePage();
    let previousPageOffset = -1;

    if (this.twoUpViewEnabled()) {
      previousPageOffset = (currentPage % 2 === 0) ? -2 : -3;
    }

    this.goToPage(currentPage + previousPageOffset);
  }

  /**
   * Go to the given page index.
   * @param {number} page the index of the page to go to. zero-based.
   */
  goToPage(page) {
    this.goToPageAndXY(page, 0, 0);
  }

  /**
   * Go to the given y position in the given page index.
   * @param {number} page the index of the page to go to. zero-based.
   * @param {number|undefined} x the x position in the page to go to.
   * @param {number|undefined} y the y position in the page to go to.
   */
  goToPageAndXY(page, x, y) {
    this.mightZoom_(() => {
      if (this.pageDimensions_.length === 0) {
        return;
      }
      if (page < 0) {
        page = 0;
      }
      if (page >= this.pageDimensions_.length) {
        page = this.pageDimensions_.length - 1;
      }
      const dimensions = this.pageDimensions_[page];

      // If `x` or `y` is not a valid number or specified, then that
      // coordinate of the current viewport position should be retained.
      const currentCoords =
          /** @type {!Point} */ (this.retrieveCurrentScreenCoordinates_());
      if (x === undefined || Number.isNaN(x)) {
        x = currentCoords.x;
      }
      if (y === undefined || Number.isNaN(y)) {
        y = currentCoords.y;
      }

      this.position = {
        x: (dimensions.x + x) * this.getZoom(),
        y: (dimensions.y + y) * this.getZoom()
      };
      this.updateViewport_();
    });
  }

  /**
   * @param {DocumentDimensions} documentDimensions The dimensions of the
   *     document
   */
  setDocumentDimensions(documentDimensions) {
    this.mightZoom_(() => {
      const initialDimensions = !this.documentDimensions_;
      this.documentDimensions_ = documentDimensions;
      this.pageDimensions_ = this.documentDimensions_.pageDimensions;
      if (initialDimensions) {
        this.setZoomInternal_(Math.min(
            this.defaultZoom_,
            this.computeFittingZoom_(this.documentDimensions_, true, false)));
        this.position = {x: 0, y: 0};
      }
      this.contentSizeChanged_();
      this.resize_();
    });
  }

  /**
   * @param {number} page
   * @return {ViewportRect} The bounds for page `page` minus the shadows.
   */
  getPageInsetDimensions(page) {
    const pageDimensions = this.pageDimensions_[page];
    const shadow = PAGE_SHADOW;
    return {
      x: pageDimensions.x + shadow.left,
      y: pageDimensions.y + shadow.top,
      width: pageDimensions.width - shadow.left - shadow.right,
      height: pageDimensions.height - shadow.top - shadow.bottom,
    };
  }

  /**
   * Get the coordinates of the page contents (excluding the page shadow)
   * relative to the screen.
   * @param {number} page The index of the page to get the rect for.
   * @return {!ViewportRect} A rect representing the page in screen coordinates.
   */
  getPageScreenRect(page) {
    if (!this.documentDimensions_) {
      return {x: 0, y: 0, width: 0, height: 0};
    }
    if (page >= this.pageDimensions_.length) {
      page = this.pageDimensions_.length - 1;
    }

    const pageDimensions = this.pageDimensions_[page];

    // Compute the page dimensions minus the shadows.
    const insetDimensions = this.getPageInsetDimensions(page);

    // Compute the x-coordinate of the page within the document.
    // TODO(raymes): This should really be set when the PDF plugin passes the
    // page coordinates, but it isn't yet.
    const x = (this.documentDimensions_.width - pageDimensions.width) / 2 +
        PAGE_SHADOW.left;
    // Compute the space on the left of the document if the document fits
    // completely in the screen.
    const zoom = this.getZoom();
    let spaceOnLeft =
        (this.size.width - this.documentDimensions_.width * zoom) / 2;
    spaceOnLeft = Math.max(spaceOnLeft, 0);

    return {
      x: x * zoom + spaceOnLeft - this.window_.scrollLeft,
      y: insetDimensions.y * zoom - this.window_.scrollTop,
      width: insetDimensions.width * zoom,
      height: insetDimensions.height * zoom
    };
  }

  /**
   * Check if the current fitting type is a paged mode.
   * In a paged mode, page up and page down scroll to the top of the
   * previous/next page and part of the page is under the toolbar.
   * @return {boolean} Whether the current fitting type is a paged mode.
   * @private
   */
  isPagedMode_() {
    return (
        this.fittingType_ === FittingType.FIT_TO_PAGE ||
        this.fittingType_ === FittingType.FIT_TO_HEIGHT);
  }

  /**
   * Retrieves the in-screen coordinates of the current viewport position.
   * @return {!Point} The current viewport position.
   * @private
   */
  retrieveCurrentScreenCoordinates_() {
    const currentPage = this.getMostVisiblePage();
    const dimension = this.pageDimensions_[currentPage];
    const x = this.position.x / this.getZoom() - dimension.x;
    const y = this.position.y / this.getZoom() - dimension.y;
    return {x: x, y: y};
  }

  /**
   * Handles a navigation request to a destination from the current controller.
   * @param {number} page
   * @param {number|undefined} x The in-screen x coordinate for the destination.
   *     If `x` is undefined, retain current x coordinate value.
   * @param {number|undefined} y The in-screen y coordinate for the destination.
   *     If `y` is undefined, retain current y coordinate value.
   * @param {number} zoom
   */
  handleNavigateToDestination(page, x, y, zoom) {
    if (zoom) {
      this.setZoom(zoom);
    }
    this.goToPageAndXY(page, x, y);
  }

  /**
   * @param {!PartialPoint} point The position to which to scroll the viewport.
   */
  scrollTo(point) {
    let changed = false;
    const newPosition = this.position;
    if (point.x !== undefined && point.x !== newPosition.x) {
      newPosition.x = point.x;
      changed = true;
    }
    if (point.y !== undefined && point.y !== newPosition.y) {
      newPosition.y = point.y;
      changed = true;
    }

    if (changed) {
      this.position = newPosition;
    }
  }

  /** @param {!Point} delta The delta by which to scroll the viewport. */
  scrollBy(delta) {
    const newPosition = this.position;
    newPosition.x += delta.x;
    newPosition.y += delta.y;
    this.scrollTo(newPosition);
  }

  /** Removes all events being tracked from the tracker. */
  resetTracker() {
    if (this.tracker_) {
      this.tracker_.removeAll();
    }
  }

  /**
   * A callback that's called when an update to a pinch zoom is detected.
   * @param {!CustomEvent<!PinchEventDetail>} e the pinch event.
   * @private
   */
  onPinchUpdate_(e) {
    // Throttle number of pinch events to one per frame.
    if (this.sentPinchEvent_) {
      return;
    }

    this.sentPinchEvent_ = true;
    window.requestAnimationFrame(() => {
      this.sentPinchEvent_ = false;
      this.mightZoom_(() => {
        const {direction, center, startScaleRatio} = e.detail;
        this.pinchPhase_ = direction === 'out' ? PinchPhase.UPDATE_ZOOM_OUT :
                                                 PinchPhase.UPDATE_ZOOM_IN;

        const scaleDelta = startScaleRatio / this.prevScale_;
        if (this.firstPinchCenterInFrame_ != null) {
          this.pinchPanVector_ =
              vectorDelta(center, this.firstPinchCenterInFrame_);
        }

        const needsScrollbars =
            this.documentNeedsScrollbars(this.zoomManager_.applyBrowserZoom(
                this.clampZoom_(this.internalZoom_ * scaleDelta)));

        this.pinchCenter_ = center;

        // If there's no horizontal scrolling, keep the content centered so
        // the user can't zoom in on the non-content area.
        // TODO(mcnee) Investigate other ways of scaling when we don't have
        // horizontal scrolling. We want to keep the document centered,
        // but this causes a potentially awkward transition when we start
        // using the gesture center.
        if (!needsScrollbars.horizontal) {
          this.pinchCenter_ = {
            x: this.window_.offsetWidth / 2,
            y: this.window_.offsetHeight / 2
          };
        } else if (this.keepContentCentered_) {
          this.oldCenterInContent_ =
              this.frameToContent_(this.frameToPluginCoordinate_(center));
          this.keepContentCentered_ = false;
        }

        this.fittingType_ = FittingType.NONE;

        this.setPinchZoomInternal_(
            scaleDelta, this.frameToPluginCoordinate_(center));
        this.updateViewport_();
        this.prevScale_ = /** @type {number} */ (startScaleRatio);
      });
    });
  }

  /**
   * A callback that's called when the end of a pinch zoom is detected.
   * @param {!CustomEvent<!PinchEventDetail>} e the pinch event.
   * @private
   */
  onPinchEnd_(e) {
    // Using rAF for pinch end prevents pinch updates scheduled by rAF getting
    // sent after the pinch end.
    window.requestAnimationFrame(() => {
      this.mightZoom_(() => {
        const {center, startScaleRatio} = e.detail;
        this.pinchPhase_ = PinchPhase.END;
        const scaleDelta = startScaleRatio / this.prevScale_;
        this.pinchCenter_ = /** @type {!Point} */ (center);

        this.setPinchZoomInternal_(
            scaleDelta, this.frameToPluginCoordinate_(center));
        this.updateViewport_();
      });

      this.pinchPhase_ = PinchPhase.NONE;
      this.pinchPanVector_ = null;
      this.pinchCenter_ = null;
      this.firstPinchCenterInFrame_ = null;
    });
  }

  /**
   * A callback that's called when the start of a pinch zoom is detected.
   * @param {!CustomEvent<!PinchEventDetail>} e the pinch event.
   * @private
   */
  onPinchStart_(e) {
    // Disable pinch gestures in Presentation  mode.
    if (document.fullscreenElement !== null) {
      return;
    }

    // We also use rAF for pinch start, so that if there is a pinch end event
    // scheduled by rAF, this pinch start will be sent after.
    window.requestAnimationFrame(() => {
      this.pinchPhase_ = PinchPhase.START;
      this.prevScale_ = 1;
      this.oldCenterInContent_ =
          this.frameToContent_(this.frameToPluginCoordinate_(e.detail.center));

      const needsScrollbars = this.documentNeedsScrollbars(this.getZoom());
      this.keepContentCentered_ = !needsScrollbars.horizontal;
      // We keep track of beginning of the pinch.
      // By doing so we will be able to compute the pan distance.
      this.firstPinchCenterInFrame_ = e.detail.center;
    });
  }

  /** @return {!GestureDetector} */
  getGestureDetectorForTesting() {
    return this.gestureDetector_;
  }
}

/**
 * Enumeration of pinch states.
 * This should match PinchPhase enum in pdf/pdf_view_plugin_base.cc.
 * @enum {number}
 */
export const PinchPhase = {
  NONE: 0,
  START: 1,
  UPDATE_ZOOM_OUT: 2,
  UPDATE_ZOOM_IN: 3,
  END: 4,
};

/**
 * The increment to scroll a page by in pixels when up/down/left/right arrow
 * keys are pressed. Usually we just let the browser handle scrolling on the
 * window when these keys are pressed but in certain cases we need to simulate
 * these events.
 * @type {number}
 */
const SCROLL_INCREMENT = 40;

/**
 * The width of the page shadow around pages in pixels.
 * @type {!{top: number, bottom: number, left: number, right: number}}
 */
export const PAGE_SHADOW = {
  top: 3,
  bottom: 7,
  left: 5,
  right: 5
};
