// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-animated-pages' is a container for a page and animated subpages.
 * It provides a set of common behaviors and animations.
 *
 * Example:
 *
 *    <os-settings-animated-pages section="[[Section.kNetwork]]">
 *      <!-- Insert your section controls here -->
 *    </os-settings-animated-pages>
 */

import '//resources/polymer/v3_0/iron-pages/iron-pages.js';

import {assert} from '//resources/js/assert_ts.js';
import {IronPagesElement} from '//resources/polymer/v3_0/iron-pages/iron-pages.js';
import {DomIf, FlattenedNodesObserver, microTask, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getSettingIdParameter} from '../common/setting_id_param_util.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, Router} from '../router.js';

import {getTemplate} from './os_settings_animated_pages.html.js';
import {OsSettingsSubpageElement} from './os_settings_subpage.js';

interface OsSettingsAnimatedPagesElement {
  $: {
    animatedPages: IronPagesElement,
  };
}

const OsSettingsAnimatedPagesElementBase = RouteObserverMixin(PolymerElement);

class OsSettingsAnimatedPagesElement extends
    OsSettingsAnimatedPagesElementBase {
  static get is() {
    return 'os-settings-animated-pages' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Routes with this section activate this element. For instance, if this
       * property is Section.kNetwork and currentRoute.section is also set to
       * Section.kNetwork, this element will display the corresponding page or
       * subpage.
       *
       * Must match one of the Section enum entries from routes.mojom.
       */
      section: {
        type: Number,
        reflectToAttribute: true,
      },
    };
  }

  section: Section;
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

  override ready(): void {
    super.ready();

    assert(this.section in Section, `Invalid section: ${this.section}.`);
  }

  private onIronSelect_(e: Event): void {
    // Ignore bubbling 'iron-select' events not originating from
    // |animatedPages| itself.
    if (e.target !== this.$.animatedPages) {
      return;
    }

    // If the setting ID parameter is present, don't focus anything since
    // a setting element will be deep linked and focused.
    if (getSettingIdParameter()) {
      return;
    }

    // Call focusBackButton() on the selected subpage, only if:
    //  1) Not a direct navigation (such that the search box stays focused), and
    //  2) Not a "back" navigation, in which case the anchor element should be
    //     focused (further below in this function).
    if (this.previousRoute_ &&
        !Router.getInstance().lastRouteChangeWasPopstate()) {
      const subpage = this.querySelector<OsSettingsSubpageElement>(
          'os-settings-subpage.iron-selected');
      if (subpage) {
        subpage.focusBackButton();
        return;
      }
    }
  }

  /**
   * Called initially once the effective children are ready.
   */
  private lightDomChanged_(): void {
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
  private runQueuedRouteChange_(): void {
    if (!this.queuedRouteChange_) {
      return;
    }

    microTask.run(() => {
      this.currentRouteChanged(
          this.queuedRouteChange_!.newRoute, this.queuedRouteChange_!.oldRoute);
    });
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
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
  private switchToSubpage_(newRoute: Route, oldRoute: Route|undefined): void {
    // Don't manipulate the light DOM until it's ready.
    if (!this.lightDomReady_) {
      this.queuedRouteChange_ = this.queuedRouteChange_ || {oldRoute, newRoute};
      this.queuedRouteChange_.newRoute = newRoute;
      return;
    }

    this.ensureSubpageInstance_();
    this.$.animatedPages.selected = newRoute.path;
  }

  /**
   * Ensures that the template enclosing the subpage is stamped.
   */
  private ensureSubpageInstance_(): void {
    const routePath = Router.getInstance().currentRoute.path;
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
    const subpage = content!.querySelector('os-settings-subpage')!;
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
    [OsSettingsAnimatedPagesElement.is]: OsSettingsAnimatedPagesElement;
  }
}

customElements.define(
    OsSettingsAnimatedPagesElement.is, OsSettingsAnimatedPagesElement);
