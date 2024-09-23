// Copyright 2015 The Chromium Authors
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

import '//resources/cr_elements/cr_page_selector/cr_page_selector.js';

import type {CrPageSelectorElement} from '//resources/cr_elements/cr_page_selector/cr_page_selector.js';
import {assert} from '//resources/js/assert.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {DomIf, FlattenedNodesObserver, microTask, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {FocusConfig} from '../focus_config.js';
import type {Route} from '../router.js';
import {RouteObserverMixin, Router} from '../router.js';

import {getTemplate} from './settings_animated_pages.html.js';
import type {SettingsSubpageElement} from './settings_subpage.js';

interface SettingsAnimatedPagesElement {
  $: {
    animatedPages: CrPageSelectorElement,
  };
}

const SettingsAnimatedPagesElementBase = RouteObserverMixin(PolymerElement);

class SettingsAnimatedPagesElement extends SettingsAnimatedPagesElementBase {
  static get is() {
    return 'settings-animated-pages';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Routes with this section activate this element. For instance, if this
       * property is 'search', and currentRoute.section is also set to 'search',
       * this element will display the subpage in currentRoute.subpage.
       *
       * The section name must match the name specified in route.js.
       */
      section: String,

      /**
       * A Map specifying which element should be focused when exiting a
       * subpage. The key of the map holds a Route path, and the value holds
       * either a query selector that identifies the desired element, an element
       * or a function to be run when a neon-animation-finish event is handled.
       */
      focusConfig: Object,
    };
  }

  section: string;
  focusConfig: FocusConfig|null = null;
  private previousRoute_: Route|null;
  private lightDomReady_: boolean = false;
  private queuedRouteChange_: {oldRoute?: Route, newRoute: Route}|null = null;

  private lightDomObserver_: FlattenedNodesObserver|null;

  constructor() {
    super();

    // Observe the light DOM so we know when it's ready.
    this.lightDomObserver_ =
        new FlattenedNodesObserver(this, this.lightDomChanged_.bind(this));

    /**
     * The last "previous" route reported by the router.
     */
    this.previousRoute_ = null;
  }

  private async onIronSelect_(e: Event) {
    // Ignore bubbling 'iron-select' events not originating from
    // |animatedPages| itself.
    if (e.target !== this.$.animatedPages) {
      return;
    }

    await this.$.animatedPages.updateComplete;

    // Call focusBackButton() on the selected subpage, only if:
    //  1) Not a direct navigation (such that the search box stays focused), and
    //  2) Not a "back" navigation, in which case the anchor element should be
    //     focused (further below in this function).
    if (this.previousRoute_ &&
        !Router.getInstance().lastRouteChangeWasPopstate()) {
      const subpage = this.querySelector<SettingsSubpageElement>(
          'settings-subpage.selected');
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
            const element = this.querySelector(pathConfig);
            assert(element);
            pathConfig = element;
          }
          focusWithoutInk(pathConfig as HTMLElement);
        };
      }
      handler();
    }
  }

  /**
   * Called initially once the effective children are ready.
   */
  private lightDomChanged_() {
    if (this.lightDomReady_) {
      return;
    }

    this.lightDomReady_ = true;
    this.lightDomObserver_!.disconnect();
    this.lightDomObserver_ = null;
    this.runQueuedRouteChange_();
  }

  /**
   * Calls currentRouteChanged with the deferred route change info.
   */
  private runQueuedRouteChange_() {
    if (!this.queuedRouteChange_) {
      return;
    }

    microTask.run(() => {
      this.currentRouteChanged(
          this.queuedRouteChange_!.newRoute, this.queuedRouteChange_!.oldRoute);
    });
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    this.previousRoute_ = oldRoute || null;

    if (newRoute.section === this.section && newRoute.isSubpage()) {
      this.switchToSubpage_(newRoute, oldRoute);
    } else {
      this.$.animatedPages.selected = 'default';
    }
  }

  /**
   * Selects the subpage specified by |newRoute|.
   */
  private async switchToSubpage_(newRoute: Route, oldRoute: Route|undefined) {
    // Don't manipulate the light DOM until it's ready.
    if (!this.lightDomReady_) {
      this.queuedRouteChange_ = this.queuedRouteChange_ || {oldRoute, newRoute};
      this.queuedRouteChange_.newRoute = newRoute;
      return;
    }

    this.ensureSubpageInstance_();
    await this.$.animatedPages.updateComplete;
    this.$.animatedPages.selected = newRoute.path;
  }

  /**
   * Ensures that the template enclosing the subpage is stamped.
   */
  private ensureSubpageInstance_() {
    const routePath = Router.getInstance().getCurrentRoute().path;
    const domIf =
        this.querySelector<DomIf>(`dom-if[route-path='${routePath}']`);

    // Nothing to do if the subpage isn't wrapped in a <dom-if> or the template
    // is already stamped.
    if (!domIf || domIf.if) {
      return;
    }

    // Set the subpage's id for use by neon-animated-pages.
    const content = DomIf._contentForTemplate(
        domIf.firstElementChild as HTMLTemplateElement);
    const subpage = content!.querySelector('settings-subpage')!;
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
    if (domIf.hasAttribute('no-search') || (domIf as any)['noSearch']) {
      subpage.setAttribute('no-search', '');
    }

    // Render synchronously so neon-animated-pages can select the subpage.
    domIf.if = true;
    domIf.render();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-animated-pages': SettingsAnimatedPagesElement;
  }
}

customElements.define(
    SettingsAnimatedPagesElement.is, SettingsAnimatedPagesElement);
