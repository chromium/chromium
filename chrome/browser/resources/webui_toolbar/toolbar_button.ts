// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';

import {ClickDispositionFlag} from './browser_proxy.js';

export const BUTTON_LEFT = 0;
export const BUTTON_MIDDLE = 1;
export const BUTTON_RIGHT = 2;

export interface GetClickDispositionFlagsOptions {
  ignoreCtrlKey?: boolean;
  ignoreShiftKey?: boolean;
}

export function getContextMenuPosition(element: HTMLElement) {
  const bounds = element.getBoundingClientRect();
  const isRtl = document.dir === 'rtl';
  const x = isRtl ? bounds.right : bounds.left;
  return {x, y: bounds.bottom};
}

// Should match ui::GetMenuSourceTypeForEvent().
// * keyboard event -> keyboard
// * touch event -> touch
// * others -> mouse
export function getClickSourceType(e: Event): MenuSourceType {
  if (e instanceof PointerEvent &&
      (e.pointerType === 'touch' || e.pointerType === 'pen')) {
    return MenuSourceType.kTouch;
  }
  // Because `e` is a PointerEvent, we cannot check whether `e` is a
  // KeyboardEvent. Need to check e.detail instead, which is 0 for
  // KeyboardEvent.
  if (e instanceof MouseEvent && e.detail === 0) {
    return MenuSourceType.kKeyboard;
  }
  return MenuSourceType.kMouse;
}

// Should match ui::GetMenuSourceTypeForEvent().
// * keyboard event -> keyboard
// * touch/gesture event -> touch
// * others -> mouse
export function getContextMenuSourceType(e: Event): MenuSourceType {
  if (e instanceof PointerEvent &&
      (e.pointerType === 'touch' || e.pointerType === 'pen')) {
    return MenuSourceType.kTouch;
  }
  if (e instanceof MouseEvent && e.button === 0) {
    // Mac Ctrl+Click (ctrlKey + button 0) should be Mouse for context menu.
    if (e.type === 'contextmenu' && e.ctrlKey) {
      return MenuSourceType.kMouse;
    }
    return MenuSourceType.kKeyboard;
  }
  return MenuSourceType.kMouse;
}

export function getClickDispositionFlags(
    e: MouseEvent,
    options: GetClickDispositionFlagsOptions = {}): ClickDispositionFlag[] {
  const flags: ClickDispositionFlag[] = [];
  if (e.button === BUTTON_MIDDLE) {
    flags.push(ClickDispositionFlag.kMiddleMouseButton);
  }
  if (e.altKey) {
    flags.push(ClickDispositionFlag.kAltKeyDown);
  }
  if (e.ctrlKey && !options.ignoreCtrlKey) {
    flags.push(ClickDispositionFlag.kControlKeyDown);
  }
  if (e.metaKey) {
    flags.push(ClickDispositionFlag.kMetaKeyDown);
  }
  if (e.shiftKey && !options.ignoreShiftKey) {
    flags.push(ClickDispositionFlag.kShiftKeyDown);
  }
  return flags;
}
