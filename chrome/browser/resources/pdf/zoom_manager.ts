// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ZoomBehavior} from './browser_api.js';


const MIN_ZOOM_DELTA = 0.01;

/** @return Whether two numbers are approximately equal. */
function floatingPointEquals(a: number, b: number): boolean {
  // If the zoom level is close enough to the current zoom level, don't
  // change it. This avoids us getting into an infinite loop of zoom changes
  // due to floating point error.
  return Math.abs(a - b) <= MIN_ZOOM_DELTA;
}

// Abstract parent of classes that manage updating the browser with zoom changes
// and/or updating the viewer's zoom when the browser zoom changes.
export abstract class ZoomManager {
  protected browserZoom: number;
  protected getViewportZoom: () => number;
  private eventTarget_ = new EventTarget();

  /**
   * @param getViewportZoomCallback Callback to get the viewport's current zoom
   *     level.
   * @param initialZoom The initial browser zoom level.
   */
  constructor(getViewportZoomCallback: () => number, initialZoom: number) {
    this.browserZoom = initialZoom;
    this.getViewportZoom = getViewportZoomCallback;
  }

  getEventTarget(): EventTarget {
    return this.eventTarget_;
  }

  /**
   * Creates the appropriate kind of zoom manager given the zoom behavior.
   * @param zoomBehavior How to manage zoom.
   * @param getViewportZoom A function that gets the current viewport zoom.
   * @param setBrowserZoomFunction A function that sets the browser zoom to the
   *     provided value.
   * @param initialZoom The initial browser zoom level.
   */
  static create(
      zoomBehavior: ZoomBehavior, getViewportZoom: () => number,
      setBrowserZoomFunction: (zoom: number) => Promise<void>,
      initialZoom: number): ZoomManager {
    switch (zoomBehavior) {
      case ZoomBehavior.MANAGE:
        return new ActiveZoomManager(
            getViewportZoom, setBrowserZoomFunction, initialZoom);
      case ZoomBehavior.PROPAGATE_PARENT:
        return new EmbeddedZoomManager(getViewportZoom, initialZoom);
      default:
        return new InactiveZoomManager(getViewportZoom, initialZoom);
    }
  }

  /**
   * Invoked when a browser-initiated zoom-level change occurs.
   * @param newZoom the zoom level to zoom to.
   */
  abstract onBrowserZoomChange(newZoom: number): void;

  /** Invoked when an extension-initiated zoom-level change occurs. */
  abstract onPdfZoomChange(): void;

  /**
   * Combines the internal pdf zoom and the browser zoom to
   * produce the total zoom level for the viewer.
   * @param internalZoom the zoom level internal to the viewer.
   * @return the total zoom level.
   */
  applyBrowserZoom(internalZoom: number): number {
    return this.browserZoom * internalZoom;
  }

  /**
   * Given a zoom level, return the internal zoom level needed to
   * produce that zoom level.
   * @param totalZoom the total zoom level.
   * @return the zoom level internal to the viewer.
   */
  internalZoomComponent(totalZoom: number): number {
    return totalZoom / this.browserZoom;
  }
}

// Has no control over the browser's zoom and does not respond to browser zoom
// changes.
export class InactiveZoomManager extends ZoomManager {
  override onBrowserZoomChange(_newZoom: number) {}
  override onPdfZoomChange() {}
}

// ActiveZoomManager controls the browser's zoom.
class ActiveZoomManager extends ZoomManager {
  private setBrowserZoomFunction_: (zoom: number) => Promise<void>;
  private changingBrowserZoom_: Promise<void>|null = null;

  /**
   * Constructs a ActiveZoomManager.
   * @param getViewportZoom A function that gets the current viewport zoom level
   * @param setBrowserZoomFunction A function that sets the browser zoom to the
   *     provided value.
   * @param initialZoom The initial browser zoom level.
   */
  constructor(
      getViewportZoom: () => number,
      setBrowserZoomFunction: (zoom: number) => Promise<void>,
      initialZoom: number) {
    super(getViewportZoom, initialZoom);
    this.setBrowserZoomFunction_ = setBrowserZoomFunction;
  }

  onBrowserZoomChange(newZoom: number) {
    // If we are changing the browser zoom level, ignore any browser zoom level
    // change events. Either, the change occurred before our update and will be
    // overwritten, or the change being reported is the change we are making,
    // which we have already handled.
    if (this.changingBrowserZoom_) {
      return;
    }

    if (floatingPointEquals(this.browserZoom, newZoom)) {
      return;
    }

    this.browserZoom = newZoom;
    this.getEventTarget().dispatchEvent(
        new CustomEvent('set-zoom', {detail: newZoom}));
  }

  override onPdfZoomChange() {
    // If we are already changing the browser zoom level in response to a
    // previous extension-initiated zoom-level change, ignore this zoom change.
    // Once the browser zoom level is changed, we check whether the extension's
    // zoom level matches the most recently sent zoom level.
    if (this.changingBrowserZoom_) {
      return;
    }

    const viewportZoom = this.getViewportZoom();
    if (floatingPointEquals(this.browserZoom, viewportZoom)) {
      return;
    }

    this.changingBrowserZoom_ =
        this.setBrowserZoomFunction_(viewportZoom).then(() => {
          this.browserZoom = viewportZoom;
          this.changingBrowserZoom_ = null;

          // The extension's zoom level may have changed while the browser zoom
          // change was in progress. We call back into onPdfZoomChange to ensure
          // the browser zoom is up to date.
          this.onPdfZoomChange();
        });
  }

  /**
   * Combines the internal pdf zoom and the browser zoom to
   * produce the total zoom level for the viewer.
   * @param internalZoom the zoom level internal to the viewer.
   * @return the total zoom level.
   */
  override applyBrowserZoom(internalZoom: number): number {
    // The internal zoom and browser zoom are changed together, so the
    // browser zoom is already applied.
    return internalZoom;
  }

  /**
   * Given a zoom level, return the internal zoom level needed to
   * produce that zoom level.
   * @param totalZoom the total zoom level.
   * @return the zoom level internal to the viewer.
   */
  override internalZoomComponent(totalZoom: number): number {
    // The internal zoom and browser zoom are changed together, so the
    // internal zoom is the total zoom.
    return totalZoom;
  }
}

// Responds to changes in the browser zoom, but does not control the browser
// zoom.
class EmbeddedZoomManager extends ZoomManager {
  /**
   * Invoked when a browser-initiated zoom-level change occurs.
   * @param newZoom the new browser zoom level.
   */
  override onBrowserZoomChange(newZoom: number) {
    const oldZoom = this.browserZoom;
    this.browserZoom = newZoom;
    this.getEventTarget().dispatchEvent(
        new CustomEvent('update-zoom-from-browser', {detail: oldZoom}));
  }

  override onPdfZoomChange() {}
}
