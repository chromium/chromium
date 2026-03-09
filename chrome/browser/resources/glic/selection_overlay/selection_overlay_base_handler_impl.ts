// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BitmapMappedFromTrustedProcess} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';
import type {RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import type {RegionSource, SelectionOverlayBaseHandler} from '/lens/selection_overlay_base_handler.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {SelectedRegionMojoType} from './selection_overlay.mojom-webui.js';

export class SelectionOverlayBaseHandlerImpl implements
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

  addClearRegionSelectionListener(_callback: () => void): number {
    return -1;
  }

  addClearAllSelectionsListener(_callback: () => void): number {
    return -1;
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
              if (!firstRegion || !firstRegion.region) {
                return;
              }
              callback(new DOMRect(
                  firstRegion.region.x, firstRegion.region.y,
                  firstRegion.region.width, firstRegion.region.height));
            });
  }

  adjustRegionSelected(rect: RectF, _source: RegionSource): void {
    BrowserProxyImpl.getInstance().handler.adjustRegion({
      region: rect,
      id: '0123456789ABCDEF0123456789ABCDEF',
    });
  }
}
