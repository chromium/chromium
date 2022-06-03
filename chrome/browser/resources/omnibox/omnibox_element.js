// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/js/assert.m.js';

/**
 * Helper class to be used as the super class of all custom elements in
 * chrome://omnibox.
 * @abstract
 */
export class OmniboxElement extends HTMLElement {
  /** @param {string} templateId */
  constructor(templateId) {
    super();
    this.attachShadow({mode: 'open'});
    const template = OmniboxElement.getTemplate(templateId);
    this.shadowRoot.appendChild(template);
    /** @type {!ShadowRoot} */
    this.shadowRoot;
  }

  /**
   * Finds the 1st element matching the query within the local shadow root. At
   * least 1 matching element should exist.
   * @param {string} query
   * @return {!Element}
   */
  $(query) {
    return OmniboxElement.getByQuery_(query, this.shadowRoot);
  }

  /**
   * Finds all elements matching the query within the local shadow root.
   * @param {string} query
   * @return {!NodeList<!Element>}
   */
  $$(query) {
    return (this.shadowRoot || document).querySelectorAll(query);
  }

  /**
   * Get a template that's known to exist within the DOM document.
   * @param {string} templateId
   * @return {!Element}
   */
  static getTemplate(templateId) {
    return OmniboxElement.getByQuery_('#' + templateId).content.cloneNode(true);
  }

  /**
   * Get an element that's known to exist by query. Unlike querySelector, this
   * satisfies the JSCompiler type system.
   * @private
   * @param {string} query
   * @param {!Node=} context
   * @return {!Element}
   */
  static getByQuery_(query, context) {
    return assertInstanceof(
        (context || document).querySelector(query), Element,
        `Missing required element: ${query}`);
  }
}
