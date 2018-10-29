// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Helper class to be used as the super class of all custom elements in
 * chrome://omnibox.
 * @abstract
 */
class OmniboxElement extends HTMLElement {
  /** @param {string} templateId */
  constructor(templateId) {
    super();
    this.attachShadow({mode: 'open'});
    const template = OmniboxElement.getTemplate(templateId);
    this.shadowRoot.appendChild(template);
  }

  /**
   * Searches local shadow root for element by id
   * @param {string} id
   * @return {Element}
   */
  $$(id) {
    return this.shadowRoot.getElementById(id);
  }

  /**
   * @param {string} templateId
   * @return {Element}
   */
  static getTemplate(templateId) {
    return $(templateId).content.cloneNode(true);
  }
}
