// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Simple OOBE polymer element which is used for displaying single slide in a
 * carousel.
 *
 * Example:
 *   <oobe-slide is-warning>
 *     <img ... slot="slide-img">
 *     <div slot="title">...</div>
 *     <div slot="text">...</div>
 *   </oobe-slide>
 *
 *   Attributes:
 *     is-warning - changes title slot color from blue to red.
 */

import './common_styles/oobe_dialog_host_styles.css.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './oobe_slide.html.js';

class OobeSlide extends PolymerElement {
  static get is() {
    return 'oobe-slide' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /* If true title is colored red, else blue.
       */
      isWarning: {
        type: Boolean,
        value: false,
      },
    };
  }

  private isWarning: boolean;
}

declare global {
  interface HTMLElementTagNameMap {
    [OobeSlide.is]: OobeSlide;
  }
}

customElements.define(OobeSlide.is, OobeSlide);
