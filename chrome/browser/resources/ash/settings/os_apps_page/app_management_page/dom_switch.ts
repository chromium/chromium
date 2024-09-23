// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * app-management-dom-switch is used to select one element to be displayed at a
 * time from a group of elements. When an element is selected, it is attached
 * to the DOM. When another element is selected, the first element is
 * detached, meaning only one of the elements is attached at a time.
 *
 * The elements are selected by giving them each a route-id attribute, then
 * setting the route property of the dom-switch equal to the route-id of the
 * element to be shown.
 *
 * Data binding from the parent element of the dom-switch to its child
 * elements works as usual.
 *
 * Usage:
 *   <parent-element>
 *     <app-management-dom-switch id="viewSelector">
 *       <template>
 *         <view-one route-id="view-one" title="[[parentProperty]]"></view-one>
 *         <view-two route-id="view-two"></view-two>
 *         <view-three route-id="view-three"></view-three>
 *       </template>
 *     </app-management-dom-switch>
 *   </parent-element>
 *
 *   this.$.viewSelector.route = 'view-two';
 */

// TODO(crbug.com/40639916) Merge with cr-view-manager.
import {assert} from 'chrome://resources/js/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {PolymerElement, TemplateInstanceBase, templatize} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './dom_switch.html.js';

export class AppManagementDomSwitchElement extends PolymerElement {
  static get is() {
    return 'app-management-dom-switch';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Should contain the route-id of one of the elements within the
       * dom-switch.
       */
      route: {
        type: String,
        observer: 'onRouteChanged_',
      },

      /**
       * The template instance.
       */
      instance_: {
        type: Object,
        value: null,
      },

      /**
       * Maps the route-id of each element within the dom-switch to the element
       * itself.
       */
      children_: {
        type: Object,
        value: () => ({}),
      },

      /**
       * The element whose route-id corresponds to the current route. This is
       * the only element within the dom-switch which is attached to the DOM.
       */
      selectedChild_: {
        type: Object,
        value: null,
      },
    };
  }

  route: string|null;
  private instance_: TemplateInstanceBase|null;
  private children_: Record<string, Element>;
  private firstRenderForTesting_: PromiseResolver<void>;
  private selectedChild_: Element|null;

  constructor() {
    super();

    this.firstRenderForTesting_ = new PromiseResolver();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    const template =
        this.shadowRoot!.querySelector('slot')!.assignedNodes({flatten: true})
            .filter(n => n.nodeType === Node.ELEMENT_NODE)[0] as
        HTMLTemplateElement;

    const TemplateClass = templatize(template, this, {
      mutableData: false,
      forwardHostProp: this._forwardHostPropV2,
    });

    // This call stamps all the child elements of the dom-switch at once
    // (calling their created Polymer lifecycle callbacks). If optimisations
    // are required in the future, it may be possible to only stamp children
    // on demand as they are rendered.
    this.instance_ = new TemplateClass();

    const children = this.instance_.root.children;
    for (const child of children) {
      this.children_[child.getAttribute('route-id')!] = child;
    }

    if (this.route) {
      // TODO(crbug.com/40642847): Add test coverage for this case.
      // If attached is called after the route has been set.
      this.onRouteChanged_(this.route);
    }
  }

  private onRouteChanged_(newRouteId: string|null): void {
    if (!this.instance_) {
      return;
    }

    if (newRouteId === null) {
      if (this.selectedChild_ === null) {
        return;
      }

      this.parentNode!.removeChild(this.selectedChild_);
      this.selectedChild_ = null;
      return;
    }

    const newSelectedChild = this.children_[newRouteId];
    assert(
        newSelectedChild,
        'The route must be equal to the route-id of a child element.');

    if (this.selectedChild_) {
      this.parentNode!.replaceChild(newSelectedChild, this.selectedChild_);
    } else {
      this.parentNode!.insertBefore(newSelectedChild, this);
    }

    this.selectedChild_ = newSelectedChild;
    this.firstRenderForTesting_.resolve();
  }

  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  private _forwardHostPropV2(prop: string, value: Object): void {
    if (this.instance_) {
      this.instance_.forwardHostProp(prop, value);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-dom-switch': AppManagementDomSwitchElement;
  }
}

customElements.define(
    AppManagementDomSwitchElement.is, AppManagementDomSwitchElement);
