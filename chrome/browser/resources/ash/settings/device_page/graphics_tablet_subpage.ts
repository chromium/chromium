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
import './input_device_settings_shared.css.js';

import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {cast, castExists} from '../assert_extras.js';
import {RouteObserverMixin, RouteObserverMixinInterface} from '../route_observer_mixin.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './graphics_tablet_subpage.html.js';
import {GraphicsTablet} from './input_device_settings_types.js';

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

  static get properties(): PolymerElementProperties {
    return {
      graphicsTablets: {
        type: Array,
      },
    };
  }

  public graphicsTablets: GraphicsTablet[];

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.GRAPHICS_TABLET) {
      return;
    }
  }

  private onCustomizeTabletButtonsClick(e: PointerEvent): void {
    const customizeTabletButton = cast(e.target, CrLinkRowElement);
    const closestTablet: HTMLDivElement|null =
        castExists(customizeTabletButton.closest('.device'));
    const url = new URLSearchParams(`graphicsTabletId=${
        encodeURIComponent(closestTablet.getAttribute('data-evdev-id')!)}`);

    Router.getInstance().navigateTo(
        routes.CUSTOMIZE_TABLET_BUTTONS,
        /* dynamicParams= */ url, /* removeSearch= */ true);
  }

  private onCustomizePenButtonsClick(): void {
    // TODO(yyhyyh@): Implement the function to redirect to the customize
    // graphics pen subpage with the clicked graphicsTabletId.
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
