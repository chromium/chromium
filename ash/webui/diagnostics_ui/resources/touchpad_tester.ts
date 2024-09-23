// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import './diagnostics_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CanvasDrawingProvider} from './drawing_provider.js';
import {TouchDeviceInfo} from './input_data_provider.mojom-webui.js';
import {getTemplate} from './touchpad_tester.html.js';

export interface TouchpadTesterElement {
  $: {
    touchpadTesterDialog: CrDialogElement,
    testerCanvas: HTMLCanvasElement,
  };
}

const TouchpadTesterElementBase = I18nMixin(PolymerElement);

// TODO(b/253021171): Remove placeholder TouchPoint, TouchEvent, and
// TouchEventObserver when mojom updated with real types.
// See: https://goto.google.com/cros-touchpad-diagnostics-dd for intended
// mojo implementation.
interface TouchPoint {
  positionX: number;
  positionY: number;
}

interface TouchEvent {
  touchData: TouchPoint[];
}

interface TouchEventObserver {
  onTouchEvent: (event: TouchEvent) => void;
}

export class TouchpadTesterElement extends TouchpadTesterElementBase implements
    TouchEventObserver {
  static get is(): 'touchpad-tester' {
    return 'touchpad-tester' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {};
  }

  protected drawingProvider: CanvasDrawingProvider|null = null;
  // Touchpad device being tested.
  touchpad: TouchDeviceInfo|null = null;

  override connectedCallback(): void {
    super.connectedCallback();
    const ctx = this.$.testerCanvas.getContext('2d');
    assert(!!ctx);
    this.drawingProvider = new CanvasDrawingProvider(ctx);
  }

  /**
   * Resets dialog configuration to default.
   */
  close(): void {
    this.$.touchpadTesterDialog.close();
    this.touchpad = null;
  }

  /** Helper to check dialog open state. */
  isOpen(): boolean {
    assert(!!this.$.touchpadTesterDialog);
    return this.$.touchpadTesterDialog.open;
  }

  /** Setup display for requested touchpad.*/
  show(touchpad: TouchDeviceInfo): void {
    assert(!!touchpad);
    this.touchpad = touchpad;
    this.$.touchpadTesterDialog.showModal();
  }

  /** Receives TouchEventObserver events and displays on the tester canvas. */
  onTouchEvent(event: TouchEvent): void {
    assert(event);
    // TODO(b/253021171): Add call to clear canvas before drawing new touch data
    //  when drawing provider implements functionality.
    event.touchData.forEach(
        (touch: TouchPoint): void => this.drawTouchPoint(touch));
  }

  /** Visualize individual contact based on provided TouchPoint data. */
  protected drawTouchPoint(touchPoint: TouchPoint): void {
    // TODO(b/253021171): Replace placeholder call to drawing provider with call
    // to TouchDrawer when implemented.
    assert(!!this.drawingProvider);
    this.drawingProvider.drawTrailMark(
        touchPoint.positionX, touchPoint.positionY);
  }

  getDrawingProviderForTesting(): CanvasDrawingProvider {
    assert(this.drawingProvider);
    return this.drawingProvider;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [TouchpadTesterElement.is]: TouchpadTesterElement;
  }
}

customElements.define(TouchpadTesterElement.is, TouchpadTesterElement);
