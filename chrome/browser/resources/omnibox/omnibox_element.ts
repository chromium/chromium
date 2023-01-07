// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Helper class to be used as the super class of all custom elements in
 * chrome://omnibox.
 */
// TODO(manukh) Replace with `CustomElement` defined in
//  ui/webui/resources/js/custom_element.ts. Their essentially equivalent with
//  slightly different loading of the template element.
export abstract class OmniboxElement extends HTMLElement {
  protected constructor(templateId: string) {
    super();
    this.attachShadow({mode: 'open'});
    const template = OmniboxElement.getTemplate(templateId);
    this.shadowRoot!.appendChild(template);
  }

  /**
   * Finds the 1st element matching the query within the local shadow root.
   */
  protected $<E extends Element = Element>(query: string): E|null {
    return this.shadowRoot!.querySelector<E>(query);
  }

  /**
   * Finds all elements matching the query within the local shadow root.
   */
  protected $all<E extends Element = Element>(query: string): NodeListOf<E> {
    return this.shadowRoot!.querySelectorAll<E>(query);
  }

  /**
   * Get a template that's known to exist within the DOM document.
   */
  private static getTemplate(templateId: string): Node {
    return document.querySelector<HTMLTemplateElement>(
                       `#${templateId}`)!.content.cloneNode(true);
  }
}
