// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BitmapMappedFromTrustedProcess} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';
import type {PointF, RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';

import {CenterRotatedBox_CoordinateType} from './geometry.mojom-webui.js';
import type {CenterRotatedBox} from './geometry.mojom-webui.js';
import type {LensPageHandlerInterface} from './lens.mojom-webui.js';
import {LensPageCallbackRouter, LensPageHandlerFactory, LensPageHandlerRemote, UserAction} from './lens.mojom-webui.js';
import {INVOCATION_SOURCE} from './lens_overlay_app.js';
import {recordLensOverlayInteraction} from './metrics_utils.js';
import type {SelectedRegion} from './selection_overlay_base_handler.js';
import {RegionSource, SelectionOverlayBaseHandler} from './selection_overlay_base_handler.js';

let instance: BrowserProxy|null = null;

export interface BrowserProxy {
  callbackRouter: LensPageCallbackRouter;
  handler: LensPageHandlerInterface;
}

class SelectionOverlayBaseHandlerImpl extends SelectionOverlayBaseHandler {
  addNotifyOverlayClosingListener(callback: () => void): number {
    return BrowserProxyImpl.getInstance()
        .callbackRouter.notifyOverlayClosing.addListener(callback);
  }

  closePreselectionBubble(): void {
    BrowserProxyImpl.getInstance().handler.closePreselectionBubble();
  }

  notifyOverlayInitialized(): void {
    BrowserProxyImpl.getInstance().handler.notifyOverlayInitialized();
  }

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
      callback: (screenshotData: BitmapMappedFromTrustedProcess) => void):
      number {
    return BrowserProxyImpl.getInstance()
        .callbackRouter.onOverlayReshown.addListener(callback);
  }

  addScreenshotDataReceivedListener(
      callback:
          (screenshotData: BitmapMappedFromTrustedProcess,
           isSidePanelOpen: boolean) => void,
      ): number {
    return BrowserProxyImpl.getInstance()
        .callbackRouter.screenshotDataReceived.addListener(callback);
  }

  addClearRegionSelectionListener(callback: () => void): number {
    return BrowserProxyImpl.getInstance()
        .callbackRouter.clearRegionSelection.addListener(callback);
  }

  addClearAllSelectionsListener(callback: () => void): number {
    return BrowserProxyImpl.getInstance()
        .callbackRouter.clearAllSelections.addListener(callback);
  }

  addNotifyResultsPanelOpenedListener(callback: () => void): number {
    return BrowserProxyImpl.getInstance()
        .callbackRouter.notifyResultsPanelOpened.addListener(callback);
  }

  addSetPostRegionSelectionListener(callback: (region: RectF) => void): number {
    return BrowserProxyImpl.getInstance()
        .callbackRouter.setPostRegionSelection.addListener(
            this.postRegionSelectionCallback.bind(this, callback));
  }

  addMultiRegionSelectionListener(
      _callback: (regions: SelectedRegion[]) => void): number {
    return -1;
  }

  deleteRegion(_id: string): void {
    // Lens doesn't support deleting a specific region.
  }

  postRegionSelectionCallback(
      callback: (region: RectF) => void, region: CenterRotatedBox): void {
    callback(region.box);
  }

  adjustRegionSelected(rect: RectF, source: RegionSource, _id?: string): void {
    let interaction = UserAction.kRegionSelection;
    let isClick = false;
    switch (source) {
      case RegionSource.KEYBOARD:
        interaction = UserAction.kFullScreenshotRegionSelection;
        break;
      case RegionSource.CLICK:
        interaction = UserAction.kTapRegionSelection;
        isClick = true;
        break;
      case RegionSource.SELECTION:
        interaction = UserAction.kRegionSelection;
        break;
      case RegionSource.SELECTION_CHANGE:
        interaction = UserAction.kRegionSelectionChange;
        break;
      default:
        break;
    }
    recordLensOverlayInteraction(INVOCATION_SOURCE, interaction);
    BrowserProxyImpl.getInstance().handler.issueLensRegionRequest(
        {
          box: rect,
          rotation: 0,
          coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
        },
        isClick);
  }

  adjustPolylineSelected(
      _points: PointF[], _source: RegionSource, _id?: string): void {
    // Lens doesn't support polylines.
  }
}

export class BrowserProxyImpl implements BrowserProxy {
  callbackRouter: LensPageCallbackRouter = new LensPageCallbackRouter();
  handler: LensPageHandlerRemote = new LensPageHandlerRemote();

  constructor() {
    const factory = LensPageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.handler.$.bindNewPipeAndPassReceiver(),
        this.callbackRouter.$.bindNewPipeAndPassRemote());
    SelectionOverlayBaseHandler.setInstance(
        new SelectionOverlayBaseHandlerImpl());
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
    SelectionOverlayBaseHandler.setInstance(
        new SelectionOverlayBaseHandlerImpl());
  }
}
