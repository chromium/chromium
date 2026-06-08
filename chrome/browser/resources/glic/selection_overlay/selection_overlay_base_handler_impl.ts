// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import type {BitmapMappedFromTrustedProcess} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';
import type {PointF, RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import type {SelectedRegion} from '/lens/selection_overlay_base_handler.js';
import {RegionSource, SelectionOverlayBaseHandler} from '/lens/selection_overlay_base_handler.js';
import {calculateCenterRotatedBox} from '/lens/selection_utils.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {SelectedRegionMojoType} from './selection_overlay.mojom-webui.js';

function generateRandomHexId(): string {
  return Array
      .from(
          {length: 32},
          () => Math.floor(Math.random() * 16).toString(16).toUpperCase())
      .join('');
}

export class SelectionOverlayBaseHandlerImpl extends
    SelectionOverlayBaseHandler {
  addNotifyOverlayClosingListener(_callback: () => void): number {
    return -1;
  }

  closePreselectionBubble(): void {
    BrowserProxyImpl.getInstance().handler.closePreselectionBubble();
  }

  notifyOverlayInitialized(): void {}

  addBackgroundBlur(): void {
    BrowserProxyImpl.getInstance().handler.addBackgroundBlur();
  }

  setLiveBlur(enabled: boolean): void {
    BrowserProxyImpl.getInstance().handler.setLiveBlur(enabled);
  }

  removeListener(id: number): boolean {
    if (id === -1) {
      return true;
    }
    return BrowserProxyImpl.getInstance().callbackRouter.removeListener(id);
  }

  addOnOverlayReshownListener(
      _callback: (screenshotData: BitmapMappedFromTrustedProcess) => void):
      number {
    return -1;
  }

  addScreenshotDataReceivedListener(
      callback:
          (screenshotData: BitmapMappedFromTrustedProcess,
           isSidePanelOpen: boolean) => void,
      ): number {
    return BrowserProxyImpl.getInstance()
        .callbackRouter.screenshotReceived.addListener(
            (screenshotData: BitmapMappedFromTrustedProcess) => {
              callback(screenshotData, false);
            });
  }

  addClearRegionSelectionListener(callback: () => void): number {
    return BrowserProxyImpl.getInstance()
        .callbackRouter.setPostRegionSelections.addListener(
            (regions: SelectedRegionMojoType[]) => {
              if (regions.length === 0) {
                callback();
              }
            });
  }

  addClearAllSelectionsListener(callback: () => void): number {
    return BrowserProxyImpl.getInstance()
        .callbackRouter.setPostRegionSelections.addListener(
            (regions: SelectedRegionMojoType[]) => {
              if (regions.length === 0) {
                callback();
              }
            });
  }

  addNotifyResultsPanelOpenedListener(callback: () => void): number {
    // We are always invoked from the results panel for now.
    callback();
    return -1;
  }

  addSetPostRegionSelectionListener(callback: (region: RectF) => void): number {
    return BrowserProxyImpl.getInstance()
        .callbackRouter.setPostRegionSelections.addListener(
            (regions: SelectedRegionMojoType[]) => {
              const firstRegion = regions[0];
              if (firstRegion && firstRegion.shape.rect) {
                callback(firstRegion.shape.rect);
              }
            });
  }

  addMultiRegionSelectionListener(
      callback: (regions: SelectedRegion[]) => void): number {
    return BrowserProxyImpl.getInstance()
        .callbackRouter.setPostRegionSelections.addListener(
            (regions: SelectedRegionMojoType[]) => {
              callback(regions.map(r => {
                let rect = r.shape.rect;
                if (!rect && r.shape.polyline) {
                  rect = calculateCenterRotatedBox(r.shape.polyline).box;
                }
                const tsRegion: SelectedRegion = {
                  id: r.id,
                  region: rect || {x: 0, y: 0, width: 0, height: 0},
                };
                if (r.shape.polyline) {
                  tsRegion.polyline = r.shape.polyline;
                }
                return tsRegion;
              }));
            });
  }

  adjustRegionSelected(
      rect: RectF, source: RegionSource,
      id: string = this.activeRegionId || generateRandomHexId()): void {
    const proxy = BrowserProxyImpl.getInstance();
    proxy.handler.adjustRegion(
        {
          id: id,
          shape: {rect: rect},
        },
        source === RegionSource.KEYBOARD);
  }

  adjustPolylineSelected(
      points: PointF[], source: RegionSource,
      id: string = this.activeRegionId || generateRandomHexId()): void {
    const proxy = BrowserProxyImpl.getInstance();
    proxy.handler.adjustRegion(
        {
          id: id,
          shape: {polyline: points},
        },
        source === RegionSource.KEYBOARD);
  }

  deleteRegion(id: string, source: RegionSource): void {
    const proxy = BrowserProxyImpl.getInstance();
    proxy.handler.deleteRegion(id, source === RegionSource.KEYBOARD);
  }
}
