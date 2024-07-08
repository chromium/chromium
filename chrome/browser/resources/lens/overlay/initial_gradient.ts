// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './initial_gradient.html.js';

export interface InitialGradientElement {
  $: {
    initialGradientScrim: HTMLElement,
  };
}

/*
 * Element responsible for showing an initial gradient.
 */
export class InitialGradientElement extends PolymerElement {
  static get is() {
    return 'initial-gradient';
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

      shouldHideScrim: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  // Whether or not the gradient scrim is visible.
  private scrimVisible: boolean;

  // Whether or not to prevent rendering the scrim. This uses CSS to
  // set the visibility to 0 which is required because transitioning the
  // opacity to 0 does not always completely fade out the gradient.
  private shouldHideScrim: boolean;

  setScrimVisible() {
    this.scrimVisible = true;
  }

  triggerHideScrimAnimation() {
    this.scrimVisible = false;
    this.shouldHideScrim = true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'initial-gradient': InitialGradientElement;
  }
}

customElements.define(InitialGradientElement.is, InitialGradientElement);
