// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import './pack_dialog.js';

import {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {listenOnce} from 'chrome://resources/js/util.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export interface ToolbarDelegate {
  /**
   * Toggles whether or not the profile is in developer mode.
   */
  setProfileInDevMode(inDevMode: boolean): void;

  /** Opens the dialog to load unpacked extensions. */
  loadUnpacked(): Promise<boolean>;

  /** Updates all extensions. */
  updateAllExtensions(extensions: chrome.developerPrivate.ExtensionInfo[]):
      Promise<string>;
}

interface ExtensionsToolbarElement {
  $: {
    devDrawer: HTMLElement,
    packExtensions: HTMLElement,
  };
}

const ExtensionsToolbarElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement) as
    {new (): PolymerElement & I18nBehavior};

class ExtensionsToolbarElement extends ExtensionsToolbarElementBase {
  static get is() {
    return 'extensions-toolbar';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      extensions: Array,
      delegate: Object,

      inDevMode: {
        type: Boolean,
        value: false,
        observer: 'onInDevModeChanged_',
        reflectToAttribute: true,
      },

      devModeControlledByPolicy: Boolean,
      isSupervised: Boolean,

      // <if expr="chromeos">
      kioskEnabled: Boolean,
      // </if>

      canLoadUnpacked: Boolean,

      expanded_: Boolean,
      showPackDialog_: Boolean,

      /**
       * Prevents initiating update while update is in progress.
       */
      isUpdating_: {type: Boolean, value: false},
    };
  }

  extensions: Array<chrome.developerPrivate.ExtensionInfo>;
  delegate: ToolbarDelegate;
  inDevMode: boolean;
  devModeControlledByPolicy: boolean;
  isSupervised: boolean;

  // <if expr="chromeos">
  kioskEnabled: boolean;
  // </if>

  canLoadUnpacked: boolean;

  private expanded_: boolean;
  private showPackDialog_: boolean;
  private isUpdating_: boolean;


  ready() {
    super.ready();
    this.setAttribute('role', 'banner');
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  private shouldDisableDevMode_(): boolean {
    return this.devModeControlledByPolicy || this.isSupervised;
  }

  private getTooltipText_(): string {
    return this.i18n(
        this.isSupervised ? 'controlledSettingChildRestriction' :
                            'controlledSettingPolicy');
  }

  private getIcon_(): string {
    return this.isSupervised ? 'cr20:kite' : 'cr20:domain';
  }

  private onDevModeToggleChange_(e: CustomEvent<boolean>) {
    this.delegate.setProfileInDevMode(e.detail);
    chrome.metricsPrivate.recordUserAction(
        'Options_ToggleDeveloperMode_' + (e.detail ? 'Enabled' : 'Disabled'));
  }

  private onInDevModeChanged_(_current: boolean, previous: boolean) {
    const drawer = this.$.devDrawer;
    if (this.inDevMode) {
      if (drawer.hidden) {
        drawer.hidden = false;
        // Requesting the offsetTop will cause a reflow (to account for
        // hidden).
        drawer.offsetTop;
      }
    } else {
      if (previous === undefined) {
        drawer.hidden = true;
        return;
      }

      listenOnce(drawer, 'transitionend', () => {
        if (!this.inDevMode) {
          drawer.hidden = true;
        }
      });
    }
    this.expanded_ = !this.expanded_;
  }

  private onLoadUnpackedTap_() {
    this.delegate.loadUnpacked()
        .then((success) => {
          if (success) {
            const toastManager = getToastManager();
            toastManager.duration = 3000;
            toastManager.show(this.i18n('toolbarLoadUnpackedDone'));
          }
        })
        .catch(loadError => {
          this.fire_('load-error', loadError);
        });
    chrome.metricsPrivate.recordUserAction('Options_LoadUnpackedExtension');
  }

  private onPackTap_() {
    chrome.metricsPrivate.recordUserAction('Options_PackExtension');
    this.showPackDialog_ = true;
  }

  private onPackDialogClose_() {
    this.showPackDialog_ = false;
    this.$.packExtensions.focus();
  }

  // <if expr="chromeos">
  private onKioskTap_() {
    this.fire_('kiosk-tap');
  }
  // </if>

  private onUpdateNowTap_() {
    // If already updating, do not initiate another update.
    if (this.isUpdating_) {
      return;
    }

    this.isUpdating_ = true;

    const toastManager = getToastManager();
    // Keep the toast open indefinitely.
    toastManager.duration = 0;
    toastManager.show(this.i18n('toolbarUpdatingToast'));
    this.delegate.updateAllExtensions(this.extensions)
        .then(
            () => {
              toastManager.hide();
              toastManager.duration = 3000;
              toastManager.show(this.i18n('toolbarUpdateDone'));
              this.isUpdating_ = false;
            },
            loadError => {
              this.fire_('load-error', loadError);
              toastManager.hide();
              this.isUpdating_ = false;
            });
  }
}

customElements.define(ExtensionsToolbarElement.is, ExtensionsToolbarElement);
