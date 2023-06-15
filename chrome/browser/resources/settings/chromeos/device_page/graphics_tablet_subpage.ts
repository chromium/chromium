// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-graphics-tablet-subpage' allow users to configure their graphics
 * tablet settings for each device in system settings.
 */

import '../icons.html.js';
import '../settings_shared.css.js';

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {RouteObserverMixin, RouteObserverMixinInterface} from '../route_observer_mixin.js';
import {Route, routes} from '../router.js';

import {getTemplate} from './graphics_tablet_subpage.html.js';

const SettingsGraphicsTabletSubpageElementBase =
    RouteObserverMixin(I18nMixin(PolymerElement)) as {
      new (): PolymerElement & I18nMixinInterface & RouteObserverMixinInterface,
    };

export class SettingsGraphicsTabletSubpageElement extends
    SettingsGraphicsTabletSubpageElementBase {
  static get is() {
    return 'settings-graphics-tablet-subpage' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.GRAPHICS_TABLET) {
      return;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsGraphicsTabletSubpageElement.is]:
        SettingsGraphicsTabletSubpageElement;
  }
}

customElements.define(
    SettingsGraphicsTabletSubpageElement.is,
    SettingsGraphicsTabletSubpageElement);
