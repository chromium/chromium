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
import './per_device_app_installed_row.js';
import './per_device_install_row.js';
import './per_device_subsection_header.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/ash/common/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {CrLinkRowElement} from 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {cast, castExists} from '../assert_extras.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {PrefsState} from '../common/types.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './graphics_tablet_subpage.html.js';
import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {CompanionAppInfo, CompanionAppState, GraphicsTablet, GraphicsTabletButtonConfig, InputDeviceSettingsProviderInterface} from './input_device_settings_types.js';
import {getDeviceStateChangesToAnnounce} from './input_device_settings_utils.js';

const SettingsGraphicsTabletSubpageElementBase =
    RouteObserverMixin(I18nMixin(PolymerElement));

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
      prefs: {
        type: Object,
        notify: true,
      },

      graphicsTablets: {
        type: Array,
        observer: 'onGraphicsTabletListUpdated',
      },

      /**
         Used to track if the pen customize button row is clicked.
       */
      currentPenChanged: {
        type: Boolean,
      },

      /**
         Used to track if the tablet customize button row is clicked.
       */
      currentTabletChanged: {
        type: Boolean,
      },

      /**
         Used to track which graphics tablet navigates to the customization
         subpage.
       */
      deviceId: {
        type: Number,
      },
    };
  }

  prefs: PrefsState;
  graphicsTablets: GraphicsTablet[];
  private currentPenChanged: boolean;
  private currentTabletChanged: boolean;
  private deviceId: number;
  private inputDeviceSettingsProvider: InputDeviceSettingsProviderInterface =
      getInputDeviceSettingsProvider();

  override currentRouteChanged(route: Route): void {
    // Avoid override deviceId, currentPenChanged, currentTabletChanged when on
    // the customization subpage.
    if (route === routes.CUSTOMIZE_PEN_BUTTONS ||
        route === routes.CUSTOMIZE_TABLET_BUTTONS) {
      return;
    }

    // Does not apply to this page.
    if (route !== routes.GRAPHICS_TABLET) {
      // Reset all values when on other pages.
      this.deviceId = -1;
      this.currentPenChanged = false;
      this.currentTabletChanged = false;
      return;
    }

    // Don't attempt to focus any item unless the last navigation was a
    // 'pop' (backwards) navigation.
    if (!Router.getInstance().lastRouteChangeWasPopstate()) {
      return;
    } else {
      // Loop through the graphics tablets and refocus on the
      // cr-link-row with the same device ID when navigating back to the
      // graphics tablet subpage.
      const graphicsTablets =
          this.shadowRoot!.querySelectorAll<HTMLDivElement>('.device');
      for (const graphicsTablet of graphicsTablets) {
        if (Number(graphicsTablet.getAttribute('data-evdev-id')!) ===
            this.deviceId) {
          if (this.currentPenChanged) {
            graphicsTablet
                .querySelector<CrLinkRowElement>(
                    '#customizePenButtons')!.focus();
          } else if (this.currentTabletChanged) {
            graphicsTablet
                .querySelector<CrLinkRowElement>(
                    '#customizeTabletButtons')!.focus();
          }
        }
      }
    }

    this.deviceId = -1;
    this.currentPenChanged = false;
    this.currentTabletChanged = false;
  }

  private onGraphicsTabletListUpdated(
      newGraphicsTabletList: GraphicsTablet[],
      oldGraphicsTabletList: GraphicsTablet[]|undefined): void {
    if (!oldGraphicsTabletList) {
      return;
    }
    const {msgId, deviceNames} = getDeviceStateChangesToAnnounce(
        newGraphicsTabletList, oldGraphicsTabletList);
    for (const deviceName of deviceNames) {
      getAnnouncerInstance().announce(this.i18n(msgId, deviceName));
    }
  }

  private onCustomizeTabletButtonsClick(e: PointerEvent): void {
    Router.getInstance().navigateTo(
        routes.CUSTOMIZE_TABLET_BUTTONS,
        /* dynamicParams= */ this.getSelectedGraphicsTabletUrl(e),
        /* removeSearch= */ true);
    this.currentTabletChanged = true;
  }

  private showInstallAppRow(appInfo: CompanionAppInfo|null): boolean {
    return appInfo?.state === CompanionAppState.kAvailable;
  }

  private onCustomizePenButtonsClick(e: PointerEvent): void {
    Router.getInstance().navigateTo(
        routes.CUSTOMIZE_PEN_BUTTONS,
        /* dynamicParams= */ this.getSelectedGraphicsTabletUrl(e),
        /* removeSearch= */ true);
    this.currentPenChanged = true;
  }

  private showCustomizeTabletButtonsRow(graphicsTablet: GraphicsTablet):
      boolean {
    // Hide the graphics tablet button page when there are no buttons
    // due to having metadata about the device.
    return (graphicsTablet.graphicsTabletButtonConfig ===
            GraphicsTabletButtonConfig.kNoConfig) ||
        (graphicsTablet.settings.tabletButtonRemappings.length !== 0);
  }

  private getSelectedGraphicsTabletUrl(e: PointerEvent): URLSearchParams {
    const customizeTabletButton = cast(e.target, CrLinkRowElement);
    const closestTablet: HTMLDivElement|null =
        castExists(customizeTabletButton.closest('.device'));
    const graphicsTabletId = closestTablet.getAttribute('data-evdev-id')!;
    this.deviceId = Number(graphicsTabletId);
    return new URLSearchParams({
      graphicsTabletId: encodeURIComponent(graphicsTabletId),
    });
  }

  private isCompanionAppInstalled(appInfo: CompanionAppInfo|null): boolean {
    return appInfo?.state === CompanionAppState.kInstalled;
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
