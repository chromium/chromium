// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import './pack_dialog.js';

import {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.js';
import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {listenOnce} from 'chrome://resources/js/util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './toolbar.html.js';

export interface ToolbarDelegate {
  /**
   * Toggles whether or not the profile is in developer mode.
   */
  setProfileInDevMode(inDevMode: boolean): void;

  /** Opens the dialog to load unpacked extensions. */
  loadUnpacked(): Promise<boolean>;

  /** Updates all extensions. */
  updateAllExtensions(extensions: chrome.developerPrivate.ExtensionInfo[]):
      Promise<void>;
}

export interface ExtensionsToolbarElement {
  $: {
    devDrawer: HTMLElement,
    devMode: CrToggleElement,
    loadUnpacked: HTMLElement,
    packExtensions: HTMLElement,
    toolbar: CrToolbarElement,
    updateNow: HTMLElement,

    // <if expr="chromeos_ash">
    kioskExtensions: HTMLElement,
    // </if>
  };
}

const ExtensionsToolbarElementBase = I18nMixin(PolymerElement);

export class ExtensionsToolbarElement extends ExtensionsToolbarElementBase {
  static get is() {
    return 'extensions-toolbar';
  }

  static get template() {
    return getTemplate();
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
      isChildAccount: Boolean,

      // <if expr="chromeos_ash">
      kioskEnabled: Boolean,
      // </if>

      narrow: {
        type: Boolean,
        notify: true,
      },

      canLoadUnpacked: Boolean,

      expanded_: Boolean,
      showPackDialog_: Boolean,

      /**
       * Prevents initiating update while update is in progress.
       */
      isUpdating_: {type: Boolean, value: false},
    };
  }

  extensions: chrome.developerPrivate.ExtensionInfo[];
  delegate: ToolbarDelegate;
  inDevMode: boolean;
  devModeControlledByPolicy: boolean;
  isChildAccount: boolean;

  // <if expr="chromeos_ash">
  kioskEnabled: boolean;
  // </if>

  narrow: boolean;
  canLoadUnpacked: boolean;

  private expanded_: boolean;
  private showPackDialog_: boolean;
  private isUpdating_: boolean;

  override ready() {
    super.ready();
    this.setAttribute('role', 'banner');
  }

  focusSearchInput() {
    this.$.toolbar.getSearchField().showAndFocus();
  }

  isSearchFocused(): boolean {
    return this.$.toolbar.getSearchField().isSearchFocused();
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  private shouldDisableDevMode_(): boolean {
    return this.devModeControlledByPolicy || this.isChildAccount;
  }

  private getTooltipText_(): string {
    return this.i18n(
        this.isChildAccount ? 'controlledSettingChildRestriction' :
                              'controlledSettingPolicy');
  }

  private getIcon_(): string {
    return this.isChildAccount ? 'cr20:kite' : 'cr20:domain';
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

  private onLoadUnpackedClick_() {
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

  private onPackClick_() {
    chrome.metricsPrivate.recordUserAction('Options_PackExtension');
    this.showPackDialog_ = true;
  }

  private onPackDialogClose_() {
    this.showPackDialog_ = false;
    this.$.packExtensions.focus();
  }

  // <if expr="chromeos_ash">
  private onKioskClick_() {
    this.fire_('kiosk-tap');
  }
  // </if>

  private onUpdateNowClick_() {
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

declare global {
  interface HTMLElementTagNameMap {
    'extensions-toolbar': ExtensionsToolbarElement;
  }
}

customElements.define(ExtensionsToolbarElement.is, ExtensionsToolbarElement);
