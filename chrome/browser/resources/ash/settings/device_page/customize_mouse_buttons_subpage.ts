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
import '../controls/settings_toggle_button.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/ash/common/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './customize_mouse_buttons_subpage.html.js';
import {getInputDeviceSettingsProvider} from './input_device_mojo_interface_provider.js';
import {ActionChoice, InputDeviceSettingsProviderInterface, MetaKey, Mouse, MouseButtonConfig, MousePolicies} from './input_device_settings_types.js';
import {getPrefPolicyFields} from './input_device_settings_utils.js';

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
      selectedMouse: {
        type: Object,
      },

      mouseList: {
        type: Array,
      },

      buttonActionList_: {
        type: Array,
      },

      mousePolicies: {
        type: Object,
      },

      primaryRightPref_: {
        type: Object,
        value() {
          return {
            key: 'fakePrimaryRightPref',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          };
        },
      },

      /**
       * Use metaKey to decide which meta key icon to display.
       */
      metaKey_: Object,
    };
  }

  static get observers(): string[] {
    return [
      'onMouseListUpdated(mouseList.*)',
      'onPoliciesChanged(mousePolicies)',
      'onSettingsChanged(primaryRightPref_.value)',
    ];
  }

  selectedMouse: Mouse;
  mouseList: Mouse[];
  mousePolicies: MousePolicies;
  private buttonActionList_: ActionChoice[];
  private inputDeviceSettingsProvider_: InputDeviceSettingsProviderInterface =
      getInputDeviceSettingsProvider();
  private previousRoute_: Route|null = null;
  private primaryRightPref_: chrome.settingsPrivate.PrefObject;
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
    if (route !== routes.CUSTOMIZE_MOUSE_BUTTONS) {
      if (this.previousRoute_ === routes.CUSTOMIZE_MOUSE_BUTTONS) {
        this.inputDeviceSettingsProvider_.stopObserving();
      }
      this.previousRoute_ = route;
      return;
    }
    this.previousRoute_ = route;

    if (!this.hasMice()) {
      return;
    }

    if (!this.selectedMouse ||
        this.selectedMouse.id !== this.getMouseIdFromUrl()) {
      await this.initializeMouse();
    }
    this.inputDeviceSettingsProvider_.startObserving(this.selectedMouse.id);
    getAnnouncerInstance().announce(
        this.getcustomizeMouseButtonsNudgeHeader_() + ' ' +
        this.getDescription_());
  }

  /**
   * Get the mouse to display according to the mouseId in the url query,
   * initializing the page and pref with the mouse data.
   */
  private async initializeMouse(): Promise<void> {
    this.isInitialized_ = false;

    const mouseId = this.getMouseIdFromUrl();
    const searchedMouse =
        this.mouseList.find((mouse: Mouse) => mouse.id === mouseId);
    this.selectedMouse = castExists(searchedMouse);
    this.set('primaryRightPref_.value', this.selectedMouse.settings.swapRight);
    this.buttonActionList_ = (await this.inputDeviceSettingsProvider_
                                  .getActionsForMouseButtonCustomization())
                                 ?.options;
    this.isInitialized_ = true;
  }

  private onPoliciesChanged(): void {
    this.primaryRightPref_ = {
      ...this.primaryRightPref_,
      ...getPrefPolicyFields(this.mousePolicies.swapRightPolicy),
    };
  }

  private getMouseIdFromUrl(): number {
    return Number(Router.getInstance().getQueryParameters().get('mouseId'));
  }

  private hasMice(): boolean {
    return this.mouseList?.length > 0;
  }

  private isMouseConnected(id: number): boolean {
    return !!this.mouseList.find(mouse => mouse.id === id);
  }

  async onMouseListUpdated(): Promise<void> {
    if (Router.getInstance().currentRoute !== routes.CUSTOMIZE_MOUSE_BUTTONS) {
      return;
    }

    if (!this.hasMice()) {
      Router.getInstance().navigateTo(routes.DEVICE);
      return;
    }

    if (!this.isMouseConnected(this.getMouseIdFromUrl())) {
      Router.getInstance().navigateTo(routes.PER_DEVICE_MOUSE);
      return;
    }
    await this.initializeMouse();
    this.inputDeviceSettingsProvider_.startObserving(this.selectedMouse.id);
  }

  onSettingsChanged(): void {
    if (!this.isInitialized_) {
      return;
    }

    this.selectedMouse!.settings!.swapRight = this.primaryRightPref_.value;
    this.inputDeviceSettingsProvider_.setMouseSettings(
        this.selectedMouse!.id, this.selectedMouse!.settings);
  }

  private getDescription_(): string {
    if (!this.selectedMouse?.name) {
      return '';
    }
    return this.i18n(
        'customizeButtonSubpageDescription', this.selectedMouse!.name);
  }

  private getcustomizeMouseButtonsNudgeHeader_(): string {
    if (this.selectedMouse?.mouseButtonConfig !== MouseButtonConfig.kNoConfig) {
      return this.i18n('customizeMouseButtonsNudgeHeaderWithMetadata');
    } else {
      return this.i18n('customizeMouseButtonsNudgeHeaderWithoutMetadata');
    }
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
