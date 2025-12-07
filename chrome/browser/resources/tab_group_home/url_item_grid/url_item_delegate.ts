// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

export interface BadgeInfo {
  text: string;
  crIconName?: string;
}

export interface UrlItem {
  id: number;
  title: string;
  url: Url;
  badgeInfo?: BadgeInfo;
}

export enum ItemEventType {
  ITEM_ADDED = 'item-added',
  ITEM_REMOVED = 'item-removed',
  ITEM_MOVED = 'item-moved',
  ITEM_UPDATED = 'item-updated',
  ITEM_THUMBNAIL_UPDATED = 'item-thumbnail-updated',
}

export interface UrlItemDelegate {
  getItems(): Promise<UrlItem[]>;
  clickItem(id: number): void;
  removeItems(ids: number[]): void;
  moveItem(id: number, index: number): void;
  // This is used to dispatch and listen for all ItemEventType.
  getEventTarget(): EventTarget;
}
