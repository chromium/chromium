// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './page_favicon.js';
import './shared_vars.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {Visit} from '/components/memories/core/memories.mojom-webui.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {decodeMojoString16, getHostnameFromUrl} from './utils.js';

/**
 * @fileoverview This file provides a custom element displaying a visit to a
 * page within a Memory. A visit features the page favicon, title, a timestamp,
 * as well as an action menu.
 */

class VisitRowElement extends PolymerElement {
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
   * Converts a Mojo String16 to a JS string.
   * @param {String16} str
   * @return {string}
   * @private
   */
  decodeMojoString16_(str) {
    return decodeMojoString16(str);
  }

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
    return decodeMojoString16(
        this.isTopVisit ? visit.relativeDate : visit.timeOfDay);
  }
}

customElements.define(VisitRowElement.is, VisitRowElement);
