// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * settings-idle-load is a simple variant of dom-if designed for lazy
 * loading and rendering of elements that are accessed imperatively. A URL is
 * given that holds the elements to be loaded lazily.
 */

import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement, TemplateInstanceBase, templatize} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ensureLazyLoaded} from '../ensure_lazy_loaded.js';

import {getTemplate} from './settings_idle_load.html.js';

export class SettingsIdleLoadElement extends PolymerElement {
  static get is() {
    return 'settings-idle-load';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  private child_: Element|null;
  private instance_: TemplateInstanceBase|null;
  private loading_: Promise<Element>|null;
  private idleCallback_: number;

  constructor() {
    super();

    this.child_ = null;
    this.instance_ = null;
    this.loading_ = null;
    this.idleCallback_ = 0;
  }


  override connectedCallback(): void {
    super.connectedCallback();

    this.idleCallback_ = requestIdleCallback(() => {
      this.get();
    });
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    // No-op if callback already fired.
    window.cancelIdleCallback(this.idleCallback_);
  }

  /**
   * @param requestFn Requests the lazy module.
   * @return Resolves with the stamped child element after the lazy module has
   *    been loaded.
   */
  private requestLazyModule_(): Promise<Element> {
    return new Promise((resolve, reject) => {
      ensureLazyLoaded().then(() => {
        const slot = this.shadowRoot!.querySelector('slot');
        assert(slot);
        const template =
            slot.assignedNodes({flatten: true})
                .filter(n => n.nodeType === Node.ELEMENT_NODE)[0] as
            HTMLTemplateElement;

        const TemplateClass = templatize(template, this, {
          mutableData: false,
          forwardHostProp: this._forwardHostPropV2,
        });

        this.instance_ = new TemplateClass();

        assert(!this.child_);
        this.child_ = this.instance_.root.firstElementChild;
        assert(this.child_);

        this.parentNode!.insertBefore(this.instance_.root, this);
        resolve(this.child_);

        const event =
            new CustomEvent('lazy-loaded', {bubbles: true, composed: true});
        this.dispatchEvent(event);
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
  private _forwardHostPropV2(prop: string, value: any): void {
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
