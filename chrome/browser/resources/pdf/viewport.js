// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {$, hasKeyModifiers, isRTL} from 'chrome://resources/js/util.m.js';

import {FittingType, Point} from './constants.js';
import {Gesture, GestureDetector, PinchEventDetail} from './gesture_detector.js';
import {UnseasonedPdfPluginElement} from './internal_plugin.js';
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
 *   direction: number,
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

// TODO(crbug.com/1276456): Would Viewport be better as a Polymer element?
export class Viewport {
  /**
   * @param {!HTMLElement} container The element which contains the scrollable
   *     content.
   * @param {!HTMLDivElement} sizer The element which represents the size of the
   *     scrollable content in the viewport
   * @param {!HTMLDivElement} content The element which is the parent of the
   *     plugin in the viewer.
   * @param {number} scrollbarWidth The width of scrollbars on the page
   * @param {number} defaultZoom The default zoom level.
   */
  constructor(container, sizer, content, scrollbarWidth, defaultZoom) {
    /** @private {!HTMLElement} */
    this.window_ = container;

    /** @private {!ScrollContent} */
    this.scrollContent_ =
        new ScrollContent(this.window_, sizer, content, scrollbarWidth);

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

    /** @private {boolean} */
    this.smoothScrolling_ = false;

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
    this.gestureDetector_ = new GestureDetector(content);

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
    this.gestureDetector_.getEventTarget().addEventListener(
        'wheel',
        e => this.onWheel_(
            /** @type {!CustomEvent<!PinchEventDetail>} */ (e)));

    // Set to a default zoom manager - used in tests.
    this.setZoomManager(new InactiveZoomManager(this.getZoom.bind(this), 1));

    // Print Preview
    if (this.window_ === document.documentElement ||
        // Necessary check since during testing a fake DOM element is used.
        !(this.window_ instanceof HTMLElement)) {
      window.addEventListener('scroll', this.updateViewport_.bind(this));
      this.scrollContent_.setEventTarget(window);
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
      this.scrollContent_.setEventTarget(this.window_);
      const resizeObserver = new ResizeObserver(_ => this.resizeWrapper_());
      const target = this.window_.parentElement;
      assert(target.id === 'main');
      resizeObserver.observe(target);
    }

    document.body.addEventListener(
        'change-zoom', e => this.setZoom(e.detail.zoom));
  }

  /**
   * Sets whether the viewport is in Presentation mode.
   * @param {boolean} enabled
   */
  setPresentationMode(enabled) {
    assert((document.fullscreenElement !== null) === enabled);
    this.gestureDetector_.setPresentationMode(enabled);
  }

  /**
   * Sets the contents of the viewport, scrolling within the viewport's window.
   * @param {?Node} content The new viewport contents, or null to clear the
   *     viewport.
   */
  setContent(content) {
    this.scrollContent_.setContent(content);
  }

  /**
   * Sets the contents of the viewport, scrolling within the content's window.
   * @param {!UnseasonedPdfPluginElement} content The new viewport contents.
   */
  setRemoteContent(content) {
    this.scrollContent_.setRemoteContent(content);
  }

  /**
   * Synchronizes scroll position from remote content.
   * @param {!Point} position
   */
  syncScrollFromRemote(position) {
    this.scrollContent_.syncScrollFromRemote(position);
  }

  /**
   * Receives acknowledgment of scroll position synchronized to remote content.
   * @param {!Point} position
   */
  ackScrollToRemote(position) {
    this.scrollContent_.ackScrollToRemote(position);
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
      this.scrollContent_.setSize(
          zoomedDimensions.width, zoomedDimensions.height);
    }
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
    return {
      x: this.scrollContent_.scrollLeft,
      y: this.scrollContent_.scrollTop,
    };
  }

  /**
   * Scroll the viewport to the specified position.
   * @param {!Point} position The position to scroll to.
   * @param {boolean} isSmooth Whether to scroll smoothly.
   */
  setPosition(position, isSmooth = false) {
    this.scrollContent_.scrollTo(position.x, position.y, isSmooth);
  }

  /** @return {!Size} The size of the viewport. */
  get size() {
    return {
      width: this.window_.offsetWidth,
      height: this.window_.offsetHeight,
    };
  }

  /**
   * Gets the content size.
   * @return {!Size}
   */
  get contentSize() {
    return this.scrollContent_.size;
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
    this.setPosition({
      x: currentScrollPos.x * zoom,
      y: currentScrollPos.y * zoom,
    });
  }

  /**
   * Sets the zoom of the viewport.
   * Same as setZoomInternal_ but for pinch zoom we have some more operations.
   * @param {number} scaleDelta The zoom delta.
   * @param {!Point} center The pinch center in plugin coordinates.
   * @private
   */
  setPinchZoomInternal_(scaleDelta, center) {
    assert(
        this.allowedToChangeZoom_,
        'Called Viewport.setPinchZoomInternal_ without calling ' +
            'Viewport.mightZoom_.');
    this.internalZoom_ = this.clampZoom_(this.internalZoom_ * scaleDelta);

    assert(this.oldCenterInContent_);
    const delta = vectorDelta(
        /** @type {!Point} */ (this.oldCenterInContent_),
        this.pluginToContent_(center));

    // Record the scroll position (relative to the pinch center).
    const zoom = this.getZoom();
    const currentScrollPos = {
      x: this.position.x - delta.x * zoom,
      y: this.position.y - delta.y * zoom
    };

    this.contentSizeChanged_();
    // Scroll to the scaled scroll position.
    this.setPosition(currentScrollPos);
  }

  /**
   *  Converts a point from plugin to content coordinates.
   *  @param {!Point} pluginPoint The plugin coordinates.
   *  @return {!Point} The content coordinates.
   *  @private
   */
  pluginToContent_(pluginPoint) {
    // TODO(mcnee) Add a helper Point class to avoid duplicating operations
    // on plain {x,y} objects.
    const zoom = this.getZoom();
    return {
      x: (pluginPoint.x + this.position.x) / zoom,
      y: (pluginPoint.y + this.position.y) / zoom
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
      this.setPosition({
        x: currentScrollPos.x * newZoom,
        y: currentScrollPos.y * newZoom,
      });
      this.updateViewport_();
    });
  }

  /**
   * Gets the width of scrollbars in the viewport in pixels.
   * @return {number}
   */
  get scrollbarWidth() {
    return this.scrollContent_.scrollbarWidth;
  }

  /**
   * Gets the width of overlay scrollbars in the viewport in pixels, or 0 if not
   * using overlay scrollbars.
   * @return {number}
   */
  get overlayScrollbarWidth() {
    return this.scrollContent_.overlayScrollbarWidth;
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
    const scrollbarWidth = this.scrollContent_.scrollbarWidth;
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
        this.setPosition({
          x: 0,
          y: this.pageDimensions_[page].y * this.getZoom(),
        });
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
        this.setPosition({
          x: 0,
          y: this.pageDimensions_[page].y * this.getZoom(),
        });
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
   * @param {boolean} formFieldFocused
   * @private
   */
  pageUpDownSpaceHandler_(e, formFieldFocused) {
    // Avoid scrolling if the space key is down while a form field is focused
    // on since the user might be typing space into the field.
    if (formFieldFocused && e.key === ' ') {
      this.window_.dispatchEvent(new CustomEvent('scroll-avoided-for-testing'));
      return;
    }

    const isDown = e.key === 'PageDown' || (e.key === ' ' && !e.shiftKey);
    // Go to the previous/next page if we are fit-to-page or fit-to-height.
    if (this.isPagedMode_()) {
      isDown ? this.goToNextPage() : this.goToPreviousPage();
      // Since we do the movement of the page.
      e.preventDefault();
    } else if (isCrossFrameKeyEvent(e)) {
      // Web scrolls by a fraction of the viewport height. Use the same
      // fractional value as `cc::kMinFractionToStepWhenPaging` in
      // cc/input/scroll_utils.h. The values must be kept in sync.
      const MIN_FRACTION_TO_STEP_WHEN_PAGING = 0.875;
      const scrollOffset = (isDown ? 1 : -1) * this.size.height *
          MIN_FRACTION_TO_STEP_WHEN_PAGING;
      this.setPosition(
          {
            x: this.position.x,
            y: this.position.y + scrollOffset,
          },
          this.smoothScrolling_);
    }

    this.window_.dispatchEvent(new CustomEvent('scroll-proceeded-for-testing'));
  }

  /**
   * @param {!KeyboardEvent} e
   * @param {boolean} formFieldFocused
   * @private
   */
  arrowLeftRightHandler_(e, formFieldFocused) {
    if (formFieldFocused || hasKeyModifiers(e)) {
      return;
    }

    // Go to the previous/next page if there are no horizontal scrollbars.
    const isRight = e.key === 'ArrowRight';
    if (!this.documentHasScrollbars().horizontal) {
      isRight ? this.goToNextPage() : this.goToPreviousPage();
      // Since we do the movement of the page.
      e.preventDefault();
    } else if (isCrossFrameKeyEvent(e)) {
      const scrollOffset = (isRight ? 1 : -1) * SCROLL_INCREMENT;
      this.setPosition(
          {
            x: this.position.x + scrollOffset,
            y: this.position.y,
          },
          this.smoothScrolling_);
    }
  }

  /**
   * @param {!KeyboardEvent} e
   * @param {boolean} formFieldFocused
   * @private
   */
  arrowUpDownHandler_(e, formFieldFocused) {
    if (formFieldFocused || hasKeyModifiers(e)) {
      return;
    }

    // Go to the previous/next page if Presentation mode is on.
    const isDown = e.key === 'ArrowDown';
    if (document.fullscreenElement !== null) {
      isDown ? this.goToNextPage() : this.goToPreviousPage();
      e.preventDefault();
    } else if (isCrossFrameKeyEvent(e)) {
      const scrollOffset = (isDown ? 1 : -1) * SCROLL_INCREMENT;
      this.setPosition({
        x: this.position.x,
        y: this.position.y + scrollOffset,
      });
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
        this.pageUpDownSpaceHandler_(e, formFieldFocused);
        return true;
      case 'PageUp':
      case 'PageDown':
        if (hasKeyModifiers(e)) {
          return false;
        }
        this.pageUpDownSpaceHandler_(e, formFieldFocused);
        return true;
      case 'ArrowLeft':
      case 'ArrowRight':
        this.arrowLeftRightHandler_(e, formFieldFocused);
        return true;
      case 'ArrowDown':
      case 'ArrowUp':
        this.arrowUpDownHandler_(e, formFieldFocused);
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

      this.setPosition({
        x: (dimensions.x + x) * this.getZoom(),
        y: (dimensions.y + y) * this.getZoom(),
      });
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

      // Override layout direction based on isRTL().
      if (this.documentDimensions_.layoutOptions) {
        if (isRTL()) {
          // `base::i18n::TextDirection::RIGHT_TO_LEFT`
          this.documentDimensions_.layoutOptions.direction = 1;
        } else {
          // `base::i18n::TextDirection::LEFT_TO_RIGHT`
          this.documentDimensions_.layoutOptions.direction = 2;
        }
      }

      this.pageDimensions_ = this.documentDimensions_.pageDimensions;
      if (initialDimensions) {
        this.setZoomInternal_(Math.min(
            this.defaultZoom_,
            this.computeFittingZoom_(this.documentDimensions_, true, false)));
        this.setPosition({x: 0, y: 0});
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
      x: x * zoom + spaceOnLeft - this.scrollContent_.scrollLeft,
      y: insetDimensions.y * zoom - this.scrollContent_.scrollTop,
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
   * @param {boolean} isSmooth
   */
  setSmoothScrolling(isSmooth) {
    this.smoothScrolling_ = isSmooth;
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
      this.setPosition(newPosition);
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
   * Dispatches a gesture external to this viewport.
   * @param {!Gesture} gesture The gesture to dispatch.
   */
  dispatchGesture(gesture) {
    this.gestureDetector_.getEventTarget().dispatchEvent(
        new CustomEvent(gesture.type, {detail: gesture.detail}));
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
          this.oldCenterInContent_ = this.pluginToContent_(this.pinchCenter_);
          this.keepContentCentered_ = false;
        }

        this.fittingType_ = FittingType.NONE;

        this.setPinchZoomInternal_(scaleDelta, center);
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
        this.pinchCenter_ = center;

        this.setPinchZoomInternal_(scaleDelta, this.pinchCenter_);
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
      this.oldCenterInContent_ = this.pluginToContent_(e.detail.center);

      const needsScrollbars = this.documentNeedsScrollbars(this.getZoom());
      this.keepContentCentered_ = !needsScrollbars.horizontal;
      // We keep track of beginning of the pinch.
      // By doing so we will be able to compute the pan distance.
      this.firstPinchCenterInFrame_ = e.detail.center;
    });
  }

  /**
   * A callback that's called when a Presentation mode wheel event is detected.
   * @param {!CustomEvent<!PinchEventDetail>} e the pinch event.
   * @private
   */
  onWheel_(e) {
    if (e.detail.direction === 'down') {
      this.goToNextPage();
    } else {
      this.goToPreviousPage();
    }
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
 * Returns whether a keyboard event came from another frame.
 * @param {!KeyboardEvent} keyEvent
 * @return {boolean}
 */
function isCrossFrameKeyEvent(keyEvent) {
  // TODO(crbug.com/1279516): Consider moving these properties to a custom
  // KeyboardEvent subtype, if it doesn't become obsolete entirely.
  const custom =
      /**
       * @type {!{
       *   fromPlugin: (boolean|undefined),
       *   fromScriptingAPI: (boolean|undefined),
       * }}
       */
      (keyEvent);
  return !!custom.fromPlugin || !!custom.fromScriptingAPI;
}

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

/**
 * A wrapper around the viewport's scrollable content. This abstraction isolates
 * details concerning internal vs. external scrolling behavior.
 */
class ScrollContent {
  /**
   * @param {!Element} container The element which contains the scrollable
   *     content.
   * @param {!Element} sizer The element which represents the size of the
   *     scrollable content.
   * @param {!Element} content The element which is the parent of the scrollable
   *     content.
   * @param {number} scrollbarWidth The width of any scrollbars.
   */
  constructor(container, sizer, content, scrollbarWidth) {
    /** @private @const {!Element} */
    this.container_ = container;

    /** @private @const {!Element} */
    this.sizer_ = sizer;

    /** @private {?EventTarget} */
    this.target_ = null;

    /** @private @const {!Element} */
    this.content_ = content;

    /** @private @const {number} */
    this.scrollbarWidth_ = scrollbarWidth;

    /** @private {?UnseasonedPdfPluginElement} */
    this.unseasonedPlugin_ = null;

    /** @private {number} */
    this.width_ = 0;

    /** @private {number} */
    this.height_ = 0;

    /** @private {number} */
    this.scrollLeft_ = 0;

    /** @private {number} */
    this.scrollTop_ = 0;

    /** @private {number} */
    this.unackedScrollsToRemote_ = 0;
  }

  /**
   * Sets the target for dispatching "scroll" events.
   * @param {!EventTarget} target
   */
  setEventTarget(target) {
    this.target_ = target;
  }

  /**
   * Dispatches a "scroll" event.
   */
  dispatchScroll_() {
    this.target_ && this.target_.dispatchEvent(new Event('scroll'));
  }

  /**
   * Sets the contents, switching to scrolling locally.
   * @param {?Node} content The new contents, or null to clear.
   */
  setContent(content) {
    if (content === null) {
      this.sizer_.style.display = 'none';
      return;
    }
    this.attachContent_(content);

    // Switch to local content.
    this.sizer_.style.display = 'block';
    if (!this.unseasonedPlugin_) {
      return;
    }
    this.unseasonedPlugin_ = null;

    // Synchronize remote state to local.
    this.updateSize_();
    this.scrollTo(this.scrollLeft_, this.scrollTop_);
  }

  /**
   * Sets the contents, switching to scrolling remotely.
   * @param {!UnseasonedPdfPluginElement} content The new contents.
   */
  setRemoteContent(content) {
    this.attachContent_(content);

    // Switch to remote content.
    const previousScrollLeft = this.scrollLeft;
    const previousScrollTop = this.scrollTop;
    this.sizer_.style.display = 'none';
    assert(!this.unseasonedPlugin_);
    this.unseasonedPlugin_ = content;

    // Synchronize local state to remote.
    this.updateSize_();
    this.scrollTo(previousScrollLeft, previousScrollTop);
  }

  /**
   * Attaches the contents to the DOM.
   * @param {!Node} content The new contents.
   * @private
   */
  attachContent_(content) {
    // We don't actually replace the content in the DOM, as the controller
    // implementations take care of "removal" in controller-specific ways:
    //
    // 1. Plugin content gets added once, then hidden and revealed using CSS.
    // 2. Ink content gets removed directly from the DOM on unload.
    if (!content.parentNode) {
      this.content_.appendChild(content);
    }
    assert(content.parentNode === this.content_);
  }

  /**
   * Synchronizes scroll position from remote content.
   * @param {!Point} position
   */
  syncScrollFromRemote(position) {
    if (this.unackedScrollsToRemote_ > 0) {
      // Don't overwrite scroll position while scrolls-to-remote are pending.
      // TODO(crbug.com/1246398): Don't need this if we make this synchronous
      // again, by moving more logic to the plugin frame.
      return;
    }

    if (this.scrollLeft_ === position.x && this.scrollTop_ === position.y) {
      // Don't trigger scroll event if scroll position hasn't changed.
      return;
    }

    this.scrollLeft_ = position.x;
    this.scrollTop_ = position.y;
    this.dispatchScroll_();
  }

  /**
   * Receives acknowledgment of scroll position synchronized to remote content.
   * @param {!Point} position
   */
  ackScrollToRemote(position) {
    assert(this.unackedScrollsToRemote_ > 0);

    if (--this.unackedScrollsToRemote_ === 0) {
      // Accept remote adjustment when there are no pending scrolls-to-remote.
      this.scrollLeft_ = position.x;
      this.scrollTop_ = position.y;
    }

    this.dispatchScroll_();
  }

  /** @return {number} */
  get scrollbarWidth() {
    return this.scrollbarWidth_;
  }

  /** @return {number} */
  get overlayScrollbarWidth() {
    let overlayScrollbarWidth = 0;

    // TODO(crbug.com/1286009): Support overlay scrollbars on all platforms.
    // <if expr="is_macosx">
    overlayScrollbarWidth = 16;
    // </if>
    // <if expr="not is_macosx">
    if (this.unseasonedPlugin_) {
      overlayScrollbarWidth = this.scrollbarWidth_;
    }
    // </if>

    return overlayScrollbarWidth;
  }

  /**
   * Gets the content size.
   * @return {!Size}
   */
  get size() {
    return {
      width: this.width_,
      height: this.height_,
    };
  }

  /**
   * Sets the content size.
   * @param {number} width
   * @param {number} height
   */
  setSize(width, height) {
    this.width_ = width;
    this.height_ = height;
    this.updateSize_();
  }

  /** @private */
  updateSize_() {
    if (this.unseasonedPlugin_) {
      this.unseasonedPlugin_.postMessage({
        type: 'updateSize',
        width: this.width_,
        height: this.height_,
      });
    } else {
      this.sizer_.style.width = `${this.width_}px`;
      this.sizer_.style.height = `${this.height_}px`;
    }
  }

  /**
   * Gets the scroll offset from the left edge.
   * @return {number}
   */
  get scrollLeft() {
    return this.unseasonedPlugin_ ? this.scrollLeft_ :
                                    this.container_.scrollLeft;
  }

  /**
   * Gets the scroll offset from the top edge.
   * @return {number}
   */
  get scrollTop() {
    return this.unseasonedPlugin_ ? this.scrollTop_ : this.container_.scrollTop;
  }

  /**
   * Scrolls to the given coordinates.
   * @param {number} x
   * @param {number} y
   * @param {boolean} isSmooth Whether to scroll smoothly.
   */
  scrollTo(x, y, isSmooth = false) {
    if (this.unseasonedPlugin_) {
      // TODO(crbug.com/1277228): Can get NaN if zoom calculations divide by 0.
      x = Number.isNaN(x) ? 0 : x;
      y = Number.isNaN(y) ? 0 : y;

      // Clamp coordinates to scroll limits. Note that the order of min() and
      // max() operations is significant, as each "maximum" can be negative.
      const maxX = this.maxScroll_(
          this.width_, this.container_.clientWidth,
          this.height_ > this.container_.clientHeight);
      const maxY = this.maxScroll_(
          this.height_, this.container_.clientHeight,
          this.width_ > this.container_.clientWidth);

      if (this.container_.dir === 'rtl') {
        // Right-to-left. If `maxX` > 0, clamp to [-maxX, 0]. Else set to 0.
        x = Math.min(Math.max(-maxX, x), 0);
      } else {
        // Left-to-right. If `maxX` > 0, clamp to [0, maxX]. Else set to 0.
        x = Math.max(0, Math.min(x, maxX));
      }
      // If `maxY` > 0, clamp to [0, maxY]. Else set to 0.
      y = Math.max(0, Math.min(y, maxY));

      // To match the DOM's scrollTo() behavior, update the scroll position
      // immediately, but fire the scroll event later (when the remote side
      // triggers `ackScrollToRemote()`).
      this.scrollLeft_ = x;
      this.scrollTop_ = y;

      ++this.unackedScrollsToRemote_;
      this.unseasonedPlugin_.postMessage({
        type: 'syncScrollToRemote',
        x: this.scrollLeft_,
        y: this.scrollTop_,
        isSmooth: isSmooth,
      });
    } else {
      this.container_.scrollTo(x, y);
    }
  }

  /**
   * Computes maximum scroll position.
   * @param {number} maxContent The maximum content dimension.
   * @param {number} maxContainer The maximum container dimension.
   * @param {boolean} hasScrollbar Whether to compensate for a scrollbar.
   * @return {number}
   * @private
   */
  maxScroll_(maxContent, maxContainer, hasScrollbar) {
    if (hasScrollbar) {
      maxContainer -= this.scrollbarWidth_;
    }

    // This may return a negative value, which is fine because scroll positions
    // are clamped to a minimum of 0.
    return maxContent - maxContainer;
  }
}
