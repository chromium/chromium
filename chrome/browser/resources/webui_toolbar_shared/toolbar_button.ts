// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import type {HelpBubbleMixinInterface} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_interface.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import {isMac} from 'chrome://resources/js/platform.js';

import {EventDispositionFlag} from './browser_controls_api_data_model.mojom-webui.js';
import {TimerHelper} from './timer_helper.js';

export const BUTTON_LEFT = 0;
export const BUTTON_MIDDLE = 1;
export const BUTTON_RIGHT = 2;

/**
 * Returns true if `e` is a Mac Ctrl+LeftClick, which macOS treats as a
 * secondary (context menu) click rather than a primary click.
 */
export function isMacContextClick(e: MouseEvent): boolean {
  return isMac && e.button === BUTTON_LEFT && e.ctrlKey;
}

/**
 * Returns true if `e` is a primary (left) button interaction, excluding Mac
 * Ctrl+LeftClick which macOS treats as a secondary (context menu) click.
 */
export function isPrimaryClick(e: MouseEvent): boolean {
  return e.button === BUTTON_LEFT && !isMacContextClick(e);
}


export interface GetEventDispositionFlagsOptions {
  ignoreCtrlKey?: boolean;
  ignoreShiftKey?: boolean;
}

type Constructor<T> = new (...args: any[]) => T;

export interface HelpBubbleAnchor {
  hasHelpBubble: boolean;
  adjustTooltipForHelpBubble(original: string): string;
}

/**
 * A mixin for components that can be targeted by a help bubble (IPH).
 * Centralizes the `hasHelpBubble` property and tooltip suppression logic.
 */
export const HelpBubbleAnchorMixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<HelpBubbleMixinInterface>&Constructor<HelpBubbleAnchor> => {
      class HelpBubbleAnchorMixin extends HelpBubbleMixinLit
      (superClass) {
        static get properties() {
          return {
            hasHelpBubble: {
              type: Boolean,
            },
          };
        }

        accessor hasHelpBubble: boolean = false;

        /**
         * Helper to suppress the tooltip when a help bubble is visible.
         *
         * @param original The original tooltip text.
         * @returns The tooltip text to display.
         */
        adjustTooltipForHelpBubble(original: string): string {
          return this.hasHelpBubble ? '' : original;
        }
      }

      return HelpBubbleAnchorMixin as T & Constructor<HelpBubbleMixinInterface>&
          Constructor<HelpBubbleAnchor>;
    };

export function setHasHelpBubble(el: Element, value: boolean) {
  (el as unknown as HelpBubbleAnchor).hasHelpBubble = value;
}

/**
 * Tracks whether a bubble is anchored to an element, as indicated by the
 * TrackedElementManager highlighting the element, in order to avoid reopening
 * a bubble when the pointerdown that dismissed it (due to focus loss) turns
 * into a click on the element.
 *
 * Usage:
 * - Pass `onHighlightChanged` as the `onHighlightChanged` option when
 *   registering the element with the TrackedElementManager (e.g. via
 *   registerHelpBubble()).
 * - Use onPointerdown() as (or call it from) the element's `pointerdown`
 *   handler.
 * - Call shouldSkipClick() from the element's click handler, and ignore the
 *   click if it returns true.
 */
export class HighlightTracker {
  // How long after an element is unhighlighted a pointerdown on it is still
  // assumed to be the one that dismissed the bubble. Sometimes the IPC to
  // remove highlighting (due to the pointerdown) can arrive before the
  // pointerdown event itself, so allow for this race. 100ms matches
  // views::kMinimumTimeBetweenButtonClicks.
  private static clickSuppressionThresholdMs_: number = 100;

  /**
   * Overrides the window during which HighlightTracker skips clicks following
   * an unhighlight. Tests that programmatically close a bubble and immediately
   * click its anchor again can pass 0 to disable the post-unhighlight window;
   * clicks while the element is still highlighted are skipped regardless.
   */
  static setClickSuppressionThresholdForTesting(thresholdMs: number) {
    HighlightTracker.clickSuppressionThresholdMs_ = thresholdMs;
  }

  // Set if the TrackedElementManager indicates the element should be
  // highlighted. Public for testing.
  highlighted: boolean = false;
  // The value of performance.now() when `highlighted` last went from true to
  // false, or -Infinity if that has not happened. Public for testing.
  lastUnhighlightedTime: number = -Infinity;

  private skipNextClick_: boolean = false;

  onHighlightChanged = (highlighted: boolean) => {
    if (this.highlighted && !highlighted) {
      this.lastUnhighlightedTime = performance.now();
    }
    this.highlighted = highlighted;
  };

  /**
   * Determines whether a click following pointerdown event `e` should be
   * skipped, because the pointerdown dismissed a bubble anchored to the
   * element. An arrow function so that it can be bound directly as an event
   * listener.
   */
  onPointerdown = (e: PointerEvent) => {
    // Ignore non-primary clicks or clicks not coming from a pointer (i.e.
    // keyboard).
    if (!isPrimaryClick(e) || e.pointerType === '') {
      this.skipNextClick_ = false;
      return;
    }
    this.skipNextClick_ = this.highlighted ||
        (performance.now() - this.lastUnhighlightedTime <
         HighlightTracker.clickSuppressionThresholdMs_);
  };

  /**
   * Returns true if click `e` should be ignored, because the pointerdown that
   * started it dismissed a bubble anchored to the element. Keyboard clicks are
   * never skipped.
   */
  shouldSkipClick(e: PointerEvent): boolean {
    if (this.skipNextClick_ && e.pointerType !== '') {
      this.skipNextClick_ = false;
      return true;
    }
    return false;
  }
}

// Tracks state used for deciding whether to display a context menu instead of
// treating a pointer interaction as a click (a "short press"). Only populated
// while holding a pointer down, cleared on release, or when the interaction is
// determined to be a "long press", which generally results in showing a context
// menu.
//
// A press may be determined to be long based on either press duration or
// vertical mouse movement. If long presses are not allowed at that point, long
// press detection stops, but mouse presses continue to be tracked, so releasing
// the pointer results in a short press. Touch and pen presses are not tracked
// any further, so they never result in a short press, matching Views.
interface ContextMenuState {
  // True as long as the pointer is down, and the interaction has not been
  // passed along as a long or short press. May remain true after the press
  // ends, if no pointerup or pointercancel event is received. See
  // `resetStalePress_()`.
  isListening: boolean;
  activePointerId: number;
  activePointerType: string;
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
 * Consumers that can't always show a context menu should pass a `canLongPress`
 * predicate to the constructor, rather than ignoring calls to `onLongPress`, so
 * that holding or dragging the mouse still results in a short press on release
 * when a menu can't be shown.
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
  // This follows what is done in the views' ToolbarButton. Public for tests.
  static readonly LONG_PRESS_TIMER_THRESHOLD_MS = 500;
  static readonly DRAG_THRESHOLD_PX = 8;
  private static readonly NO_ACTIVE_POINTER_ID = -1;

  private onLongPress_: (source: MenuSourceType) => void;
  private onShortPress_: (e: PointerEvent) => void;
  private canLongPress_: () => boolean;

  private contextMenuState_: ContextMenuState = {
    isListening: false,
    activePointerId: PressHandler.NO_ACTIVE_POINTER_ID,
    activePointerType: '',
    activeElement: null,
    longPressTimer: new TimerHelper(),
    initialY: 0,
  };

  // Do we expect the pointer to be captured between pointerDown and pointerUp?
  // Mostly true, but false for screen reader synthetic mouse clicks.
  private expectPointerCapture_: boolean = true;

  // `canLongPress` is queried whenever a long press would otherwise be
  // triggered. If it returns false, `onLongPress` is not called. Mouse presses
  // held or dragged long enough to be long presses are then treated as short
  // presses if the pointer is released over the button, while touch and pen
  // presses are ignored, matching Views.
  constructor(
      onLongPress: (source: MenuSourceType) => void,
      onShortPress: (e: PointerEvent) => void,
      canLongPress: () => boolean = () => true) {
    this.onLongPress_ = onLongPress;
    this.onShortPress_ = onShortPress;
    this.canLongPress_ = canLongPress;
  }

  private onPointermove_ = (e: PointerEvent) => {
    if (e.pointerId !== this.contextMenuState_.activePointerId) {
      return;
    }
    // Detect downward drag.
    if (e.clientY - this.contextMenuState_.initialY >
        PressHandler.DRAG_THRESHOLD_PX) {
      this.onLongPressDetected_(getContextMenuSourceType(e));
    }
  };

  // Called when the tracked press is held or dragged long enough to become a
  // long press. If long presses are allowed, stops tracking the press and
  // invokes `onLongPress_`. Otherwise, for touch and pen presses, stops
  // tracking the press, so it will be ignored on release, matching Views. For
  // other presses, stops long press detection, but keeps tracking the press,
  // so it will be treated as a short press on release.
  private onLongPressDetected_(source: MenuSourceType) {
    assert(this.contextMenuState_.isListening);
    this.resetStalePress_();
    if (!this.contextMenuState_.isListening) {
      return;
    }
    if (this.canLongPress_()) {
      // Have to release capture so that a short press will not be triggered.
      this.resetContextMenuState_();
      this.onLongPress_(source);
      return;
    }
    const pointerType = this.contextMenuState_.activePointerType;
    if (pointerType === 'touch' || pointerType === 'pen') {
      // Releasing capture means the press will be ignored on release.
      this.resetContextMenuState_();
      return;
    }
    this.contextMenuState_.longPressTimer.clearTimeout();
    this.contextMenuState_.activeElement?.removeEventListener(
        'pointermove', this.onPointermove_);
  }

  // Resets state if a press is being tracked, but it ended without a pointerup
  // or pointercancel reaching us. This can happen if the element is removed
  // from the DOM mid-press, or otherwise loses pointer capture.
  private resetStalePress_() {
    const {isListening, activeElement, activePointerId} =
        this.contextMenuState_;
    if (!isListening) {
      return;
    }
    const isLive = !!activeElement?.isConnected &&
        (!this.expectPointerCapture_ ||
         activeElement.hasPointerCapture(activePointerId));
    if (!isLive) {
      this.resetContextMenuState_();
    }
  }

  private resetContextMenuState_() {
    if (this.contextMenuState_.isListening) {
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
    if (!this.contextMenuState_.activeElement) {
      // If we're not actively tracking an element, don't cancel the click.
      return false;
    }
    const rect = this.contextMenuState_.activeElement.getBoundingClientRect();
    return e.clientX < rect.left || e.clientX > rect.right ||
        e.clientY < rect.top || e.clientY > rect.bottom;
  }

  onPointerdown = (e: PointerEvent) => {
    // Ignore secondary pointers if we are actively listening to a primary
    // pointer. If the primary pointer's press ended without us being notified,
    // drop its state rather than ignoring all future presses.
    this.resetStalePress_();
    if (this.contextMenuState_.isListening) {
      return;
    }

    // On Mac, Ctrl+LeftClick is a Context Menu action. We return early to
    // bypass pointer capture and short-press logic, and let the native
    // contextmenu event handle it.
    if (isMacContextClick(e)) {
      return;
    }

    const target = e.currentTarget as HTMLElement;
    target.setPointerCapture(e.pointerId);

    // With some screen readers, mouse clicks coming from the keyboard aren't
    // able to hold pointer capture as the pointer isn't involved.
    this.expectPointerCapture_ = target.hasPointerCapture(e.pointerId);

    if (e.button === BUTTON_RIGHT) {
      // The TypeScript code should only handle long press for the
      // left-click/middle-click.
      return;
    }

    // Start tracking the press, and start the long press timer. Whether a long
    // press is currently allowed is only checked when a long press is
    // detected, since that may change during the press.

    this.contextMenuState_.isListening = true;
    // Use currentTarget to ensure we are capturing the button element that
    // the listener was attached to, even if the pointer is over a child
    // element (like an icon).
    this.contextMenuState_.activeElement = target;
    this.contextMenuState_.activePointerId = e.pointerId;
    this.contextMenuState_.activePointerType = e.pointerType;

    this.contextMenuState_.longPressTimer.setTimeout(() => {
      this.onLongPressDetected_(MenuSourceType.kLongPress);
    }, PressHandler.LONG_PRESS_TIMER_THRESHOLD_MS);

    this.contextMenuState_.initialY = e.clientY;
    target.addEventListener('pointermove', this.onPointermove_);
  };

  onPointerup = (e: PointerEvent) => {
    this.resetStalePress_();

    const target = e.currentTarget as HTMLElement;
    // Ignore pointers that are not captured, indicating that there was no
    // corresponding pointer down event over this button.
    if (this.expectPointerCapture_ && !target.hasPointerCapture(e.pointerId)) {
      return;
    }

    // It's not necessary to release captured pointers on pointer up, since that
    // will be done automatically.

    // Ignore secondary pointers if we are actively listening to a primary
    // pointer.
    if (this.contextMenuState_.isListening &&
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

    // If it's Ctrl+LeftClick on Mac, abort the click.
    if (isMacContextClick(e)) {
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
    if (this.contextMenuState_.isListening &&
        e.pointerId !== this.contextMenuState_.activePointerId) {
      return;
    }
    this.resetContextMenuState_();
  };

  onContextmenu = (e: PointerEvent) => {
    e.preventDefault();
    // For Mac Ctrl+LeftClick, the browser natively fires this event immediately
    // after pointerdown (no wait for pointerup), perfectly matching Views'
    // context menu timing.
    if (this.canLongPress_()) {
      this.onLongPress_(getContextMenuSourceType(e));
    }
  };
}

export function getContextMenuPosition(element: HTMLElement) {
  // Returns the element's bounding rectangle relative to the WebUI viewport in
  // CSS pixels.
  return element.getBoundingClientRect();
}

// `PointerEvent.button` value when no button is involved (e.g. a
// keyboard-triggered `contextmenu` event).
const BUTTON_NONE = -1;

// Should match ui::GetMenuSourceTypeForEvent().
// * keyboard event -> keyboard
// * touch event -> touch
// * others -> mouse
export function getClickSourceType(e: Event): MenuSourceType {
  if (e instanceof PointerEvent &&
      (e.pointerType === 'touch' || e.pointerType === 'pen')) {
    return MenuSourceType.kTouch;
  }
  // Keyboard activations on `<cr-icon-button>` or `<button>` (Space or Enter)
  // programmatically synthesize standard `click` events with `detail` 0, while
  // direct keydown activations (e.g. ArrowDown/ArrowUp) pass a `KeyboardEvent`.
  // Only `click` events are checked for `detail` 0: other mouse/pointer events
  // (e.g. pointerdown, pointerup, pointermove, mousemove) natively have
  // `detail` 0 even for physical interactions, so they must not be classified
  // as keyboard.
  if (e instanceof KeyboardEvent ||
      (e instanceof MouseEvent && e.type === 'click' && e.detail === 0)) {
    return MenuSourceType.kKeyboard;
  }
  return MenuSourceType.kMouse;
}

// Should match ui::GetMenuSourceTypeForEvent().
// * keyboard event -> keyboard
// * touch/gesture event -> touch
// * others -> mouse
export function getContextMenuSourceType(e: Event): MenuSourceType {
  if (e.type !== 'contextmenu') {
    return getClickSourceType(e);
  }
  if (e instanceof PointerEvent &&
      (e.pointerType === 'touch' || e.pointerType === 'pen')) {
    return MenuSourceType.kTouch;
  }
  // Blink never populates `detail` for `contextmenu` events, so use `button`
  // instead: keyboard-triggered context menus (e.g. Menu key or Shift+F10) have
  // no button (reported as -1 on `PointerEvent` and 0 on `MouseEvent`) and no
  // modifiers. Mac Ctrl+Click (button 0 + ctrlKey) is a mouse interaction.
  if (e instanceof MouseEvent &&
      (e.button === BUTTON_NONE || e.button === BUTTON_LEFT) && !e.ctrlKey) {
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

export function roundedIconsEnabled() {
  return loadTimeData.getBoolean('roundedIconsEnabled');
}

/**
 * Triggers programmatic SMIL animations within the shadow root of a cr-icon
 * that is nested inside the given button. Filtering by begin="indefinite"
 * ensures that sequentially chained or auto-running timed animations are not
 * inadvertently force-started simultaneously.
 */
export function playIconAnimation(button: CrLitElement) {
  const crIcon = button.shadowRoot.querySelector<CrLitElement>('cr-icon');
  assert(crIcon);
  const animates = crIcon.shadowRoot.querySelectorAll<
      SVGAnimateElement|SVGAnimateTransformElement|SVGAnimateMotionElement>(
      'animate[begin="indefinite"], ' +
      'animateTransform[begin="indefinite"], ' +
      'animateMotion[begin="indefinite"]');
  animates.forEach(animate => animate.beginElement());
}
