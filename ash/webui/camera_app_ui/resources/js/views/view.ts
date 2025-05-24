// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists, assertInstanceof} from '../assert.js';
import {PtzController} from '../device/ptz_controller.js';
import * as dom from '../dom.js';
import {I18nString} from '../i18n_string.js';
import * as state from '../state.js';
import {ViewName} from '../type.js';
import {KeyboardShortcut} from '../util.js';
import {WaitableEvent} from '../waitable_event.js';

export interface DialogEnterOptions {
  /**
   * Whether the dialog view is cancellable.
   */
  cancellable?: boolean;
  /**
   * Description of the dialog.
   */
  description?: I18nString;
  /**
   * Message of the dialog view.
   */
  message?: string;
  /**
   * Title of the dialog.
   */
  title?: I18nString;
}

/**
 * Warning message name.
 */
type WarningEnterOptions = string;

/**
 * Flash view processing message name.
 */
export type FlashEnterOptions = string;

/**
 * Options for open PTZ panel.
 */
export class PtzPanelOptions {
  readonly ptzController: PtzController;

  constructor(ptzController: PtzController) {
    this.ptzController = ptzController;
  }
}

export interface StateOption {
  readonly label: I18nString;

  readonly ariaLabel: I18nString;

  readonly state: state.State;

  readonly isDisableOption?: boolean;
}

/**
 * Options for open Option panel.
 */
export class OptionPanelOptions {
  readonly triggerButton: HTMLElement;

  readonly titleLabel: I18nString;

  readonly stateOptions: StateOption[];

  readonly onStateChanged: (newState: state.State|null) => void;

  readonly ariaDescribedByElement: HTMLElement;

  constructor({
    triggerButton,
    titleLabel,
    stateOptions,
    onStateChanged,
    ariaDescribedByElement,
  }: {
    triggerButton: HTMLElement,
    titleLabel: I18nString,
    stateOptions: StateOption[],
    onStateChanged: (newState: state.State|null) => void,
    ariaDescribedByElement: HTMLElement,
  }) {
    this.triggerButton = triggerButton;
    this.titleLabel = titleLabel;
    this.stateOptions = stateOptions;
    this.onStateChanged = onStateChanged;
    this.ariaDescribedByElement = ariaDescribedByElement;
  }
}

// TODO(pihsun): After we migrate all files into TypeScript, we can have some
// sort of "global" view registration, so we can enforce the enter / leave type
// at compile time.
export type EnterOptions = DialogEnterOptions|FlashEnterOptions|
    OptionPanelOptions|PtzPanelOptions|WarningEnterOptions;

export type LeaveCondition = {
  kind: 'BACKGROUND_CLICKED'|'ESC_KEY_PRESSED'|'STREAMING_STOPPED',
}|{
  kind: 'CLOSED',
  val?: unknown,
};

interface ViewOptions {
  /**
   * Enables dismissible by Esc-key.
   */
  dismissByEsc?: boolean;

  /**
   * Enables dismissible by background-click.
   */
  dismissByBackgroundClick?: boolean;

  /**
   * Selects element to be focused in focus(). Focus to first element whose
   * tabindex is not -1 when argument is not presented.
   */
  defaultFocusSelector?: string;

  /**
   * Close the view when the it's opened and the camera stops streaming.
   */
  dismissOnStopStreaming?: boolean;
}

/**
 * Base controller of a view for views' navigation sessions (nav.ts).
 */
export class View {
  root: HTMLElement;

  /**
   * Signal it to ends the session.
   */
  private session: WaitableEvent<LeaveCondition>|null = null;

  private readonly dismissByEsc: boolean;

  private readonly defaultFocusSelector: string;

  protected lastFocusedElement: HTMLElement|null = null;

  /**
   * @param name Unique name of view which should be same as its DOM element id.
   */
  constructor(readonly name: ViewName, {
    dismissByEsc = false,
    dismissByBackgroundClick = false,
    defaultFocusSelector = '[tabindex]:not([tabindex="-1"])',
    dismissOnStopStreaming = false,
  }: ViewOptions = {}) {
    this.root = dom.get(`#${name}`, HTMLElement);
    this.dismissByEsc = dismissByEsc;
    this.defaultFocusSelector = defaultFocusSelector;

    if (dismissByBackgroundClick) {
      this.root.addEventListener('click', (event) => {
        if (event.target === this.root) {
          this.leave({kind: 'BACKGROUND_CLICKED'});
        }
      });
    }

    if (dismissOnStopStreaming) {
      state.addObserver(state.State.STREAMING, (streaming) => {
        if (!streaming && state.get(this.name)) {
          this.leave({kind: 'STREAMING_STOPPED'});
        }
      });
    }
  }

  /**
   * Gets sub-views nested under this view.
   */
  getSubViews(): View[] {
    return [];
  }

  /**
   * Hook of the subclass for handling the key.
   *
   * @param _key Key to be handled.
   * @return Whether the key has been handled or not.
   */
  handlingKey(_key: string): boolean {
    return false;
  }

  /**
   * Handles the pressed key.
   *
   * @param key Key to be handled.
   * @return Whether the key has been handled or not.
   */
  onKeyPressed(key: KeyboardShortcut): boolean {
    if (this.handlingKey(key)) {
      return true;
    } else if (this.dismissByEsc && key === 'Escape') {
      this.leave({kind: 'ESC_KEY_PRESSED'});
      return true;
    }
    return false;
  }

  /**
   * Deactivates the view to be unfocusable.
   */
  protected setUnfocusable(): void {
    this.root.setAttribute('aria-hidden', 'true');
    for (const element of dom.getAllFrom(
             this.root, '[tabindex]', HTMLElement)) {
      element.dataset['tabindex'] =
          assertExists(element.getAttribute('tabindex'));
      element.setAttribute('tabindex', '-1');
    }
    const activeElement = document.activeElement;
    if (activeElement instanceof HTMLElement) {
      activeElement.blur();
    }
  }

  /**
   * Activates the view to be focusable.
   */
  protected setFocusable(): void {
    this.root.setAttribute('aria-hidden', 'false');
    for (const element of dom.getAllFrom(
             this.root, '[tabindex]', HTMLElement)) {
      if (element.dataset['tabindex'] === undefined) {
        // First activation, no need to restore tabindex from data-tabindex.
        continue;
      }
      element.setAttribute('tabindex', element.dataset['tabindex']);
      element.removeAttribute('data-tabindex');
    }
  }

  /**
   * The view is newly shown as the topmost view.
   */
  onShownAsTop(): void {
    this.setFocusable();
    // Focus on the default selector on enter.
    const el = this.root.querySelector(this.defaultFocusSelector);
    if (el !== null) {
      assertInstanceof(el, HTMLElement).focus();
    }
  }

  /**
   * The view was the topmost shown view and is being hidden.
   */
  onHideAsTop(): void {
    this.lastFocusedElement = null;
    this.setUnfocusable();
  }

  /**
   * The view was the topmost shown view and is being covered by newly shown
   * view.
   */
  onCoveredAsTop(): void {
    this.lastFocusedElement = document.activeElement === null ?
        null :
        assertInstanceof(document.activeElement, HTMLElement);
    this.setUnfocusable();
  }

  /**
   * The view becomes the new topmost shown view after some upper view is
   * hidden.
   *
   * @param _viewName The name of the upper view that is hidden.
   */
  onUncoveredAsTop(_viewName: ViewName): void {
    this.setFocusable();
    // Focus on last focused element or default selector
    if (this.lastFocusedElement !== null) {
      this.lastFocusedElement.focus();
      this.lastFocusedElement = null;
    } else {
      const el = this.root.querySelector(this.defaultFocusSelector);
      if (el !== null) {
        assertInstanceof(el, HTMLElement).focus();
      }
    }
  }

  /**
   * Layouts the view.
   */
  layout(): void {
    // To be overridden by subclasses.
  }

  /**
   * Hook of the subclass for entering the view.
   *
   * @param _options Optional rest parameters for entering the view.
   */
  protected entering(_options?: EnterOptions): void {
    // To be overridden by subclasses.
  }

  /**
   * Enters the view.
   *
   * @param options Optional rest parameters for entering the view.
   * @return Promise for the navigation session.
   */
  enter(options?: EnterOptions): Promise<LeaveCondition> {
    // The session is started by entering the view and ended by leaving the
    // view.
    if (this.session === null) {
      this.session = new WaitableEvent();
    }
    this.entering(options);
    return this.session.wait();
  }

  /**
   * Hook of the subclass for leaving the view.
   *
   * @return Whether able to leave the view or not.
   */
  protected leaving(_condition: LeaveCondition): boolean {
    return true;
  }

  /**
   * Leaves the view.
   *
   * @param condition Optional condition for leaving the view and also as
   *     the result for the ended session.
   */
  leave(condition: LeaveCondition = {kind: 'CLOSED'}): void {
    if (this.session !== null && this.leaving(condition)) {
      this.session.signal(condition);
      this.session = null;
    }
  }
}
