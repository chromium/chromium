// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * settings-idle-load is a simple variant of dom-if designed for lazy
 * loading and rendering of elements that are accessed imperatively. A URL is
 * given that holds the elements to be loaded lazily.
 */
import {assert} from '//resources/js/assert.js';
import type {TemplateInstanceBase} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {html, PolymerElement, templatize} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ensureLazyLoaded} from '../ensure_lazy_loaded.js';

export class SettingsIdleLoadElement extends PolymerElement {
  static get is() {
    return 'settings-idle-load';
  }

  static get template() {
    return html`<slot></slot>`;
  }

  private child_: Element|null = null;
  private instance_: TemplateInstanceBase|null = null;
  private idleCallback_: number = 0;
  private loading_: Promise<Element>|null = null;

  override connectedCallback() {
    super.connectedCallback();

    this.idleCallback_ = requestIdleCallback(() => {
      this.get();
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    // No-op if callback already fired.
    cancelIdleCallback(this.idleCallback_);
  }

  /**
   * @return Resolves with the stamped child element after
   *     the lazy module has been loaded.
   */
  private requestLazyModule_(): Promise<Element> {
    return new Promise((resolve, reject) => {
      ensureLazyLoaded().then(() => {
        const template =
            (this.shadowRoot!.querySelector('slot')!
                 .assignedNodes({flatten: true})
                 .filter(n => n.nodeType === Node.ELEMENT_NODE)[0]) as
            HTMLTemplateElement;

        const TemplateClass = templatize(template, this, {
          mutableData: false,
          forwardHostProp: this._forwardHostPropV2,
        });

        this.instance_ = new TemplateClass();

        assert(!this.child_);
        this.child_ = this.instance_.root.firstElementChild;

        this.parentNode!.insertBefore(this.instance_.root, this);
        resolve(this.child_!);

        this.dispatchEvent(
            new CustomEvent('lazy-loaded', {bubbles: true, composed: true}));
      }, reject);
    });
  }

  /**
   * @return Child element which has been stamped into the DOM tree.
   */
  override get(): Promise<Element> {
    if (this.loading_) {
      return this.loading_;
    }

    this.loading_ = this.requestLazyModule_();
    return this.loading_;
  }

  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  private _forwardHostPropV2(prop: string, value: any) {
    if (this.instance_) {
      this.instance_.forwardHostProp(prop, value);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-idle-load': SettingsIdleLoadElement;
  }
}

customElements.define(SettingsIdleLoadElement.is, SettingsIdleLoadElement);
