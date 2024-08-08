// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';
import '../os_settings_page/os_settings_animated_pages.js';
import './facegaze_actions_card.js';
import './facegaze_cursor_card.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {DomRepeat, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Route, routes} from '../router.js';

import {getTemplate} from './facegaze_subpage.html.js';

const SettingsFaceGazeSubpageElementBase = DeepLinkingMixin(RouteObserverMixin(
    WebUiListenerMixin(PrefsMixin(I18nMixin(PolymerElement)))));

export interface SettingsFaceGazeSubpageElement {
  $: {
    recognitionConfidenceRepeat: DomRepeat,
  };
}

export class SettingsFaceGazeSubpageElement extends
    SettingsFaceGazeSubpageElementBase {
  static get is() {
    return 'settings-facegaze-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      toggleLabel_: {
        type: String,
        computed:
            'getToggleLabel_(prefs.settings.a11y.face_gaze.enabled.value)',
      },
    };
  }

  private getToggleLabel_(): string {
    return this.getPref('settings.a11y.face_gaze.enabled').value ?
        this.i18n('deviceOn') :
        this.i18n('deviceOff');
  }

  static get observers() {
    return [];
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.MANAGE_FACEGAZE_SETTINGS) {
      return;
    }

    this.attemptDeepLink();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsFaceGazeSubpageElement.is]: SettingsFaceGazeSubpageElement;
  }
}

customElements.define(
    SettingsFaceGazeSubpageElement.is, SettingsFaceGazeSubpageElement);
