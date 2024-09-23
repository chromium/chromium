// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'customize-pen-buttons-subpage' displays the customized pen buttons on
 * the graphics tablets, and allow users to configure the pen buttons for
 * each graphics tablet.
 */
import '../icons.html.js';
import '../settings_shared.css.js';
import './input_device_settings_shared.css.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/ash/common/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './customize_pen_buttons_subpage.html.js';
import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {ActionChoice, GraphicsTablet, GraphicsTabletButtonConfig, InputDeviceSettingsProviderInterface, MetaKey} from './input_device_settings_types.js';

const SettingsCustomizePenButtonsSubpageElementBase =
    RouteObserverMixin(I18nMixin(PolymerElement));

export class SettingsCustomizePenButtonsSubpageElement extends
    SettingsCustomizePenButtonsSubpageElementBase {
  static get is() {
    return 'settings-customize-pen-buttons-subpage' as const;
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

      /**
       * Use metaKey to decide which meta key icon to display.
       */
      metaKey_: Object,
    };
  }

  static get observers(): string[] {
    return [
      'onGraphicsTabletListUpdated(graphicsTablets.*)',
    ];
  }

  selectedTablet: GraphicsTablet;
  graphicsTablets: GraphicsTablet[];
  private buttonActionList_: ActionChoice[];
  private inputDeviceSettingsProvider_: InputDeviceSettingsProviderInterface =
      getInputDeviceSettingsProvider();
  private previousRoute_: Route|null = null;
  private isInitialized_: boolean = false;
  private metaKey_: MetaKey = MetaKey.kSearch;

  override async connectedCallback(): Promise<void> {
    super.connectedCallback();

    this.addEventListener('button-remapping-changed', this.onSettingsChanged);
    this.metaKey_ =
        (await this.inputDeviceSettingsProvider_.getMetaKeyToDisplay())
            ?.metaKey;
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    this.removeEventListener(
        'button-remapping-changed', this.onSettingsChanged);
  }

  override async currentRouteChanged(route: Route): Promise<void> {
    // Does not apply to this page.
    if (route !== routes.CUSTOMIZE_PEN_BUTTONS) {
      if (this.previousRoute_ === routes.CUSTOMIZE_PEN_BUTTONS) {
        this.inputDeviceSettingsProvider_.stopObserving();
      }
      this.previousRoute_ = route;
      return;
    }
    this.previousRoute_ = route;

    if (!this.hasGraphicsTablets()) {
      return;
    }

    if (!this.selectedTablet ||
        this.selectedTablet.id !== this.getGraphicsTabletIdFromUrl()) {
      await this.initializePen();
    }
    this.inputDeviceSettingsProvider_.startObserving(this.selectedTablet.id);
    getAnnouncerInstance().announce(
        this.getcustomizePenButtonsNudgeHeader_() + ' ' +
        this.getDescription_());
  }

  /**
   * Get the pen to display according to the graphicsTabletId in the url
   * query, initializing the page and pref with the graphics tablet data.
   */
  private async initializePen(): Promise<void> {
    this.isInitialized_ = false;

    const tabletId = this.getGraphicsTabletIdFromUrl();
    const searchedGraphicsTablet = this.graphicsTablets.find(
        (graphicsTablet: GraphicsTablet) => graphicsTablet.id === tabletId);
    this.selectedTablet = castExists(searchedGraphicsTablet);
    this.buttonActionList_ =
        (await this.inputDeviceSettingsProvider_
             .getActionsForGraphicsTabletButtonCustomization())
            ?.options;
    this.isInitialized_ = true;
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
    if (Router.getInstance().currentRoute !== routes.CUSTOMIZE_PEN_BUTTONS) {
      return;
    }

    if (!this.hasGraphicsTablets()) {
      Router.getInstance().navigateTo(routes.DEVICE);
      return;
    }

    if (!this.isTabletConnected(this.getGraphicsTabletIdFromUrl())) {
      Router.getInstance().navigateTo(routes.GRAPHICS_TABLET);
      return;
    }
    await this.initializePen();
    this.inputDeviceSettingsProvider_.startObserving(this.selectedTablet.id);
  }

  onSettingsChanged(): void {
    if (!this.isInitialized_) {
      return;
    }

    this.inputDeviceSettingsProvider_.setGraphicsTabletSettings(
        this.selectedTablet!.id, this.selectedTablet!.settings);
  }

  private getDescription_(): string {
    if (!this.selectedTablet?.name) {
      return '';
    }
    return this.i18n(
        'customizeTabletButtonSubpageDescription', this.selectedTablet!.name);
  }

  private getcustomizePenButtonsNudgeHeader_(): string {
    if (this.selectedTablet?.graphicsTabletButtonConfig !==
        GraphicsTabletButtonConfig.kNoConfig) {
      return this.i18n('customizePenButtonsNudgeHeaderWithMetadata');
    } else {
      return this.i18n('customizePenButtonsNudgeHeaderWithoutMetadata');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsCustomizePenButtonsSubpageElement.is]:
        SettingsCustomizePenButtonsSubpageElement;
  }
}

customElements.define(
    SettingsCustomizePenButtonsSubpageElement.is,
    SettingsCustomizePenButtonsSubpageElement);
