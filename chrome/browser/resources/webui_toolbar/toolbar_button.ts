// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import {isMac} from 'chrome://resources/js/platform.js';

import {ClickDispositionFlag} from './browser_proxy.js';

export const BUTTON_LEFT = 0;
export const BUTTON_MIDDLE = 1;
export const BUTTON_RIGHT = 2;

// This follows what is done in the views code (ToolbarButton::OnMousePressed).
const LONG_PRESS_TIMER_THRESHOLD_MS = 500;

export interface GetClickDispositionFlagsOptions {
  ignoreCtrlKey?: boolean;
  ignoreShiftKey?: boolean;
}

/**
 * A helper class for handling pointer events to trigger long press, short
 * press, as well as context menu actions.
 *
 * This class encapsulates the common logic for determining whether a pointer
 * interaction should result in a short press, a long press (often used to show
 * a context menu), or a direct context menu trigger (e.g., right-click or Mac
 * Ctrl+LeftClick).
 *
 * Safe usage of this class requires the caller to route all relevant pointer
 * events (pointerdown, pointerup, pointercancel) and the contextmenu event
 * to this class's respective handlers. These can be bound directly in the
 * HTML template, or called from within custom event listeners if the component
 * requires additional logic (like the reload button).
 *
 * Example of direct usage in .html.ts:
 * ```html
 * <cr-icon-button
 *   @pointerdown="${this.pressHandler_.onPointerdown}"
 *   @pointerup="${this.pressHandler_.onPointerup}"
 *   @pointercancel="${this.pressHandler_.onPointercancel}"
 *   @contextmenu="${this.pressHandler_.onContextmenu}">
 * </cr-icon-button>
 * ```
 */
export class PressHandler {
  private isLongPressed_: boolean = false;
  private longPressTimer_: number = 0;
  private onLongPress_: (source: MenuSourceType) => void;
  private onShortPress_: (e: PointerEvent) => void;
  // Whether to treat Mac Ctrl+LeftClick as a context menu trigger (long press)
  // instead of a short press. This is standard behavior for most buttons,
  // but some (like reload) need it disabled to handle Ctrl+Click differently.
  private enableMacContextClick_: boolean;

  constructor(
      onLongPress: (source: MenuSourceType) => void,
      onShortPress: (e: PointerEvent) => void,
      enableMacContextClick: boolean = true) {
    this.onLongPress_ = onLongPress;
    this.onShortPress_ = onShortPress;
    this.enableMacContextClick_ = enableMacContextClick;
  }

  onPointerdown = (e: PointerEvent, skipLongPress: boolean = false) => {
    if (e.button === BUTTON_RIGHT) {
      // The TypeScript code should only handle long press for the
      // left-click/middle-click.
      return;
    }

    if (this.enableMacContextClick_ && isMac && e.button === BUTTON_LEFT &&
        e.ctrlKey) {
      this.onLongPress_(getContextMenuSourceType(e));
      return;
    }

    this.isLongPressed_ = false;
    clearTimeout(this.longPressTimer_);

    if (skipLongPress) {
      return;
    }

    this.longPressTimer_ = setTimeout(() => {
      // When the long press is triggered and handled, mark `isLongPressed_`
      // as true, so that it won't be treated as a normal click.
      this.isLongPressed_ = true;
      this.onLongPress_(MenuSourceType.kLongPress);
    }, LONG_PRESS_TIMER_THRESHOLD_MS);
  };

  onPointerup = (e: PointerEvent) => {
    if (e.button === BUTTON_RIGHT) {
      return;
    }

    const isMacCtrlClick = this.enableMacContextClick_ && isMac &&
        e.button === BUTTON_LEFT && e.ctrlKey;

    // If the long press is already handled or it's Ctrl+LeftClick on Mac,
    // skip the rest.
    if (this.isLongPressed_ || isMacCtrlClick) {
      this.isLongPressed_ = false;
      clearTimeout(this.longPressTimer_);
      return;
    }

    clearTimeout(this.longPressTimer_);
    this.onShortPress_(e);
  };

  onPointercancel = () => {
    clearTimeout(this.longPressTimer_);
  };

  onContextmenu = (e: PointerEvent) => {
    e.preventDefault();
    // If it's a Mac Ctrl+LeftClick, the browser natively fires a contextmenu
    // event. If `enableMacContextClick_` is true, we already showed the menu in
    // pointerdown. If `enableMacContextClick_` is false, we want it to act as a
    // normal click, not a context menu. In both cases, we MUST suppress the
    // native contextmenu event.
    if (isMac && e.button === BUTTON_LEFT && e.ctrlKey) {
      return;
    }

    this.onLongPress_(getContextMenuSourceType(e));
  };
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
