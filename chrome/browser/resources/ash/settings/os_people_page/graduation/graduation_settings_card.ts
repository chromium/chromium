// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'graduation-settings-card' is the card element for settings
 * related to the Content Transfer app.
 */

import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import '../../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../../common/route_observer_mixin.js';
import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import type {Route} from '../../router.js';
import {routes} from '../../router.js';

import {getTemplate} from './graduation_settings_card.html.js';
import {getGraduationHandlerProvider} from './mojo_interface_provider.js';

const GraduationSettingsCardElementBase =
    RouteObserverMixin(DeepLinkingMixin(I18nMixin(PolymerElement)));

export class GraduationSettingsCardElement extends
    GraduationSettingsCardElementBase {
  static get is() {
    return 'graduation-settings-card' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kGraduation,
        ]),
      },
    };
  }

  override currentRouteChanged(newRoute: Route): void {
    if (newRoute !== routes.OS_PEOPLE) {
      return;
    }

    this.attemptDeepLink();
  }

  private onGraduationRowClick_(): void {
    getGraduationHandlerProvider().launchGraduationApp();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [GraduationSettingsCardElement.is]: GraduationSettingsCardElement;
  }
}

customElements.define(
    GraduationSettingsCardElement.is, GraduationSettingsCardElement);
