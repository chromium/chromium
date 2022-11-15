// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-select-to-speak-subpage' is the accessibility settings subpage for
 * Select-to-speak settings.
 */

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../../settings_shared.css.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin, WebUiListenerMixinInterface} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteOriginBehavior, RouteOriginBehaviorInterface} from '../route_origin_behavior.js';

import {getTemplate} from './select_to_speak_subpage.html.js';

const SettingsSelectToSpeakSubpageElementBase =
    mixinBehaviors(
        [
          DeepLinkingBehavior,
          RouteOriginBehavior,
        ],
        WebUiListenerMixin(I18nMixin(PolymerElement))) as {
      new (): PolymerElement & I18nMixinInterface &
          WebUiListenerMixinInterface & DeepLinkingBehaviorInterface &
          RouteOriginBehaviorInterface,
    };

class SettingsSelectToSpeakSubpageElement extends
    SettingsSelectToSpeakSubpageElementBase {
  static get is() {
    return 'settings-select-to-speak-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },
    };
  }

  prefs: {[key: string]: any};
  private route_: Route;

  constructor() {
    super();

    /** RouteOriginBehavior override */
    this.route_ = routes.A11Y_SELECT_TO_SPEAK;
  }

  /**
   * Note: Overrides RouteOriginBehavior implementation.
   */
  override currentRouteChanged(newRoute: Route, prevRoute?: Route) {
    super.currentRouteChanged(newRoute, prevRoute);

    // Does not apply to this page.
    if (newRoute !== routes.A11Y_SELECT_TO_SPEAK) {
      return;
    }

    this.attemptDeepLink();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-select-to-speak-subpage': SettingsSelectToSpeakSubpageElement;
  }
}

customElements.define(
    SettingsSelectToSpeakSubpageElement.is,
    SettingsSelectToSpeakSubpageElement);
