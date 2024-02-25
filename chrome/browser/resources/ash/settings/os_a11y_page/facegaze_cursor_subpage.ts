// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../settings_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../controls/settings_slider.js';
import '../controls/settings_toggle_button.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Route, routes} from '../router.js';

import {getTemplate} from './facegaze_cursor_subpage.html.js';

const SettingsFaceGazeCursorSubpageElementBase = DeepLinkingMixin(
    RouteObserverMixin(WebUiListenerMixin(I18nMixin(PolymerElement))));

export interface SettingsFaceGazeCursorSubpageElement {
  $: {};
}

export class SettingsFaceGazeCursorSubpageElement extends
    SettingsFaceGazeCursorSubpageElementBase {
  static get is() {
    return 'settings-facegaze-cursor-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },
    };
  }

  prefs: {[key: string]: any};

  constructor() {
    super();
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.MANAGE_FACEGAZE_CURSOR_SETTINGS) {
      return;
    }

    this.attemptDeepLink();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsFaceGazeCursorSubpageElement.is]:
        SettingsFaceGazeCursorSubpageElement;
  }
}

customElements.define(
    SettingsFaceGazeCursorSubpageElement.is,
    SettingsFaceGazeCursorSubpageElement);
