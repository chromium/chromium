// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {hasKeyModifiers, isRTL} from 'chrome://resources/js/util.js';

import type {ExtendedKeyEvent, Point, Rect} from './constants.js';
import {FittingType} from './constants.js';
import type {Gesture, PinchEventDetail} from './gesture_detector.js';
import {GestureDetector} from './gesture_detector.js';
import type {PdfPluginElement} from './internal_plugin.js';
import {SwipeDetector, SwipeDirection} from './swipe_detector.js';
import type {ZoomManager} from './zoom_manager.js';
import {InactiveZoomManager} from './zoom_manager.js';

export interface ViewportRect {
  x: number;
  y: number;
  width: number;
  height: number;
}

export interface DocumentDimensions {
  width: number;
  height: number;
  pageDimensions: ViewportRect[];
  layoutOptions?: LayoutOptions;
}

export interface LayoutOptions {
  direction: number;
  defaultPageOrientation: number;
  twoUpViewEnabled: boolean;
}

export interface Size {
  width: number;
  height: number;
}

interface FitToPageParams {
  page?: number;
  scrollToTop?: boolean;
}

interface FitToHeightParams {
  page?: number;
  viewPosition?: number;
}

interface FitToBoundingBoxParams {
  boundingBox: Rect;
  page: number;
}

interface FitToBoundingBoxDimensionParams extends FitToBoundingBoxParams {
  viewPosition?: number;
  fitToWidth: boolean;
}

type FitToWidthParams = FitToHeightParams;
type FittingTypeParams = FitToPageParams|FitToHeightParams|FitToWidthParams|
    FitToBoundingBoxParams|FitToBoundingBoxDimensionParams;

/** @return The area of the intersection of the rects */
function getIntersectionArea(rect1: ViewportRect, rect2: ViewportRect): number {
  const left = Math.max(rect1.x, rect2.x);
  const top = Math.max(rect1.y, rect2.y);
  const right = Math.min(rect1.x + rect1.width, rect2.x + rect2.width);
  const bottom = Math.min(rect1.y + rect1.height, rect2.y + rect2.height);

  if (left >= right || top >= bottom) {
    return 0;
  }

  return (right - left) * (bottom - top);
}

/** @return The vector between the two points. */
function vectorDelta(p1: Point, p2: Point): Point {
  return {x: p2.x - p1.x, y: p2.y - p1.y};
}

type HtmlElementWithExtras = HTMLElement&{
  scrollCallback(): void,
  resizeCallback(): void,
};

// TODO(crbug.com/40808900): Would Viewport be better as a Polymer element?
export class Viewport {
  private window_: HTMLElement;
  private scrollContent_: ScrollContent;
  private defaultZoom_: number;

  private viewportChangedCallback_: () => void;
  private beforeZoomCallback_: () => void;
  private afterZoomCallback_: () => void;
  private userInitiatedCallback_: (userInitiated: boolean) => void;

  private allowedToChangeZoom_: boolean = false;
  private internalZoom_: number = 1;

  /**
   * Zoom state used to change zoom and fitting type to what it was
   * originally when saved.
   */
  private savedZoom_: number|null = null;
  private savedFittingType_: FittingType|null = null;

  /**
   * Predefined zoom factors to be used when zooming in/out. These are in
   * ascending order.
   */
  private presetZoomFactors_: number[] = [];

  private zoomManager_: ZoomManager|null = null;
  private documentDimensions_: DocumentDimensions|null = null;
  private pageDimensions_: ViewportRect[] = [];
  private fittingType_: FittingType = FittingType.NONE;
  private prevScale_: number = 1;
  private smoothScrolling_: boolean = false;
  private pinchPhase_: PinchPhase = PinchPhase.NONE;
  private pinchPanVector_: Point|null = null;
  private pinchCenter_: Point|null = null;
  private firstPinchCenterInFrame_: Point|null = null;
  private oldCenterInContent_: Point|null = null;
  private keepContentCentered_: boolean = false;
  private tracker_: EventTracker = new EventTracker();
  private gestureDetector_: GestureDetector;
  private swipeDetector_: SwipeDetector;
  private sentPinchEvent_: boolean = false;
  private fullscreenForTesting_: boolean = false;

  /**
   * @param container The element which contains the scrollable content.
   * @param sizer The element which represents the size of the scrollable
   *     content in the viewport
   * @param content The element which is the parent of the plugin in the viewer.
   * @param scrollbarWidth The width of scrollbars on the page
   * @param defaultZoom The default zoom level.
   */
  constructor(
      container: HTMLElement, sizer: HTMLElement, content: HTMLElement,
      scrollbarWidth: number, defaultZoom: number) {
    this.window_ = container;

    this.scrollContent_ =
        new ScrollContent(this.window_, sizer, content, scrollbarWidth);

    this.defaultZoom_ = defaultZoom;

    this.viewportChangedCallback_ = function() {};
    this.beforeZoomCallback_ = function() {};
    this.afterZoomCallback_ = function() {};
    this.userInitiatedCallback_ = function() {};

    this.gestureDetector_ = new GestureDetector(content);

    this.gestureDetector_.getEventTarget().addEventListener(
        'pinchstart',
        e => this.onPinchStart_(e as CustomEvent<PinchEventDetail>));
    this.gestureDetector_.getEventTarget().addEventListener(
        'pinchupdate',
        e => this.onPinchUpdate_(e as CustomEvent<PinchEventDetail>));
    this.gestureDetector_.getEventTarget().addEventListener(
        'pinchend', e => this.onPinchEnd_(e as CustomEvent<PinchEventDetail>));
    this.gestureDetector_.getEventTarget().addEventListener(
        'wheel', e => this.onWheel_(e as CustomEvent<PinchEventDetail>));

    this.swipeDetector_ = new SwipeDetector(content);

    this.swipeDetector_.getEventTarget().addEventListener(
        'swipe', e => this.onSwipe_(e as CustomEvent<SwipeDirection>));

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
      (this.window_ as HtmlElementWithExtras).scrollCallback =
          this.updateViewport_.bind(this);
      window.addEventListener('resize', this.resizeWrapper_.bind(this));
      // The following line is only used in tests, since they expect
      // |resizeCallback| to be called on the mock |window_| object (legacy).
      (this.window_ as HtmlElementWithExtras).resizeCallback =
          this.resizeWrapper_.bind(this);
    } else {
      // Standard PDF viewer
      this.window_.addEventListener('scroll', this.updateViewport_.bind(this));
      this.scrollContent_.setEventTarget(this.window_);
      const resizeObserver = new ResizeObserver(_ => this.resizeWrapper_());
      const target = this.window_.parentElement;
      assert(target!.id === 'main');
      resizeObserver.observe(target!);
    }

    document.body.addEventListener(
        'change-zoom', e => this.setZoom(e.detail.zoom));
  }

  /**
   * Sets whether the viewport is in Presentation mode.
   */
  setPresentationMode(enabled: boolean) {
    assert((document.fullscreenElement !== null) === enabled);
    this.gestureDetector_.setPresentationMode(enabled);
    this.swipeDetector_.setPresentationMode(enabled);
  }

  /**
   * Sets the contents of the viewport, scrolling within the viewport's window.
   * @param content The new viewport contents, or null to clear the viewport.
   */
  setContent(content: Node|null) {
    this.scrollContent_.setContent(content);
  }

  /**
   * Sets the contents of the viewport, scrolling within the content's window.
   * @param content The new viewport contents.
   */
  setRemoteContent(content: PdfPluginElement) {
    this.scrollContent_.setRemoteContent(content);
  }

  /**
   * Synchronizes scroll position from remote content.
   */
  syncScrollFromRemote(position: Point) {
    this.scrollContent_.syncScrollFromRemote(position);
  }

  /**
   * Receives acknowledgment of scroll position synchronized to remote content.
   */
  ackScrollToRemote(position: Point) {
    this.scrollContent_.ackScrollToRemote(position);
  }

  setViewportChangedCallback(viewportChangedCallback: () => void) {
    this.viewportChangedCallback_ = viewportChangedCallback;
  }

  setBeforeZoomCallback(beforeZoomCallback: () => void) {
    this.beforeZoomCallback_ = beforeZoomCallback;
  }

  setAfterZoomCallback(afterZoomCallback: () => void) {
    this.afterZoomCallback_ = afterZoomCallback;
  }

  setUserInitiatedCallback(
      userInitiatedCallback: (userInitiated: boolean) => void) {
    this.userInitiatedCallback_ = userInitiatedCallback;
  }

  /**
   * @return The number of clockwise 90-degree rotations that have been applied.
   */
  getClockwiseRotations(): number {
    const options = this.getLayoutOptions();
    return options ? options.defaultPageOrientation : 0;
  }

  /** @return Whether viewport is in two-up view mode. */
  twoUpViewEnabled(): boolean {
    const options = this.getLayoutOptions();
    return !!options && options.twoUpViewEnabled;
  }

  /**
   * Clamps the zoom factor (or page scale factor) to be within the limits.
   * @param factor The zoom/scale factor.
   * @return The factor clamped within the limits.
   */
  private clampZoom_(factor: number): number {
    assert(this.presetZoomFactors_.length > 0);
    return Math.max(
        this.presetZoomFactors_[0]!,
        Math.min(
            factor,
            this.presetZoomFactors_[this.presetZoomFactors_.length - 1]!));
  }

  /** @param factors Array containing zoom/scale factors. */
  setZoomFactorRange(factors: number[]) {
    assert(factors.length !== 0);
    this.presetZoomFactors_ = factors;
  }

  /**
   * Converts a page position (e.g. the location of a bookmark) to a screen
   * position.
   * @param point The position on `page`.
   * @return The screen position.
   */
  convertPageToScreen(page: number, point: Point): Point {
    const dimensions = this.getPageInsetDimensions(page);

    // width & height are already rotated.
    const height = dimensions.height;
    const width = dimensions.width;

    const matrix = new DOMMatrix();

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
   * @param zoom The zoom to use to compute the scaled dimensions.
   * @return Scaled 'width' and 'height' of the document.
   */
  private getZoomedDocumentDimensions_(zoom: number): Size|null {
    if (!this.documentDimensions_) {
      return null;
    }
    return {
      width: Math.round(this.documentDimensions_.width * zoom),
      height: Math.round(this.documentDimensions_.height * zoom),
    };
  }

  /** @return A dictionary with the 'width'/'height' of the document. */
  getDocumentDimensions(): Size {
    return {
      width: this.documentDimensions_!.width,
      height: this.documentDimensions_!.height,
    };
  }

  /** @return A dictionary carrying layout options from the plugin. */
  getLayoutOptions(): LayoutOptions|undefined {
    return this.documentDimensions_ ? this.documentDimensions_.layoutOptions :
                                      undefined;
  }

  /** @return ViewportRect for the viewport given current zoom. */
  private getViewportRect_(): ViewportRect {
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
      height: this.size.height / zoom,
    };
  }

  /**
   * @param zoom Zoom to compute scrollbars for
   * @return Whether horizontal or vertical scrollbars are needed.
   * Public so tests can call it directly.
   */
  documentNeedsScrollbars(zoom: number):
      {horizontal: boolean, vertical: boolean} {
    const zoomedDimensions = this.getZoomedDocumentDimensions_(zoom);
    if (!zoomedDimensions) {
      return {horizontal: false, vertical: false};
    }

    return {
      horizontal: zoomedDimensions.width > this.window_.offsetWidth,
      vertical: zoomedDimensions.height > this.window_.offsetHeight,
    };
  }

  /**
   * @return Whether horizontal and vertical scrollbars are needed.
   */
  documentHasScrollbars(): {horizontal: boolean, vertical: boolean} {
    return this.documentNeedsScrollbars(this.getZoom());
  }

  /**
   * Helper function called when the zoomed document size changes. Updates the
   * sizer's width and height.
   */
  private contentSizeChanged_() {
    const zoomedDimensions = this.getZoomedDocumentDimensions_(this.getZoom());
    if (zoomedDimensions) {
      this.scrollContent_.setSize(
          zoomedDimensions.width, zoomedDimensions.height);
    }
  }

  /** Called when the viewport should be updated. */
  private updateViewport_() {
    this.viewportChangedCallback_();
  }

  /** Called when the browser window size changes. */
  private resizeWrapper_() {
    this.userInitiatedCallback_(false);
    this.resize_();
    this.userInitiatedCallback_(true);
  }

  /** Called when the viewport size changes. */
  private resize_() {
    // Force fit-to-height when resizing happens as a result of entering full
    // screen mode.
    if (document.fullscreenElement !== null) {
      this.fittingType_ = FittingType.FIT_TO_HEIGHT;
      this.window_.dispatchEvent(
          new CustomEvent('fitting-type-changed-for-testing'));
    }

    if (this.fittingType_ === FittingType.FIT_TO_PAGE) {
      this.fitToPage({scrollToTop: false});
    } else if (this.fittingType_ === FittingType.FIT_TO_WIDTH) {
      this.fitToWidth();
    } else if (this.fittingType_ === FittingType.FIT_TO_HEIGHT) {
      this.fitToHeight();
    } else if (this.internalZoom_ === 0) {
      this.fitToNone();
    } else {
      this.updateViewport_();
    }
  }

  /** @return The scroll position of the viewport. */
  get position(): Point {
    return {
      x: this.scrollContent_.scrollLeft,
      y: this.scrollContent_.scrollTop,
    };
  }

  /**
   * Scroll the viewport to the specified position.
   * @param position The position to scroll to.
   * @param isSmooth Whether to scroll smoothly.
   */
  setPosition(position: Point, isSmooth: boolean = false) {
    this.scrollContent_.scrollTo(position.x, position.y, isSmooth);
  }

  /** @return The size of the viewport. */
  get size(): Size {
    return {
      width: this.window_.offsetWidth,
      height: this.window_.offsetHeight,
    };
  }

  /** Gets the content size. */
  get contentSize(): Size {
    return this.scrollContent_.size;
  }

  /** @return The current zoom. */
  getZoom(): number {
    return this.zoomManager_!.applyBrowserZoom(this.internalZoom_);
  }

  /** @return The preset zoom factors. */
  get presetZoomFactors(): number[] {
    return this.presetZoomFactors_;
  }

  setZoomManager(manager: ZoomManager) {
    this.resetTracker();
    this.zoomManager_ = manager;
    this.tracker_.add(
        this.zoomManager_!.getEventTarget(), 'set-zoom',
        (e: CustomEvent<number>) => this.setZoom(e.detail));
    this.tracker_.add(
        this.zoomManager_!.getEventTarget(), 'update-zoom-from-browser',
        this.updateZoomFromBrowserChange_.bind(this));
  }

  /**
   * @return The phase of the current pinch gesture for the viewport.
   */
  get pinchPhase(): PinchPhase {
    return this.pinchPhase_;
  }

  /**
   * @return The panning caused by the current pinch gesture (as the deltas of
   *     the x and y coordinates).
   */
  get pinchPanVector(): Point|null {
    return this.pinchPanVector_;
  }

  /**
   * @return The coordinates of the center of the current pinch gesture.
   */
  get pinchCenter(): Point|null {
    return this.pinchCenter_;
  }

  /**
   * Used to wrap a function that might perform zooming on the viewport. This is
   * required so that we can notify the plugin that zooming is in progress
   * so that while zooming is taking place it can stop reacting to scroll events
   * from the viewport. This is to avoid flickering.
   */
  private mightZoom_(f: () => void) {
    this.beforeZoomCallback_();
    this.allowedToChangeZoom_ = true;
    f();
    this.allowedToChangeZoom_ = false;
    this.afterZoomCallback_();
    this.zoomManager_!.onPdfZoomChange();
  }

  /**
   * @param currentScrollPos Optional starting position to zoom into. Otherwise,
   *     use the current position.
   */
  private setZoomInternal_(newZoom: number, currentScrollPos?: Point) {
    assert(
        this.allowedToChangeZoom_,
        'Called Viewport.setZoomInternal_ without calling ' +
            'Viewport.mightZoom_.');
    // Record the scroll position (relative to the top-left of the window).
    let zoom = this.getZoom();
    if (!currentScrollPos) {
      currentScrollPos = {
        x: this.position.x / zoom,
        y: this.position.y / zoom,
      };
    }

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
   * @param scaleDelta The zoom delta.
   * @param center The pinch center in plugin coordinates.
   */
  private setPinchZoomInternal_(scaleDelta: number, center: Point) {
    assert(
        this.allowedToChangeZoom_,
        'Called Viewport.setPinchZoomInternal_ without calling ' +
            'Viewport.mightZoom_.');
    this.internalZoom_ = this.clampZoom_(this.internalZoom_ * scaleDelta);

    assert(this.oldCenterInContent_);
    const delta =
        vectorDelta(this.oldCenterInContent_, this.pluginToContent_(center));

    // Record the scroll position (relative to the pinch center).
    const zoom = this.getZoom();
    const currentScrollPos = {
      x: this.position.x - delta.x * zoom,
      y: this.position.y - delta.y * zoom,
    };

    this.contentSizeChanged_();
    // Scroll to the scaled scroll position.
    this.setPosition(currentScrollPos);
  }

  /**
   *  Converts a point from plugin to content coordinates.
   *  @param pluginPoint The plugin coordinates.
   *  @return The content coordinates.
   */
  private pluginToContent_(pluginPoint: Point): Point {
    // TODO(mcnee) Add a helper Point class to avoid duplicating operations
    // on plain {x,y} objects.
    const zoom = this.getZoom();
    return {
      x: (pluginPoint.x + this.position.x) / zoom,
      y: (pluginPoint.y + this.position.y) / zoom,
    };
  }

  /** @param newZoom The zoom level to zoom to. */
  setZoom(newZoom: number) {
    this.fittingType_ = FittingType.NONE;
    this.mightZoom_(() => {
      this.setZoomInternal_(this.clampZoom_(newZoom));
      this.updateViewport_();
    });
  }

  /**
   * Save the current zoom and fitting type.
   */
  saveZoomState() {
    // Fitting to bounding box does not need to be saved, so set the fitting
    // type to none.
    if (this.fittingType_ === FittingType.FIT_TO_BOUNDING_BOX) {
      this.setFittingType(FittingType.NONE);
    }
    this.savedZoom_ = this.internalZoom_;
    this.savedFittingType_ = this.fittingType_;
  }

  /**
   * Set zoom and fitting type to what it was when saved. See saveZoomState().
   */
  restoreZoomState() {
    assert(
        this.savedZoom_ !== null && this.savedFittingType_ !== null,
        'No saved zoom state exists');
    if (this.savedFittingType_ === FittingType.NONE) {
      this.setZoom(this.savedZoom_);
    } else {
      this.setFittingType(this.savedFittingType_);
    }
    this.savedZoom_ = null;
    this.savedFittingType_ = null;
  }

  /** @param e Event containing the old browser zoom. */
  private updateZoomFromBrowserChange_(e: CustomEvent<number>) {
    const oldBrowserZoom = e.detail;
    this.mightZoom_(() => {
      // Record the scroll position (relative to the top-left of the window).
      const oldZoom = oldBrowserZoom * this.internalZoom_;
      const currentScrollPos = {
        x: this.position.x / oldZoom,
        y: this.position.y / oldZoom,
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
   */
  get scrollbarWidth(): number {
    return this.scrollContent_.scrollbarWidth;
  }

  /**
   * Gets the width of overlay scrollbars in the viewport in pixels, or 0 if not
   * using overlay scrollbars.
   */
  get overlayScrollbarWidth(): number {
    return this.scrollContent_.overlayScrollbarWidth;
  }

  /** @return The fitting type the viewport is currently in. */
  get fittingType(): FittingType {
    return this.fittingType_;
  }

  /** @return The y coordinate of the bottom of the given page. */
  private getPageBottom_(index: number): number {
    // Called in getPageAtY_ in a loop that already checks |index| is in bounds.
    return this.pageDimensions_[index]!.y + this.pageDimensions_[index]!.height;
  }

  /**
   * Get the page at a given y position. If there are multiple pages
   * overlapping the given y-coordinate, return the page with the smallest
   * index.
   * @param y The y-coordinate to get the page at.
   * @return The index of a page overlapping the given y-coordinate.
   */
  private getPageAtY_(y: number): number {
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
  }

  /**
   * Return the last page visible in the viewport. Returns the last index of the
   * document if the viewport is below the document.
   * @return The highest index of the pages visible in the viewport.
   */
  private getLastPageInViewport_(viewportRect: ViewportRect): number {
    const pageAtY = this.getPageAtY_(viewportRect.y + viewportRect.height);

    if (!this.twoUpViewEnabled() || pageAtY % 2 === 1 ||
        pageAtY + 1 >= this.pageDimensions_.length) {
      return pageAtY;
    }

    const nextPage = this.pageDimensions_[pageAtY + 1]!;
    return getIntersectionArea(viewportRect, nextPage) > 0 ? pageAtY + 1 :
                                                             pageAtY;
  }

  /** @return Whether |point| (in screen coordinates) is inside a page. */
  isPointInsidePage(point: Point): boolean {
    const zoom = this.getZoom();
    const size = this.size;
    const position = this.position;
    // getPageAtY_() always returns a value in range of pageDimensions_.
    const page = this.getPageAtY_((position.y + point.y) / zoom);
    const pageWidth = this.pageDimensions_[page]!.width * zoom;
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
   * @return The index of the page with the greatest proportion of its area in
   *     the current viewport.
   */
  getMostVisiblePage(): number {
    const viewportRect = this.getViewportRect_();

    // These methods always return a page that is >= 0 and
    // < pageDimensions_.length.
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
          this.pageDimensions_[i]!.width * this.pageDimensions_[i]!.height;

      // TODO(thestig): check whether we can remove this check.
      if (pageArea <= 0) {
        continue;
      }

      const pageIntersectionArea =
          getIntersectionArea(this.pageDimensions_[i]!, viewportRect) /
          pageArea;

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
   * @param pageDimensions The dimensions of a given page in px.
   * @param fitWidth Whether the whole width of the page needs to be in the
   *     viewport.
   * @param fitHeight Whether the whole height of the page needs to be in the
   *     viewport.
   */
  private computeFittingZoom_(
      pageDimensions: Size, fitWidth: boolean, fitHeight: boolean): number {
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
    assert(zoomedDimensions !== null);

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

    return this.zoomManager_!.internalZoomComponent(zoom);
  }

  /**
   * Compute a zoom level given the dimensions to fit and the actual numbers
   * in those dimensions.
   * @param fitWidth Whether to constrain the page width to the window.
   * @param fitHeight Whether to constrain the page height to the window.
   * @param windowWidth Width of the window in px.
   * @param windowHeight Height of the window in px.
   * @param pageWidth Width of the page in px.
   * @param pageHeight Height of the page in px.
   */
  private computeFittingZoomGivenDimensions_(
      fitWidth: boolean, fitHeight: boolean, windowWidth: number,
      windowHeight: number, pageWidth: number, pageHeight: number): number {
    // Assumes at least one of {fitWidth, fitHeight} is set.
    let zoomWidth: number|null = null;
    let zoomHeight: number|null = null;

    if (fitWidth) {
      zoomWidth = windowWidth / pageWidth;
    }

    if (fitHeight) {
      zoomHeight = windowHeight / pageHeight;
    }

    let zoom: number;
    if (!fitWidth && fitHeight) {
      zoom = zoomHeight!;
    } else if (fitWidth && !fitHeight) {
      zoom = zoomWidth!;
    } else {
      // Assume fitWidth && fitHeight
      zoom = Math.min(zoomWidth!, zoomHeight!);
    }

    return Math.max(zoom, 0);
  }

  /**
   * Set the fitting type and fit within the viewport accordingly.
   * @param params Params needed to determine the page, position, and zoom for
   *     certain fitting types.
   */
  setFittingType(fittingType: FittingType, params?: FittingTypeParams) {
    switch (fittingType) {
      case FittingType.FIT_TO_PAGE:
        this.fitToPage(params as FitToPageParams);
        return;
      case FittingType.FIT_TO_WIDTH:
        this.fitToWidth(params as FitToWidthParams);
        return;
      case FittingType.FIT_TO_HEIGHT:
        this.fitToHeight(params as FitToHeightParams);
        return;
      case FittingType.FIT_TO_BOUNDING_BOX:
        this.fitToBoundingBox(params as FitToBoundingBoxParams);
        return;
      case FittingType.FIT_TO_BOUNDING_BOX_WIDTH:
        this.fitToBoundingBoxDimension(
            params as FitToBoundingBoxDimensionParams);
        return;
      case FittingType.FIT_TO_BOUNDING_BOX_HEIGHT:
        this.fitToBoundingBoxDimension(
            params as FitToBoundingBoxDimensionParams);
        return;
      case FittingType.NONE:
        // Does not take any params.
        this.fittingType_ = fittingType;
        return;
      default:
        assertNotReached('Invalid fittingType');
    }
  }

  /**
   * Zoom the viewport so that the page width consumes the entire viewport.
   * @param params Optional params that may contain the page to scroll to the
   *     top of. Otherwise, remain at the current scroll position. Params may
   *     also contain the y offset from the top of the page.
   */
  fitToWidth(params?: FitToWidthParams) {
    this.mightZoom_(() => {
      this.fittingType_ = FittingType.FIT_TO_WIDTH;
      if (!this.documentDimensions_) {
        return;
      }

      const scrollPosition = {
        x: this.position.x / this.getZoom(),
        y: this.position.y / this.getZoom(),
      };

      if (params?.page !== undefined) {
        assert(params.page < this.pageDimensions_.length);
        scrollPosition.y = this.pageDimensions_[params.page]!.y;
      }

      if (params?.viewPosition !== undefined) {
        if (params.page === undefined) {
          // getMostVisiblePage() always returns an index in range of
          // pageDimensions_.
          scrollPosition.y = this.pageDimensions_[this.getMostVisiblePage()]!.y;
        }
        scrollPosition.y += params.viewPosition;
      }

      // When computing fit-to-width, the maximum width of a page in the
      // document is used, which is equal to the size of the document width.
      this.setZoomInternal_(
          this.computeFittingZoom_(this.documentDimensions_, true, false),
          scrollPosition);
      this.updateViewport_();
    });
  }

  /**
   * Zoom the viewport so that the page height consumes the entire viewport.
   * @param params Optional params that may contain the page to scroll to the
   *     top of. Otherwise, remain at the current scroll position. Params may
   *     also contain the x offset from the left of the page.
   */
  fitToHeight(params?: FitToHeightParams) {
    this.mightZoom_(() => {
      this.fittingType_ = FittingType.FIT_TO_HEIGHT;
      if (!this.documentDimensions_) {
        return;
      }

      const scrollPosition = {
        x: this.position.x / this.getZoom(),
        y: this.position.y / this.getZoom(),
      };

      const page =
          params?.page !== undefined ? params.page : this.getMostVisiblePage();
      assert(this.pageDimensions_.length > page);

      if (params?.page !== undefined || document.fullscreenElement !== null) {
        scrollPosition.y = this.pageDimensions_[page]!.y;
      }

      if (params?.viewPosition !== undefined) {
        scrollPosition.x = this.pageDimensions_[page]!.x + params.viewPosition;
      }

      // When computing fit-to-height, the maximum height of the page is used.
      const dimensions = {
        width: 0,
        height: this.pageDimensions_[page]!.height,
      };
      this.setZoomInternal_(
          this.computeFittingZoom_(dimensions, false, true), scrollPosition);
      this.updateViewport_();
    });
  }

  /**
   * Zoom the viewport so that a page consumes as much as of the viewport as
   * possible.
   * @param params Optional params that may contain the page to scroll to the
   *     top of. Also may contain `scrollToTop`, whether to scroll to the top of
   *     the page or not. Defaults to true. Ignored if a page value is provided.
   */
  fitToPage(params?: FitToPageParams) {
    this.mightZoom_(() => {
      this.fittingType_ = FittingType.FIT_TO_PAGE;
      if (!this.documentDimensions_) {
        return;
      }

      const scrollPosition = {
        x: this.position.x / this.getZoom(),
        y: this.position.y / this.getZoom(),
      };

      const page =
          params?.page !== undefined ? params.page : this.getMostVisiblePage();
      assert(this.pageDimensions_.length > page);

      if (params?.page !== undefined || params?.scrollToTop !== false) {
        // Scroll to top of page.
        scrollPosition.x = 0;
        scrollPosition.y = this.pageDimensions_[page]!.y;
      }

      // Fit to the page's height and the widest page's width.
      const dimensions = {
        width: this.documentDimensions_.width,
        height: this.pageDimensions_[page]!.height,
      };
      this.setZoomInternal_(
          this.computeFittingZoom_(dimensions, true, true), scrollPosition);
      this.updateViewport_();
    });
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

  /**
   * Zoom the viewport so that the bounding box of a page consumes the entire
   * viewport.
   * @param params Required params containing the bounding box to fit to and the
   *     page to scroll to.
   */
  fitToBoundingBox(params: FitToBoundingBoxParams) {
    const boundingBox = params.boundingBox;
    // Ignore invalid bounding boxes, which can occur if the plugin fails to
    // give a valid box.
    if (!boundingBox.width || !boundingBox.height) {
      return;
    }

    this.fittingType_ = FittingType.FIT_TO_BOUNDING_BOX;

    // Use the smallest zoom that fits the full bounding box on screen.
    const boundingBoxSize = {
      width: boundingBox.width,
      height: boundingBox.height,
    };

    const zoomFitToWidth =
        this.computeFittingZoom_(boundingBoxSize, true, false);
    const zoomFitToHeight =
        this.computeFittingZoom_(boundingBoxSize, false, true);
    const newZoom = this.clampZoom_(Math.min(zoomFitToWidth, zoomFitToHeight));

    // Calculate the position.
    const pageInsetDimensions = this.getPageInsetDimensions(params.page);
    const viewportSize = this.size;
    const screenPosition: Point = {
      x: pageInsetDimensions.x + boundingBox.x,
      y: pageInsetDimensions.y + boundingBox.y,
    };

    // Center the bounding box in the dimension that isn't fully zoomed in.
    if (newZoom !== zoomFitToWidth) {
      screenPosition.x -=
          ((viewportSize.width / newZoom) - boundingBox.width) / 2;
    }
    if (newZoom !== zoomFitToHeight) {
      screenPosition.y -=
          ((viewportSize.height / newZoom) - boundingBox.height) / 2;
    }

    this.mightZoom_(() => {
      this.setZoomInternal_(newZoom, screenPosition);
    });
  }

  /**
   * If params.viewPosition is defined, use it as the x offset of the given
   * page.
   */
  private getBoundingBoxHeightPosition_(
      params: FitToBoundingBoxDimensionParams, zoomFitToDimension: number,
      newZoom: number): Point {
    const boundingBox = params.boundingBox;
    const pageInsetDimensions = this.getPageInsetDimensions(params.page);
    const screenPosition: Point = {
      x: pageInsetDimensions.x,
      y: pageInsetDimensions.y + boundingBox.y,
    };

    // Center the bounding box in the y dimension if not fully zoomed in.
    if (newZoom !== zoomFitToDimension) {
      screenPosition.y -=
          ((this.size.height / newZoom) - boundingBox.height) / 2;
    }

    if (params.viewPosition !== undefined) {
      screenPosition.x += params.viewPosition;
    }

    return screenPosition;
  }

  /**
   * If params.viewPosition is defined, use it as the y offset of the given
   * page.
   */
  private getBoundingBoxWidthPosition_(
      params: FitToBoundingBoxDimensionParams, zoomFitToDimension: number,
      newZoom: number): Point {
    const boundingBox = params.boundingBox;
    const pageInsetDimensions = this.getPageInsetDimensions(params.page);
    const screenPosition: Point = {
      x: pageInsetDimensions.x + boundingBox.x,
      y: pageInsetDimensions.y,
    };

    // Center the bounding box in the x dimension if not fully zoomed in.
    if (newZoom !== zoomFitToDimension) {
      screenPosition.x -= ((this.size.width / newZoom) - boundingBox.width) / 2;
    }

    if (params.viewPosition !== undefined) {
      screenPosition.y += params.viewPosition;
    }

    return screenPosition;
  }

  /**
   * Zoom the viewport so that the given dimension of the bounding box of a page
   * consumes the entire viewport.
   * @param params Required params containing the bounding box to fit to, the
   *     page to scroll to, and the dimension to fit to. Optionally contains the
   *     offset of the given page.
   */
  fitToBoundingBoxDimension(params: FitToBoundingBoxDimensionParams) {
    const boundingBox = params.boundingBox;
    const fitToWidth = params.fitToWidth;
    // Ignore invalid bounding boxes, which can occur if the plugin fails to
    // give a valid box.
    if (!boundingBox.width || !boundingBox.height) {
      return;
    }

    this.fittingType_ = fitToWidth ? FittingType.FIT_TO_BOUNDING_BOX_WIDTH :
                                     FittingType.FIT_TO_BOUNDING_BOX_HEIGHT;

    const zoomFitToDimension =
        this.computeFittingZoom_(boundingBox, fitToWidth, !fitToWidth);
    const newZoom = this.clampZoom_(zoomFitToDimension);

    const screenPosition = fitToWidth ?
        this.getBoundingBoxWidthPosition_(params, zoomFitToDimension, newZoom) :
        this.getBoundingBoxHeightPosition_(params, zoomFitToDimension, newZoom);

    this.mightZoom_(() => {
      this.setZoomInternal_(newZoom, screenPosition);
    });
  }

  /** Zoom out to the next predefined zoom level. */
  zoomOut() {
    this.mightZoom_(() => {
      this.fittingType_ = FittingType.NONE;
      assert(this.presetZoomFactors.length > 0);
      let nextZoom = this.presetZoomFactors_[0]!;
      for (let i = 0; i < this.presetZoomFactors_.length; i++) {
        if (this.presetZoomFactors_[i]! < this.internalZoom_) {
          nextZoom = this.presetZoomFactors_[i]!;
        }
      }
      this.setZoomInternal_(nextZoom);
      this.updateViewport_();
      this.announceZoom_();
    });
  }

  /** Zoom in to the next predefined zoom level. */
  zoomIn() {
    this.mightZoom_(() => {
      this.fittingType_ = FittingType.NONE;
      assert(this.presetZoomFactors_.length > 0);
      const maxZoomIndex = this.presetZoomFactors_.length - 1;
      let nextZoom = this.presetZoomFactors_[maxZoomIndex]!;
      for (let i = maxZoomIndex; i >= 0; i--) {
        if (this.presetZoomFactors_[i]! > this.internalZoom_) {
          nextZoom = this.presetZoomFactors_[i]!;
        }
      }
      this.setZoomInternal_(nextZoom);
      this.updateViewport_();
      this.announceZoom_();
    });
  }

  /** Announce zoom level for screen readers. */
  private announceZoom_(): void {
    const announcer = getAnnouncerInstance();
    const ariaLabel = loadTimeData.getString('zoomTextInputAriaLabel');
    const zoom = Math.round(100 * this.getZoom());
    announcer.announce(`${ariaLabel}: ${zoom}%`);
  }

  private pageUpDownSpaceHandler_(e: KeyboardEvent, formFieldFocused: boolean) {
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

  private arrowLeftRightHandler_(e: KeyboardEvent, formFieldFocused: boolean) {
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

  private arrowUpDownHandler_(e: KeyboardEvent, formFieldFocused: boolean) {
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
   * @param formFieldFocused Whether a form field is currently focused.
   * @return Whether the event was handled.
   */
  handleDirectionalKeyEvent(e: KeyboardEvent, formFieldFocused: boolean):
      boolean {
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
   * @param page the index of the page to go to. zero-based.
   */
  goToPage(page: number) {
    this.goToPageAndXy(page, 0, 0);
  }

  /**
   * Go to the given y position in the given page index.
   * @param page the index of the page to go to. zero-based.
   */
  goToPageAndXy(page: number, x: number|undefined, y: number|undefined) {
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
      const dimensions = this.pageDimensions_[page]!;

      // If `x` or `y` is not a valid number or specified, then that
      // coordinate of the current viewport position should be retained.
      const currentCoords = this.retrieveCurrentScreenCoordinates_();
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

  setDocumentDimensions(documentDimensions: DocumentDimensions) {
    this.mightZoom_(() => {
      const initialDimensions = !this.documentDimensions_;
      const initialRotations = this.getClockwiseRotations();
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

      if (initialRotations !== this.getClockwiseRotations()) {
        this.announceRotation_();
      }
    });
  }

  /** Announce state of rotation, clockwise, for screen readers. */
  private announceRotation_() {
    const announcer = getAnnouncerInstance();

    const clockwiseRotationsDegrees = this.getClockwiseRotations() * 90;
    const rotationStateLabel = loadTimeData.getString(
        `rotationStateLabel${clockwiseRotationsDegrees}`);
    announcer.announce(rotationStateLabel);
  }

  /** @return The bounds for page `page` minus the shadows. */
  getPageInsetDimensions(page: number): ViewportRect {
    const pageDimensions = this.pageDimensions_[page];
    assert(pageDimensions);
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
   * @param page The index of the page to get the rect for.
   * @return A rect representing the page in screen coordinates.
   */
  getPageScreenRect(page: number): ViewportRect {
    if (!this.documentDimensions_) {
      return {x: 0, y: 0, width: 0, height: 0};
    }
    if (page >= this.pageDimensions_.length) {
      page = this.pageDimensions_.length - 1;
    }

    const pageDimensions = this.pageDimensions_[page]!;

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
    const scrollbarWidth = this.documentHasScrollbars().vertical ?
        this.scrollContent_.scrollbarWidth :
        0;
    let spaceOnLeft = (this.size.width - scrollbarWidth -
                       this.documentDimensions_.width * zoom) /
        2;
    spaceOnLeft = Math.max(spaceOnLeft, 0);

    return {
      x: x * zoom + spaceOnLeft - this.scrollContent_.scrollLeft,
      y: insetDimensions.y * zoom - this.scrollContent_.scrollTop,
      width: insetDimensions.width * zoom,
      height: insetDimensions.height * zoom,
    };
  }

  /**
   * Check if the current fitting type is a paged mode.
   * In a paged mode, page up and page down scroll to the top of the
   * previous/next page and part of the page is under the toolbar.
   * @return Whether the current fitting type is a paged mode.
   */
  private isPagedMode_(): boolean {
    return (
        this.fittingType_ === FittingType.FIT_TO_PAGE ||
        this.fittingType_ === FittingType.FIT_TO_HEIGHT);
  }

  /**
   * Retrieves the in-screen coordinates of the current viewport position.
   */
  private retrieveCurrentScreenCoordinates_(): Point {
    // getMostVisiblePage() always returns an index in range of pageDimensions_.
    const currentPage = this.getMostVisiblePage();
    const dimension = this.pageDimensions_[currentPage]!;
    const x = this.position.x / this.getZoom() - dimension.x;
    const y = this.position.y / this.getZoom() - dimension.y;
    return {x: x, y: y};
  }

  /**
   * Handles a navigation request to a destination from the current controller.
   * @param x The in-screen x coordinate for the destination.
   *     If `x` is undefined, retain current x coordinate value.
   * @param y The in-screen y coordinate for the destination.
   *     If `y` is undefined, retain current y coordinate value.
   */
  handleNavigateToDestination(
      page: number, x: number|undefined, y: number|undefined, zoom: number) {
    // TODO(crbug.com/40262954): Handle view parameters and fitting types.
    if (zoom) {
      this.setZoom(zoom);
    }
    this.goToPageAndXy(page, x, y);
  }

  setSmoothScrolling(isSmooth: boolean) {
    this.smoothScrolling_ = isSmooth;
  }

  /** @param point The position to which to scroll the viewport. */
  scrollTo(point: Partial<Point>) {
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

  /** @param delta The delta by which to scroll the viewport. */
  scrollBy(delta: Point) {
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
   */
  dispatchGesture(gesture: Gesture) {
    this.gestureDetector_.getEventTarget().dispatchEvent(
        new CustomEvent(gesture.type, {detail: gesture.detail}));
  }

  /**
   * Dispatches a swipe event of |direction| external to this viewport.
   */
  dispatchSwipe(direction: SwipeDirection) {
    this.swipeDetector_.getEventTarget().dispatchEvent(
        new CustomEvent('swipe', {detail: direction}));
  }

  /**
   * A callback that's called when an update to a pinch zoom is detected.
   */
  private onPinchUpdate_(e: CustomEvent<PinchEventDetail>) {
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

        const scaleDelta = startScaleRatio! / this.prevScale_;
        if (this.firstPinchCenterInFrame_ != null) {
          this.pinchPanVector_ =
              vectorDelta(center, this.firstPinchCenterInFrame_);
        }

        const needsScrollbars =
            this.documentNeedsScrollbars(this.zoomManager_!.applyBrowserZoom(
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
            y: this.window_.offsetHeight / 2,
          };
        } else if (this.keepContentCentered_) {
          this.oldCenterInContent_ = this.pluginToContent_(this.pinchCenter_);
          this.keepContentCentered_ = false;
        }

        this.fittingType_ = FittingType.NONE;

        this.setPinchZoomInternal_(scaleDelta, center);
        this.updateViewport_();
        this.prevScale_ = startScaleRatio!;
      });
    });
  }

  /**
   * A callback that's called when the end of a pinch zoom is detected.
   */
  private onPinchEnd_(e: CustomEvent<PinchEventDetail>) {
    // Using rAF for pinch end prevents pinch updates scheduled by rAF getting
    // sent after the pinch end.
    window.requestAnimationFrame(() => {
      this.mightZoom_(() => {
        const {center, startScaleRatio} = e.detail;
        this.pinchPhase_ = PinchPhase.END;
        const scaleDelta = startScaleRatio! / this.prevScale_;
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
   */
  private onPinchStart_(e: CustomEvent<PinchEventDetail>) {
    // Disable pinch gestures in Presentation mode.
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
   */
  private onWheel_(e: CustomEvent<PinchEventDetail>) {
    if (e.detail.direction === 'down') {
      this.goToNextPage();
    } else {
      this.goToPreviousPage();
    }
  }

  getGestureDetectorForTesting(): GestureDetector {
    return this.gestureDetector_;
  }

  /**
   * A callback that's called when a left/right swipe is detected in
   * Presentation mode.
   */
  private onSwipe_(e: CustomEvent<SwipeDirection>) {
    // Left and right swipes are enabled only in Presentation mode.
    if (document.fullscreenElement === null && !this.fullscreenForTesting_) {
      return;
    }

    if ((e.detail === SwipeDirection.RIGHT_TO_LEFT && !isRTL()) ||
        (e.detail === SwipeDirection.LEFT_TO_RIGHT && isRTL())) {
      this.goToNextPage();
    } else {
      this.goToPreviousPage();
    }
  }

  enableFullscreenForTesting() {
    this.fullscreenForTesting_ = true;
  }
}

/**
 * Enumeration of pinch states.
 * This should match PinchPhase enum in pdf/pdf_view_web_plugin.cc.
 */
export enum PinchPhase {
  NONE = 0,
  START = 1,
  UPDATE_ZOOM_OUT = 2,
  UPDATE_ZOOM_IN = 3,
  END = 4,
}

/**
 * The increment to scroll a page by in pixels when up/down/left/right arrow
 * keys are pressed. Usually we just let the browser handle scrolling on the
 * window when these keys are pressed but in certain cases we need to simulate
 * these events.
 */
const SCROLL_INCREMENT: number = 40;

/**
 * Returns whether a keyboard event came from another frame.
 */
function isCrossFrameKeyEvent(keyEvent: ExtendedKeyEvent): boolean {
  return !!keyEvent.fromPlugin || !!keyEvent.fromScriptingAPI;
}

/**
 * The width of the page shadow around pages in pixels.
 */
export const PAGE_SHADOW:
    {top: number, bottom: number, left: number, right: number} = {
      top: 3,
      bottom: 7,
      left: 5,
      right: 5,
    };

/**
 * A wrapper around the viewport's scrollable content. This abstraction isolates
 * details concerning internal vs. external scrolling behavior.
 */
class ScrollContent {
  private readonly container_: HTMLElement;
  private readonly sizer_: HTMLElement;
  private target_: EventTarget|null = null;
  private readonly content_: HTMLElement;
  private readonly scrollbarWidth_: number;
  private plugin_: PdfPluginElement|null = null;
  private width_: number = 0;
  private height_: number = 0;
  private scrollLeft_: number = 0;
  private scrollTop_: number = 0;
  private unackedScrollsToRemote_: number = 0;

  /**
   * @param container The element which contains the scrollable content.
   * @param sizer The element which represents the size of the scrollable
   *     content.
   * @param content The element which is the parent of the scrollable content.
   * @param scrollbarWidth The width of any scrollbars.
   */
  constructor(
      container: HTMLElement, sizer: HTMLElement, content: HTMLElement,
      scrollbarWidth: number) {
    this.container_ = container;
    this.sizer_ = sizer;
    this.content_ = content;
    this.scrollbarWidth_ = scrollbarWidth;
  }

  /**
   * Sets the target for dispatching "scroll" events.
   */
  setEventTarget(target: EventTarget) {
    this.target_ = target;
  }

  /**
   * Dispatches a "scroll" event.
   */
  private dispatchScroll_() {
    this.target_ && this.target_.dispatchEvent(new Event('scroll'));
  }

  /**
   * Sets the contents, switching to scrolling locally.
   * @param content The new contents, or null to clear.
   */
  setContent(content: Node|null) {
    if (content === null) {
      this.sizer_.style.display = 'none';
      return;
    }
    this.attachContent_(content);

    // Switch to local content.
    this.sizer_.style.display = 'block';
    if (!this.plugin_) {
      return;
    }
    this.plugin_ = null;

    // Synchronize remote state to local.
    this.updateSize_();
    this.scrollTo(this.scrollLeft_, this.scrollTop_);
  }

  /**
   * Sets the contents, switching to scrolling remotely.
   * @param content The new contents.
   */
  setRemoteContent(content: PdfPluginElement) {
    this.attachContent_(content);

    // Switch to remote content.
    const previousScrollLeft = this.scrollLeft;
    const previousScrollTop = this.scrollTop;
    this.sizer_.style.display = 'none';
    assert(!this.plugin_);
    this.plugin_ = content;

    // Synchronize local state to remote.
    this.updateSize_();
    this.scrollTo(previousScrollLeft, previousScrollTop);
  }

  /**
   * Attaches the contents to the DOM.
   * @param content The new contents.
   */
  private attachContent_(content: Node) {
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
   */
  syncScrollFromRemote(position: Point) {
    if (this.unackedScrollsToRemote_ > 0) {
      // Don't overwrite scroll position while scrolls-to-remote are pending.
      // TODO(crbug.com/40789211): Don't need this if we make this synchronous
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
   */
  ackScrollToRemote(position: Point) {
    assert(this.unackedScrollsToRemote_ > 0);

    if (--this.unackedScrollsToRemote_ === 0) {
      // Accept remote adjustment when there are no pending scrolls-to-remote.
      this.scrollLeft_ = position.x;
      this.scrollTop_ = position.y;
    }

    this.dispatchScroll_();
  }

  get scrollbarWidth(): number {
    return this.scrollbarWidth_;
  }

  get overlayScrollbarWidth(): number {
    // Default width for overlay scrollbars to avoid painting the page indicator
    // over the scrollbar parts.
    let overlayScrollbarWidth = 16;

    // MacOS has a fixed width independent of the presence of a pdf plugin.
    // <if expr="not is_macosx">
    if (this.plugin_) {
      overlayScrollbarWidth = this.scrollbarWidth_;
    }
    // </if>

    return overlayScrollbarWidth;
  }

  /** Gets the content size. */
  get size(): Size {
    return {
      width: this.width_,
      height: this.height_,
    };
  }

  /** Sets the content size. */
  setSize(width: number, height: number) {
    this.width_ = width;
    this.height_ = height;
    this.updateSize_();
  }

  private updateSize_() {
    if (this.plugin_) {
      this.plugin_.postMessage({
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
   */
  get scrollLeft(): number {
    return this.plugin_ ? this.scrollLeft_ : this.container_.scrollLeft;
  }

  /**
   * Gets the scroll offset from the top edge.
   */
  get scrollTop(): number {
    return this.plugin_ ? this.scrollTop_ : this.container_.scrollTop;
  }

  /**
   * Scrolls to the given coordinates.
   * @param isSmooth Whether to scroll smoothly.
   */
  scrollTo(x: number, y: number, isSmooth: boolean = false) {
    if (this.plugin_) {
      // TODO(crbug.com/40809449): Can get NaN if zoom calculations divide by 0.
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
      this.plugin_.postMessage({
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
   * @param maxContent The maximum content dimension.
   * @param maxContainer The maximum container dimension.
   * @param hasScrollbar Whether to compensate for a scrollbar.
   */
  private maxScroll_(
      maxContent: number, maxContainer: number, hasScrollbar: boolean): number {
    if (hasScrollbar) {
      maxContainer -= this.scrollbarWidth_;
    }

    // This may return a negative value, which is fine because scroll positions
    // are clamped to a minimum of 0.
    return maxContent - maxContainer;
  }
}
