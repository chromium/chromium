// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-animated-pages' is a container for a page and animated subpages.
 * It provides a set of common behaviors and animations.
 *
 * Example:
 *
 *    <settings-animated-pages section="privacy">
 *      <!-- Insert your section controls here -->
 *    </settings-animated-pages>
 */

import '//resources/polymer/v3_0/iron-pages/iron-pages.js';

import {assert} from '//resources/js/assert.m.js';
import {focusWithoutInk} from '//resources/js/cr/ui/focus_without_ink.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {afterNextRender, dom, DomIf, html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, RouteObserverBehavior, Router} from '../router.js';
import {getSettingIdParameter} from '../setting_id_param_util.js';

Polymer({
  is: 'settings-animated-pages',

  _template: html`{__html_template__}`,

  behaviors: [RouteObserverBehavior],

  properties: {
    /**
     * Routes with this section activate this element. For instance, if this
     * property is 'search', and currentRoute.section is also set to 'search',
     * this element will display the subpage in currentRoute.subpage.
     *
     * The section name must match the name specified in route.js.
     */
    section: String,

    /**
     * A Map specifying which element should be focused when exiting a subpage.
     * The key of the map holds a Route path, and the value holds
     * either a query selector that identifies the desired element, an element
     * or a function to be run when a neon-animation-finish event is handled.
     * @type {?Map<string, (string|Element|Function)>}
     */
    focusConfig: Object,
  },

  /**
   * The last "previous" route reported by the router.
   * @private {?Route}
   */
  previousRoute_: null,

  /** @override */
  created() {
    // Observe the light DOM so we know when it's ready.
    this.lightDomObserver_ =
        dom(this).observeNodes(this.lightDomChanged_.bind(this));
  },

  /**
   * @param {!Event} e
   * @private
   */
  onIronSelect_(e) {
    // Ignore bubbling 'iron-select' events not originating from
    // |animatedPages| itself.
    if (e.target !== this.$.animatedPages) {
      return;
    }

    // <if expr="chromeos">
    // If the setting ID parameter is present, don't focus anything since
    // a setting element will be deep linked and focused.
    if (loadTimeData.valueExists('isOSSettings') &&
        loadTimeData.getBoolean('isOSSettings') && getSettingIdParameter()) {
      return;
    }
    // </if>

    // Call focusBackButton() on the selected subpage, only if:
    //  1) Not a direct navigation (such that the search box stays focused), and
    //  2) Not a "back" navigation, in which case the anchor element should be
    //     focused (further below in this function).
    if (this.previousRoute_ &&
        !Router.getInstance().lastRouteChangeWasPopstate()) {
      const subpage = this.querySelector('settings-subpage.iron-selected');
      if (subpage) {
        subpage.focusBackButton();
        return;
      }
    }

    // Don't attempt to focus any anchor element, unless last navigation was a
    // 'pop' (backwards) navigation.
    if (!Router.getInstance().lastRouteChangeWasPopstate()) {
      return;
    }

    if (!this.focusConfig || !this.previousRoute_) {
      return;
    }

    // Ensure focus-config was correctly specified as a Polymer property.
    assert(this.focusConfig instanceof Map);


    const currentRoute = Router.getInstance().getCurrentRoute();
    const fromToKey = `${this.previousRoute_.path}_${currentRoute.path}`;

    // Look for a key that captures both previous and current route first. If
    // not found, then look for a key that only captures the previous route.
    let pathConfig = this.focusConfig.get(fromToKey) ||
        this.focusConfig.get(this.previousRoute_.path);
    if (pathConfig) {
      let handler;
      if (typeof pathConfig === 'function') {
        handler = pathConfig;
      } else {
        handler = () => {
          if (typeof pathConfig === 'string') {
            pathConfig = assert(this.querySelector(pathConfig));
          }
          focusWithoutInk(/** @type {!Element} */ (pathConfig));
        };
      }
      handler();
    }
  },

  /**
   * Called initially once the effective children are ready.
   * @private
   */
  lightDomChanged_() {
    if (this.lightDomReady_) {
      return;
    }

    this.lightDomReady_ = true;
    dom(this).unobserveNodes(this.lightDomObserver_);
    this.runQueuedRouteChange_();
  },

  /**
   * Calls currentRouteChanged with the deferred route change info.
   * @private
   */
  runQueuedRouteChange_() {
    if (!this.queuedRouteChange_) {
      return;
    }
    this.async(this.currentRouteChanged.bind(
        this, this.queuedRouteChange_.newRoute,
        this.queuedRouteChange_.oldRoute));
  },

  /** @protected */
  currentRouteChanged(newRoute, oldRoute) {
    this.previousRoute_ = oldRoute;

    if (newRoute.section === this.section && newRoute.isSubpage()) {
      this.switchToSubpage_(newRoute, oldRoute);
    } else {
      this.$.animatedPages.selected = 'default';
    }
  },

  /**
   * Selects the subpage specified by |newRoute|.
   * @param {!Route} newRoute
   * @param {!Route} oldRoute
   * @private
   */
  switchToSubpage_(newRoute, oldRoute) {
    // Don't manipulate the light DOM until it's ready.
    if (!this.lightDomReady_) {
      this.queuedRouteChange_ = this.queuedRouteChange_ || {oldRoute: oldRoute};
      this.queuedRouteChange_.newRoute = newRoute;
      return;
    }

    this.ensureSubpageInstance_();
    this.$.animatedPages.selected = newRoute.path;
  },

  /**
   * Ensures that the template enclosing the subpage is stamped.
   * @private
   */
  ensureSubpageInstance_() {
    const routePath = Router.getInstance().getCurrentRoute().path;
    const domIf = this.querySelector(`dom-if[route-path='${routePath}']`);

    // Nothing to do if the subpage isn't wrapped in a <dom-if> or the template
    // is already stamped.
    if (!domIf || domIf.if) {
      return;
    }

    // Set the subpage's id for use by neon-animated-pages.
    const content =
        /**
           @type {!{_contentForTemplate:
               function(!HTMLTemplateElement):!HTMLElement}}
         */
        (DomIf)._contentForTemplate(
            /** @type {!HTMLTemplateElement} */ (domIf.firstElementChild));
    const subpage = content.querySelector('settings-subpage');
    subpage.setAttribute('route-path', routePath);

    // Carry over the
    //  1)'no-search' attribute or
    //  2) 'noSearch' Polymer property
    // template to the stamped instance (both cases are mapped to a 'no-search'
    // attribute intentionally), such that the stamped instance will also be
    // ignored by the searching algorithm.
    //
    // In the case were no-search is dynamically calculated use the following
    // pattern:
    //
    // <template is="dom-if" route-path="/myPath"
    //     no-search="[[shouldSkipSearch_(foo, bar)">
    //   <settings-subpage
    //     no-search$="[[shouldSkipSearch_(foo, bar)">
    //     ...
    //   </settings-subpage>
    //  </template>
    //
    // Note that the dom-if should always use the property and settings-subpage
    // should always use the attribute.
    if (domIf.hasAttribute('no-search') || domIf.noSearch) {
      subpage.setAttribute('no-search', '');
    }

    // Render synchronously so neon-animated-pages can select the subpage.
    domIf.if = true;
    domIf.render();
  },
});
