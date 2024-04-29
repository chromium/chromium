// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './initial_toast.html.js';

const MESSAGE_FADE_IN_TIME_MS = 300;
const SCRIM_FADE_IN_TIME_MS = 200;
const FADE_OUT_TIME_MS = 10000;

export interface InitialToastElement {
  $: {
    initialToast: HTMLElement,
    initialToastScrim: HTMLElement,
  };
}

/*
 * Element responsible for showing an initial toast.
 */
export class InitialToastElement extends PolymerElement {
  static get is() {
    return 'initial-toast';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      scrimVisible: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      messageVisible: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  // Whether or not the toast message is visible.
  private messageVisible: boolean;

  // Whether or not the toast scrim is visible.
  private scrimVisible: boolean;

  override connectedCallback() {
    super.connectedCallback();
    setTimeout(() => {
      this.messageVisible = true;
    }, MESSAGE_FADE_IN_TIME_MS);
    setTimeout(() => {
      this.scrimVisible = true;
    }, SCRIM_FADE_IN_TIME_MS);
    setTimeout(this.triggerHideAnimation.bind(this), FADE_OUT_TIME_MS);
  }

  triggerHideAnimation() {
    this.messageVisible = false;
    this.scrimVisible = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'initial-toast': InitialToastElement;
  }
}

customElements.define(InitialToastElement.is, InitialToastElement);
