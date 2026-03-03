// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BitmapMappedFromTrustedProcess} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';
import type {RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';

export enum RegionSource {
  CLICK,
  SELECTION,
  KEYBOARD,
  SELECTION_CHANGE,
}

/*
 * Interface definition of the core functionality that the common selection
 * controller requires from the embedding code.
 */
export interface SelectionOverlayBaseHandler {
  addBackgroundBlur(): void;
  addOnOverlayReshownListener(
      callback: (screenshotData: BitmapMappedFromTrustedProcess) => void):
      number;
  addNotifyOverlayClosingListener(callback: () => void): number;
  addScreenshotDataReceivedListener(
      callback:
          (screenshotData: BitmapMappedFromTrustedProcess,
           isSidePanelOpen: boolean) => void,
      ): number;
  addClearRegionSelectionListener(callback: () => void): number;
  addClearAllSelectionsListener(callback: () => void): number;
  addNotifyResultsPanelOpenedListener(callback: () => void): number;
  addSetPostRegionSelectionListener(callback: (region: RectF) => void): number;
  adjustRegionSelected(rect: RectF, source: RegionSource): void;
  closePreselectionBubble(): void;
  notifyOverlayInitialized(): void;
  removeListener(id: number): boolean;
  setLiveBlur(enabled: boolean): void;
}

/*
 * This class has a static member pointing to the implementation of the
 * installed SelectionOverlayBaseHandler. setInstance should be called in each
 * embedder.
 */
export class SelectionOverlayBaseHandler {
  private static instance: SelectionOverlayBaseHandler|null = null;

  static getInstance(): SelectionOverlayBaseHandler {
    return SelectionOverlayBaseHandler.instance!;
  }

  static setInstance(obj: SelectionOverlayBaseHandler) {
    SelectionOverlayBaseHandler.instance = obj;
  }
}
