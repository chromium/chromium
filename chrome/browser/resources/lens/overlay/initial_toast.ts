// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './initial_toast.html.js';

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
      messageVisible: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      scrimVisible: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      shouldHideMessage: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      shouldHideScrim: {
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

  // Whether or not to prevent rendering the message. This uses CSS to
  // set the visibility to 0 which is required because transitioning the
  // opacity to 0 does not always completely fade out the toast.
  private shouldHideMessage: boolean;

  // Whether or not to prevent rendering the scrim. This uses CSS to
  // set the visibility to 0 which is required because transitioning the
  // opacity to 0 does not always completely fade out the toast.
  private shouldHideScrim: boolean;

  override connectedCallback() {
    super.connectedCallback();
    requestAnimationFrame(() => {
      this.messageVisible = true;
      this.scrimVisible = true;
    });
  }

  triggerHideMessageAnimation() {
    this.messageVisible = false;
    this.shouldHideMessage = true;
  }

  triggerHideScrimAnimation() {
    this.scrimVisible = false;
    this.shouldHideScrim = true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'initial-toast': InitialToastElement;
  }
}

customElements.define(InitialToastElement.is, InitialToastElement);
