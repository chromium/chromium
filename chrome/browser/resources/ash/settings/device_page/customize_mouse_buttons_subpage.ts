// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-customize-mouse-buttons-subpage' displays the customized buttons
 * and allow users to configure their buttons for each mouse.
 */
import '../icons.html.js';
import '../settings_shared.css.js';
import './input_device_settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './customize_mouse_buttons_subpage.html.js';
import {Mouse} from './input_device_settings_types.js';

const SettingsCustomizeMouseButtonsSubpageElementBase =
    RouteObserverMixin(I18nMixin(PolymerElement));

export class SettingsCustomizeMouseButtonsSubpageElement extends
    SettingsCustomizeMouseButtonsSubpageElementBase {
  static get is() {
    return 'settings-customize-mouse-buttons-subpage' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      mouse: {
        type: Object,
      },

      mice: {
        type: Array,
      },
    };
  }

  static get observers(): string[] {
    return [
      'onMouseListUpdated(mice.*)',
    ];
  }

  mouse: Mouse;
  mice: Mouse[];

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.CUSTOMIZE_MOUSE_BUTTONS) {
      return;
    }
    if (this.hasMice() &&
        (!this.mouse || this.mouse.id !== this.getMouseIdFromUrl())) {
      this.initializeMouse();
    }
  }

  /**
   * Get the mouse to display according to the mouseId in the url query,
   * initializing the page and pref with the mouse data.
   */
  private initializeMouse(): void {
    const mouseId = this.getMouseIdFromUrl();
    const searchedMouse =
        this.mice.find((mouse: Mouse) => mouse.id === mouseId);
    this.mouse = castExists(searchedMouse);
  }

  private getMouseIdFromUrl(): number {
    return Number(Router.getInstance().getQueryParameters().get('mouseId'));
  }

  private hasMice(): boolean {
    return this.mice?.length > 0;
  }

  private isMouseConnected(id: number): boolean {
    return !!this.mice.find(mouse => mouse.id === id);
  }

  onMouseListUpdated(): void {
    if (Router.getInstance().currentRoute !== routes.CUSTOMIZE_MOUSE_BUTTONS) {
      return;
    }

    if (!this.hasMice() || !this.isMouseConnected(this.getMouseIdFromUrl())) {
      Router.getInstance().navigateTo(routes.DEVICE);
      return;
    }
    this.initializeMouse();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsCustomizeMouseButtonsSubpageElement.is]:
        SettingsCustomizeMouseButtonsSubpageElement;
  }
}

customElements.define(
    SettingsCustomizeMouseButtonsSubpageElement.is,
    SettingsCustomizeMouseButtonsSubpageElement);
