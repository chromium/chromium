// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './page_favicon.js';
import './shared_vars.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {Visit} from '/components/history_clusters/core/memories.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MojomConversionMixinBase} from './mojom_conversion_mixin.js';
import {getHostnameFromUrl} from './utils.js';

/**
 * @fileoverview This file provides a custom element displaying a visit to a
 * page within a Memory. A visit features the page favicon, title, a timestamp,
 * as well as an action menu.
 */

/** @polymer */
class VisitRowElement extends MojomConversionMixinBase {
  static get is() {
    return 'visit-row';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      /**
       * The visit to display.
       * @type {!Visit}
       */
      visit: Object,

      /**
       * Whether the visit is a top visit.
       * @type {boolean}
       */
      isTopVisit: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  //============================================================================
  // Event handlers
  //============================================================================

  /**
   * @param {!MouseEvent} event
   * @private
   */
  onClick_(event) {
    // Only handle main (usually the left) and auxiliary (usually the wheel or
    // the middle) button presses.
    if (event.button > 1) {
      return;
    }

    this.dispatchEvent(new CustomEvent('visit-click', {
      bubbles: true,
      composed: true,
      detail: {event},
    }));
  }

  //============================================================================
  // Helper methods
  //============================================================================

  /**
   * @param {!Url} url
   * @return {string} The domain name of the URL without the leading 'www.'.
   * @private
   */
  getHostnameFromUrl_(url) {
    return getHostnameFromUrl(url);
  }

  /**
   * @param {!Visit} visit
   * @return {string} Time of day or relative date of visit, e.g., "1 day ago",
   *     depending on if the visit is a top visit.
   * @private
   */
  getTimeOfVisit_(visit) {
    return this.decodeMojoString16(
        this.isTopVisit ? visit.relativeDate : visit.timeOfDay);
  }
}

customElements.define(VisitRowElement.is, VisitRowElement);
