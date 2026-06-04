// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import {isMac} from 'chrome://resources/js/platform.js';

import {EventDispositionFlag} from './browser_proxy.js';
import {TimerHelper} from './timer_helper.js';

export const BUTTON_LEFT = 0;
export const BUTTON_MIDDLE = 1;
export const BUTTON_RIGHT = 2;


export interface GetEventDispositionFlagsOptions {
  ignoreCtrlKey?: boolean;
  ignoreShiftKey?: boolean;
}

// Tracks state used for deciding whether to display a context menu instead of
// treating a pointer interaction as a click (a "short press"). Only populated
// while holding a pointer down, cleared on release, or when the interaction is
// determined to be a "long press", which generally results in showing a context
// menu.
//
// A press may be determined to be long based on either press duration or
// vertical mouse movement.
interface ContextMenuState {
  // True as long as the pointer is down, and the interaction has not been
  // passed along as a long or short press.
  isListening: boolean;
  activePointerId: number;
  activeElement: HTMLElement|null;

  longPressTimer: TimerHelper;

  initialY: number;
}

// Tests if a mouse event occurred over the given HTMLElement. Since shadow DOM
// interfered with using getElementFromPoint() and checking `target` contains
// it, instead checks that the mouse is in the DOMRect of `target`.
function isMouseEventOverTarget(e: MouseEvent, target: HTMLElement) {
  const targetRect: DOMRect = target.getBoundingClientRect();
  // Note that the area an element occupies is [left, right) and [top, bottom),
  // hence this check only allows equality for the left and top edges, and not
  // the right and bottom ones.
  return e.clientX >= targetRect.left && e.clientX < targetRect.right &&
      e.clientY >= targetRect.top && e.clientY < targetRect.bottom;
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
 * This class typically gets pointer capture on pointer down, and then on
 * pointer up, only treats it as a click if it has pointer capture. This gets us
 * the standard button behavior where pressing on a different button or on a
 * non-button area and releasing on a button does nothing, nor does pressing
 * on a button when it's disabled, and releasing after it's been enabled.
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

  private onLongPress_: (source: MenuSourceType) => void;
  private onShortPress_: (e: MouseEvent) => void;

  private contextMenuState_: ContextMenuState|null = null;

  constructor(
      onLongPress: (source: MenuSourceType) => void,
      onShortPress: (e: MouseEvent) => void,
      enableContextMenu: boolean = true) {
    this.onLongPress_ = onLongPress;
    this.onShortPress_ = onShortPress;
    if (enableContextMenu) {
      this.contextMenuState_ = {
        isListening: false,
        activePointerId: PressHandler.NO_ACTIVE_POINTER_ID,
        activeElement: null,
        longPressTimer: new TimerHelper(),
        initialY: 0,
      };
    }
  }

  private onPointermove_ = (e: PointerEvent) => {
    if (!this.contextMenuState_ ||
        e.pointerId !== this.contextMenuState_.activePointerId) {
      return;
    }
    // Detect downward drag.
    if (e.clientY - this.contextMenuState_.initialY >
        PressHandler.DRAG_THRESHOLD_PX) {
      this.onLongPress_(getContextMenuSourceType(e));
      this.resetContextMenuState_();
    }
  };

  private resetContextMenuState_() {
    if (this.contextMenuState_?.isListening) {
      this.contextMenuState_.longPressTimer.clearTimeout();

      if (this.contextMenuState_.activeElement) {
        this.contextMenuState_.activeElement.removeEventListener(
            'pointermove', this.onPointermove_);
        if (this.contextMenuState_.activePointerId !==
            PressHandler.NO_ACTIVE_POINTER_ID) {
          if (this.contextMenuState_.activeElement.hasPointerCapture(
                  this.contextMenuState_.activePointerId)) {
            this.contextMenuState_.activeElement.releasePointerCapture(
                this.contextMenuState_.activePointerId);
          }
          this.contextMenuState_.activePointerId =
              PressHandler.NO_ACTIVE_POINTER_ID;
        }
      }

      this.contextMenuState_.activeElement = null;
      this.contextMenuState_.isListening = false;
    }
  }

  private shouldCancelClick_(e: PointerEvent): boolean {
    if (!this.contextMenuState_?.activeElement) {
      // If drag-to-open-menu is not enabled or we're not actively tracking an
      // element, don't cancel the click.
      return false;
    }
    const rect = this.contextMenuState_.activeElement.getBoundingClientRect();
    return e.clientX < rect.left || e.clientX > rect.right ||
        e.clientY < rect.top || e.clientY > rect.bottom;
  }

  onPointerdown = (e: PointerEvent, skipLongPress: boolean = false) => {
    // Ignore secondary pointers if we are actively listening to a primary
    // pointer.
    if (this.contextMenuState_?.isListening) {
      return;
    }

    if (isMac && e.button === BUTTON_LEFT && e.ctrlKey) {
      this.onLongPress_(getContextMenuSourceType(e));
      return;
    }

    const target = e.currentTarget as HTMLElement;
    target.setPointerCapture(e.pointerId);

    if (e.button === BUTTON_RIGHT) {
      // The TypeScript code should only handle long press for the
      // left-click/middle-click.
      return;
    }

    if (skipLongPress || !this.contextMenuState_) {
      return;
    }

    // If can show a context menu, need to initialize `contextMenuState_` and
    // start a timer.

    this.contextMenuState_.isListening = true;
    // Use currentTarget to ensure we are capturing the button element that
    // the listener was attached to, even if the pointer is over a child
    // element (like an icon).
    this.contextMenuState_.activeElement = target;
    this.contextMenuState_.activePointerId = e.pointerId;

    this.contextMenuState_.longPressTimer.setTimeout(() => {
      // Have to release capture so that a short press will not be triggered.
      this.resetContextMenuState_();
      this.onLongPress_(MenuSourceType.kLongPress);
    }, PressHandler.LONG_PRESS_TIMER_THRESHOLD_MS);

    this.contextMenuState_.initialY = e.clientY;
    target.addEventListener('pointermove', this.onPointermove_);
  };

  onPointerup = (e: PointerEvent) => {
    const target = e.currentTarget as HTMLElement;
    // Ignore pointers that are not captured, indicating that there was no
    // corresponding pointer down event over this button.
    if (!target.hasPointerCapture(e.pointerId)) {
      return;
    }

    // It's not necessary to release captured pointers on pointer up, since that
    // will be done automatically.

    // Ignore secondary pointers if we are actively listening to a primary
    // pointer.
    if (this.contextMenuState_?.isListening &&
        e.pointerId !== this.contextMenuState_.activePointerId) {
      return;
    }

    // Do nothing and reset state on release events that were not over the event
    // target, which can happen when mouse down occurs over one element and then
    // the mouse is captured by that element, and then mouse up occurs over
    // another.
    if (!isMouseEventOverTarget(e, target)) {
      this.resetContextMenuState_();
      return;
    }

    if (e.button === BUTTON_RIGHT) {
      this.resetContextMenuState_();
      return;
    }

    // If it's Ctrl+LeftClick on Mac, skip the rest.
    if (isMac && e.button === BUTTON_LEFT && e.ctrlKey) {
      this.resetContextMenuState_();
      return;
    }

    const shouldCancel = this.shouldCancelClick_(e);
    this.resetContextMenuState_();

    if (shouldCancel) {
      return;
    }

    this.onShortPress_(e);
  };

  onPointercancel = (e: PointerEvent) => {
    // Ignore secondary pointers if we are actively listening to a primary
    // pointer.
    if (this.contextMenuState_?.isListening &&
        e.pointerId !== this.contextMenuState_.activePointerId) {
      return;
    }
    this.resetContextMenuState_();
  };

  onContextmenu = (e: PointerEvent) => {
    e.preventDefault();
    // If it's a Mac Ctrl+LeftClick, the browser natively fires a contextmenu
    // event. We already showed the menu in pointerdown. We MUST suppress the
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
    return getClickSourceType(e);
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

export function getEventDispositionFlags(
    e: MouseEvent|KeyboardEvent,
    options: GetEventDispositionFlagsOptions = {}): EventDispositionFlag[] {
  const flags: EventDispositionFlag[] = [];
  if (e instanceof MouseEvent) {
    if (e.button === BUTTON_MIDDLE) {
      flags.push(EventDispositionFlag.kMiddleMouseButton);
    }
  }
  if (e.altKey) {
    flags.push(EventDispositionFlag.kAltKeyDown);
  }
  if (e.ctrlKey && !options.ignoreCtrlKey) {
    flags.push(EventDispositionFlag.kControlKeyDown);
  }
  if (e.metaKey) {
    flags.push(EventDispositionFlag.kMetaKeyDown);
  }
  if (e.shiftKey && !options.ignoreShiftKey) {
    flags.push(EventDispositionFlag.kShiftKeyDown);
  }
  if (e.getModifierState('AltGraph')) {
    flags.push(EventDispositionFlag.kAltGrKeyDown);
  }
  return flags;
}
