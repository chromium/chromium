// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import {isMac} from 'chrome://resources/js/platform.js';

import {ClickDispositionFlag} from './browser_proxy.js';
import {TimerHelper} from './timer_helper.js';

export const BUTTON_LEFT = 0;
export const BUTTON_MIDDLE = 1;
export const BUTTON_RIGHT = 2;


export interface GetClickDispositionFlagsOptions {
  ignoreCtrlKey?: boolean;
  ignoreShiftKey?: boolean;
}

interface DragState {
  initialY: number;
  isListening: boolean;
  activePointerId: number;
  activeElement: HTMLElement|null;
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
 * Important note on accessibility:
 * `PressHandler` specifically manages pointer (mouse, touch, pen) interactions.
 * Natively, `<cr-icon-button>` intercepts keyboard `Space` and `Enter` keys and
 * synthesizes a standard `click` event (where `e.detail === 0`).
 * To ensure your button is fully accessible to keyboard, you MUST also add an
 * `@click` listener that explicitly intercepts these synthetic clicks and
 * delegates them to your short press logic, while ignoring actual mouse clicks
 * (which have `detail > 0` and are already handled here by `pointerup`).
 *
 * Example of direct usage in .html.ts:
 * ```html
 * <cr-icon-button
 *   @pointerdown="${this.pressHandler_.onPointerdown}"
 *   @pointerup="${this.pressHandler_.onPointerup}"
 *   @pointercancel="${this.pressHandler_.onPointercancel}"
 *   @contextmenu="${this.pressHandler_.onContextmenu}"
 *   @click="${this.onClick_}">
 * </cr-icon-button>
 * ```
 */
export class PressHandler {
  // This follows what is done in the views' ToolbarButton.
  private static readonly LONG_PRESS_TIMER_THRESHOLD_MS = 500;
  private static readonly DRAG_THRESHOLD_PX = 8;
  private static readonly NO_ACTIVE_POINTER_ID = -1;

  private isLongPressed_: boolean = false;
  private longPressTimer_: TimerHelper = new TimerHelper();
  private onLongPress_: (source: MenuSourceType) => void;
  private onShortPress_: (e: MouseEvent) => void;
  // Whether to treat Mac Ctrl+LeftClick as a context menu trigger (long press)
  // instead of a short press. This is standard behavior for most buttons,
  // but some (like reload) need it disabled to handle Ctrl+Click differently.
  private enableMacContextClick_: boolean;
  private dragState_: DragState|null = null;

  constructor(
      onLongPress: (source: MenuSourceType) => void,
      onShortPress: (e: MouseEvent) => void,
      enableMacContextClick: boolean = true,
      enableDragToOpenMenu: boolean = true) {
    this.onLongPress_ = onLongPress;
    this.onShortPress_ = onShortPress;
    this.enableMacContextClick_ = enableMacContextClick;
    if (enableDragToOpenMenu) {
      this.dragState_ = {
        initialY: 0,
        isListening: false,
        activePointerId: PressHandler.NO_ACTIVE_POINTER_ID,
        activeElement: null,
      };
    }
  }

  private onPointermove_ = (e: PointerEvent) => {
    if (!this.dragState_ || e.pointerId !== this.dragState_.activePointerId) {
      return;
    }
    // Detect downward drag.
    if (e.clientY - this.dragState_.initialY > PressHandler.DRAG_THRESHOLD_PX) {
      this.isLongPressed_ = true;
      this.longPressTimer_.clearTimeout();
      this.onLongPress_(getContextMenuSourceType(e));
      this.resetDragState_();
    }
  };

  private resetDragState_() {
    if (this.dragState_?.isListening && this.dragState_.activeElement) {
      this.dragState_.activeElement.removeEventListener(
          'pointermove', this.onPointermove_);
      if (this.dragState_.activePointerId !==
          PressHandler.NO_ACTIVE_POINTER_ID) {
        if (this.dragState_.activeElement.hasPointerCapture(
                this.dragState_.activePointerId)) {
          this.dragState_.activeElement.releasePointerCapture(
              this.dragState_.activePointerId);
        }
        this.dragState_.activePointerId = PressHandler.NO_ACTIVE_POINTER_ID;
      }
      this.dragState_.activeElement = null;
      this.dragState_.isListening = false;
    }
  }

  private shouldCancelClick_(e: PointerEvent): boolean {
    if (!this.dragState_?.activeElement) {
      // If drag-to-open-menu is not enabled or we're not actively tracking an
      // element, don't cancel the click.
      return false;
    }
    const rect = this.dragState_.activeElement.getBoundingClientRect();
    return e.clientX < rect.left || e.clientX > rect.right ||
        e.clientY < rect.top || e.clientY > rect.bottom;
  }

  onPointerdown = (e: PointerEvent, skipLongPress: boolean = false) => {
    // Ignore secondary pointers if we are actively listening to a primary
    // pointer.
    if (this.dragState_?.isListening) {
      return;
    }

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
    this.longPressTimer_.clearTimeout();

    if (skipLongPress) {
      return;
    }

    if (this.dragState_) {
      this.dragState_.initialY = e.clientY;
      this.dragState_.isListening = true;
      // Use currentTarget to ensure we are capturing the button element that
      // the listener was attached to, even if the pointer is over a child
      // element (like an icon).
      const target = e.currentTarget as HTMLElement;
      this.dragState_.activeElement = target;
      this.dragState_.activePointerId = e.pointerId;
      target.setPointerCapture(e.pointerId);
      target.addEventListener('pointermove', this.onPointermove_);
    }

    this.longPressTimer_.setTimeout(() => {
      // When the long press is triggered and handled, mark `isLongPressed_`
      // as true, so that it won't be treated as a normal click.
      this.isLongPressed_ = true;
      this.resetDragState_();
      this.onLongPress_(MenuSourceType.kLongPress);
    }, PressHandler.LONG_PRESS_TIMER_THRESHOLD_MS);
  };

  onPointerup = (e: PointerEvent) => {
    // Ignore secondary pointers if we are actively listening to a primary
    // pointer.
    if (this.dragState_?.isListening &&
        e.pointerId !== this.dragState_.activePointerId) {
      return;
    }

    if (e.button === BUTTON_RIGHT) {
      this.resetDragState_();
      return;
    }

    const isMacCtrlClick = this.enableMacContextClick_ && isMac &&
        e.button === BUTTON_LEFT && e.ctrlKey;

    // If the long press is already handled or it's Ctrl+LeftClick on Mac,
    // skip the rest.
    if (this.isLongPressed_ || isMacCtrlClick) {
      this.isLongPressed_ = false;
      this.longPressTimer_.clearTimeout();
      this.resetDragState_();
      return;
    }

    this.longPressTimer_.clearTimeout();
    const shouldCancel = this.shouldCancelClick_(e);
    this.resetDragState_();

    if (shouldCancel) {
      return;
    }

    this.onShortPress_(e);
  };

  onPointercancel = (e: PointerEvent) => {
    // Ignore secondary pointers if we are actively listening to a primary
    // pointer.
    if (this.dragState_?.isListening &&
        e.pointerId !== this.dragState_.activePointerId) {
      return;
    }
    this.resetDragState_();
    this.longPressTimer_.clearTimeout();
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
  // Returns the element's bounding rectangle relative to the WebUI viewport in
  // CSS pixels.
  return element.getBoundingClientRect();
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
  // KeyboardEvent. Note: Keyboard activations on `<cr-icon-button>` (Space or
  // Enter) programmatically synthesize standard `click` events with `detail` 0.
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
  if (e instanceof PointerEvent) {
    if (e.pointerType === 'touch' || e.pointerType === 'pen') {
      return MenuSourceType.kTouch;
    }
    return MenuSourceType.kMouse;
  }
  if (e instanceof MouseEvent && e.button === BUTTON_LEFT) {
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
