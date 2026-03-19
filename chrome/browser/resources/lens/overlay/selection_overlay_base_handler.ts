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

export interface SelectedRegion {
  id: string;
  region: RectF;
}

/*
 * Base class definition of the core functionality that the common selection
 * controller requires from the embedding code.
 */
export abstract class SelectionOverlayBaseHandler {
  private static instance: SelectionOverlayBaseHandler|null = null;
  activeRegionId: string = '';

  static getInstance(): SelectionOverlayBaseHandler {
    return SelectionOverlayBaseHandler.instance!;
  }

  static setInstance(obj: SelectionOverlayBaseHandler) {
    SelectionOverlayBaseHandler.instance = obj;
  }

  abstract addMultiRegionSelectionListener(
      callback: (regions: SelectedRegion[]) => void): number;
  abstract removeListener(id: number): boolean;

  abstract addBackgroundBlur(): void;
  abstract addOnOverlayReshownListener(
      callback: (screenshotData: BitmapMappedFromTrustedProcess) => void):
      number;
  abstract addNotifyOverlayClosingListener(callback: () => void): number;
  abstract addScreenshotDataReceivedListener(
      callback:
          (screenshotData: BitmapMappedFromTrustedProcess,
           isSidePanelOpen: boolean) => void,
      ): number;
  abstract addClearRegionSelectionListener(callback: () => void): number;
  abstract addClearAllSelectionsListener(callback: () => void): number;
  abstract addNotifyResultsPanelOpenedListener(callback: () => void): number;
  abstract addSetPostRegionSelectionListener(callback: (region: RectF) => void):
      number;
  abstract adjustRegionSelected(rect: RectF, source: RegionSource, id?: string):
      void;
  abstract deleteRegion(id: string): void;
  abstract closePreselectionBubble(): void;
  abstract notifyOverlayInitialized(): void;
  abstract setLiveBlur(enabled: boolean): void;
}
