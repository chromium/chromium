// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEnumVariant} from '../util.js';

export enum UIComponent {
  PREVIEW_VIDEO = 'preview_video',
}

export interface UIComponentInfo {
  name: string;
  selector: string;
}
type UIComponentMap = Map<UIComponent, UIComponentInfo>;
const uiComponentMap: UIComponentMap = new Map([
  [
    UIComponent.PREVIEW_VIDEO,
    {name: 'Preview video', selector: '#preview-video'},
  ],
]);

/**
 * Returns UIComponentInfo of the specified UIComponent enum, throws error if
 * there are no such UIComponent value.
 */
export function getUIComponent(name: UIComponent): UIComponentInfo {
  assertEnumVariant(UIComponent, name);
  const info = uiComponentMap.get(name);
  if (info === undefined) {
    throw new Error(`Cannot find component for ${name}`);
  }
  return info;
}
