// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'customize-tablet-buttons-subpage' displays the customized tablet buttons
 * and allow users to configure their tablet buttons for each graphics tablet.
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

import {getTemplate} from './customize_tablet_buttons_subpage.html.js';
import {FakeInputDeviceSettingsProvider} from './fake_input_device_settings_provider.js';
import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {ActionChoice, GraphicsTablet, InputDeviceSettingsProviderInterface} from './input_device_settings_types.js';

const SettingsCustomizeTabletButtonsSubpageElementBase =
    RouteObserverMixin(I18nMixin(PolymerElement));

export class SettingsCustomizeTabletButtonsSubpageElement extends
    SettingsCustomizeTabletButtonsSubpageElementBase {
  static get is() {
    return 'settings-customize-tablet-buttons-subpage' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      selectedTablet: {
        type: Object,
      },

      graphicsTablets: {
        type: Array,
      },
    };
  }

  static get observers(): string[] {
    return [
      'onGraphicsTabletListUpdated(graphicsTablets.*)',
    ];
  }

  selectedTablet: GraphicsTablet;
  public graphicsTablets: GraphicsTablet[];
  private buttonActionList_: ActionChoice[];
  private inputDeviceSettingsProvider_: InputDeviceSettingsProviderInterface =
      getInputDeviceSettingsProvider();
  private previousRoute_: Route|null = null;

  override connectedCallback(): void {
    super.connectedCallback();

    this.addEventListener('button-remapping-changed', this.onSettingsChanged);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    this.removeEventListener(
        'button-remapping-changed', this.onSettingsChanged);
  }

  override async currentRouteChanged(route: Route): Promise<void> {
    // Does not apply to this page.
    if (route !== routes.CUSTOMIZE_TABLET_BUTTONS) {
      if (this.previousRoute_ === routes.CUSTOMIZE_TABLET_BUTTONS) {
        this.inputDeviceSettingsProvider_.stopObserving();
      }
      this.previousRoute_ = route;
      return;
    }
    this.previousRoute_ = route;
    if (this.hasGraphicsTablets() &&
        (!this.selectedTablet ||
         this.selectedTablet.id !== this.getGraphicsTabletIdFromUrl())) {
      await this.initializeTablet();
    }
    this.inputDeviceSettingsProvider_.startObserving(this.selectedTablet.id);
  }

  /**
   * Get the tablet to display according to the graphicsTabletId in the url
   * query, initializing the page and pref with the tablet data.
   */
  private async initializeTablet(): Promise<void> {
    const tabletId = this.getGraphicsTabletIdFromUrl();
    this.buttonActionList_ =
        (await this.inputDeviceSettingsProvider_
             .getActionsForGraphicsTabletButtonCustomization())
            ?.options;
    const searchedGraphicsTablet = this.graphicsTablets.find(
        (graphicsTablet: GraphicsTablet) => graphicsTablet.id === tabletId);
    this.selectedTablet = castExists(searchedGraphicsTablet);
  }

  private getGraphicsTabletIdFromUrl(): number {
    return Number(
        Router.getInstance().getQueryParameters().get('graphicsTabletId'));
  }

  private hasGraphicsTablets(): boolean {
    return this.graphicsTablets?.length > 0;
  }

  private isTabletConnected(id: number): boolean {
    return !!this.graphicsTablets.find(tablet => tablet.id === id);
  }

  async onGraphicsTabletListUpdated(): Promise<void> {
    if (Router.getInstance().currentRoute !== routes.CUSTOMIZE_TABLET_BUTTONS) {
      return;
    }

    if (!this.hasGraphicsTablets() ||
        !this.isTabletConnected(this.getGraphicsTabletIdFromUrl())) {
      Router.getInstance().navigateTo(routes.DEVICE);
      return;
    }
    await this.initializeTablet();
    this.inputDeviceSettingsProvider_.startObserving(this.selectedTablet.id);
  }

  onSettingsChanged(): void {
    // TODO(yyhyyh@): Remove the if-condition after mojo api is done.
    if (this.inputDeviceSettingsProvider_ instanceof
        FakeInputDeviceSettingsProvider) {
      this.inputDeviceSettingsProvider_.setGraphicsTabletSettings(
          this.selectedTablet!.id, this.selectedTablet!.settings);
    }
  }

  private getDescription_(): string {
    if (!this.selectedTablet?.name) {
      return '';
    }
    return this.i18n(
        'customizeButtonSubpageDescription', this.selectedTablet!.name);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsCustomizeTabletButtonsSubpageElement.is]:
        SettingsCustomizeTabletButtonsSubpageElement;
  }
}

customElements.define(
    SettingsCustomizeTabletButtonsSubpageElement.is,
    SettingsCustomizeTabletButtonsSubpageElement);
