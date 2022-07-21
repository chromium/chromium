// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
import '../../icons.html.js';
import '../../settings_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route} from '../../router.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

/**
 * @fileoverview
 * 'audio-settings' allow users to configure their audio settings in system
 * settings.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const SettingsAudioElementBase =
    mixinBehaviors([RouteObserverBehavior, I18nBehavior], PolymerElement);

/** @polymer */
class SettingsAudioElement extends SettingsAudioElementBase {
  static get is() {
    return 'settings-audio';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {boolean} */
      showAudioInfo: {
        type: Boolean,
        value: false,
      },

      // TODO(owenzhang): Add and connect slider values to CrosAudioConfig
      // Mojo.
    };
  }

  constructor() {
    super();
  }

  /** @override */
  ready() {
    super.ready();
  }

  /**
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    // TODO(owenzhang): Add DeepLinkingBehavior and attempt deep link.
    if (route !== routes.AUDIO) {
      return;
    }
  }
}

customElements.define(SettingsAudioElement.is, SettingsAudioElement);
