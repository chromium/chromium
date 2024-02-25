// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * cr-lazy-render is a simple variant of dom-if designed for lazy rendering
 * of elements that are accessed imperatively.
 * Usage:
 *   <cr-lazy-render id="menu">
 *     <template>
 *       <heavy-menu></heavy-menu>
 *     </template>
 *   </cr-lazy-render>
 *
 *   this.$.menu.get().show();
 *
 * Forked from
 * ui/webui/resources/cr_elements/cr_lazy_render/cr_lazy_render.ts
 */

import {assert} from '//resources/js/assert.js';
import {html, PolymerElement, TemplateInstanceBase, templatize} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class CrLazyRenderElement<T extends HTMLElement> extends PolymerElement {
  static get is() {
    return 'cr-lazy-render';
  }

  static get template() {
    return html`<slot></slot>`;
  }

  private child_: T|null = null;
  private instance_: TemplateInstanceBase|null = null;

  /**
   * Stamp the template into the DOM tree synchronously
   * @return Child element which has been stamped into the DOM tree.
   */
  override get(): T {
    if (!this.child_) {
      this.render_();
    }
    assert(this.child_);
    return this.child_;
  }

  /**
   * @return The element contained in the template, if it has
   *   already been stamped.
   */
  getIfExists(): (T|null) {
    return this.child_;
  }

  private render_() {
    const template =
        (this.shadowRoot!.querySelector('slot')!.assignedNodes({flatten: true})
             .filter(n => n.nodeType === Node.ELEMENT_NODE)[0]) as
        HTMLTemplateElement;

    const TemplateClass = templatize(template, this, {
      mutableData: false,
      forwardHostProp: this._forwardHostPropV2,
    });
    const parentNode = this.parentNode;
    if (parentNode && !this.child_) {
      this.instance_ = new TemplateClass();
      this.child_ = this.instance_.root.firstElementChild as T;
      parentNode.insertBefore(this.instance_.root, this);
    }
  }

  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  _forwardHostPropV2(prop: string, value: object) {
    if (this.instance_) {
      this.instance_.forwardHostProp(prop, value);
    }
  }
}

customElements.define(CrLazyRenderElement.is, CrLazyRenderElement);
