// Copyright 2019 The Chromium Authors. All rights reserved.
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
 *     <app-management-dom-switch id="view-selector">
 *       <template>
 *         <view-one route-id="view-one" title="[[parentProperty]]"></view-one>
 *         <view-two route-id="view-two"></view-two>
 *         <view-three route-id="view-three"></view-three>
 *       </template>
 *     </app-management-dom-switch>
 *   </parent-element>
 *
 *   this.$['view-selector'].route = 'view-two';
 */

// TODO(crbug.com/992795) Merge with cr-view-manager.
Polymer({
  is: 'app-management-dom-switch',

  behaviors: [Polymer.Templatizer],

  properties: {
    /**
     * Should contain the route-id of one of the elements within the dom-switch.
     * @private {?string}
     */
    route: {
      type: String,
      observer: 'onRouteChanged_',
    },

    /**
     * The template instance.
     * @private {?Element}
     */
    instance_: {
      type: Object,
      value: null,
    },

    /**
     * Maps the route-id of each element within the dom-switch to the element
     * itself.
     * @private {Object<string, Element>}
     */
    children_: {
      type: Object,
      value: () => ({}),
    },

    /**
     * The element whose route-id corresponds to the current route. This is the
     * only element within the dom-switch which is attached to the DOM.
     * @private {?Element}
     */
    selectedChild_: {
      type: Object,
      value: null,
    },
  },

  firstRenderForTesting_: new PromiseResolver(),

  attached: function() {
    const template = this.getContentChildren()[0];
    this.templatize(template);

    // This call stamps all the child elements of the dom-switch at once
    // (calling their created Polymer lifecycle callbacks). If optimisations
    // are required in the future, it may be possible to only stamp children
    // on demand as they are rendered.
    this.instance_ = this.stamp({});

    const children = this.instance_.root.children;
    for (const child of children) {
      this.children_[child.getAttribute('route-id')] = child;
    }

    if (this.route) {
      // TODO(crbug.com/999523): Add test coverage for this case.
      // If attached is called after the route has been set.
      this.onRouteChanged_(this.route);
    }
  },

  /**
   * @param {?string} newRouteId
   */
  onRouteChanged_: function(newRouteId) {
    if (!this.instance_) {
      return;
    }

    if (newRouteId === null) {
      if (this.selectedChild_ === null) {
        return;
      }

      this.parentNode.removeChild(this.selectedChild_);
      this.selectedChild_ = null;
      return;
    }

    const newSelectedChild = this.children_[newRouteId];
    assert(
        newSelectedChild,
        'The route must be equal to the route-id of a child element.');

    if (this.selectedChild_) {
      this.parentNode.replaceChild(newSelectedChild, this.selectedChild_);
    } else {
      this.parentNode.insertBefore(newSelectedChild, this);
    }

    this.selectedChild_ = newSelectedChild;
    this.firstRenderForTesting_.resolve();
  },

  /**
   * @param {string} prop
   * @param {Object} value
   */
  _forwardHostPropV2: function(prop, value) {
    if (this.instance_) {
      this.instance_.forwardHostProp(prop, value);
    }
  },
});
