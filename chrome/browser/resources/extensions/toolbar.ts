// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
import './pack_dialog.js';

import {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.js';
import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import type {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {listenOnce} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './toolbar.css.js';
import {getHtml} from './toolbar.html.js';

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

class DummyToolbarDelegate {
  setProfileInDevMode(_inDevMode: boolean) {}
  loadUnpacked() {
    return Promise.resolve(true);
  }
  updateAllExtensions(_extensions: chrome.developerPrivate.ExtensionInfo[]) {
    return Promise.resolve();
  }
}

export interface ExtensionsToolbarElement {
  $: {
    devDrawer: HTMLElement,
    devMode: CrToggleElement,
    loadUnpacked: HTMLElement,
    packExtensions: HTMLElement,
    toolbar: CrToolbarElement,
    updateNow: HTMLElement,
  };
}

const ExtensionsToolbarElementBase = I18nMixinLit(CrLitElement);

export class ExtensionsToolbarElement extends ExtensionsToolbarElementBase {
  static get is() {
    return 'extensions-toolbar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      extensions: {type: Array},
      delegate: {type: Object},

      inDevMode: {
        type: Boolean,
        reflect: true,
      },

      devModeControlledByPolicy: {type: Boolean},
      isChildAccount: {type: Boolean},

      narrow: {
        type: Boolean,
        notify: true,
      },

      canLoadUnpacked: {type: Boolean},

      expanded_: {type: Boolean},
      showPackDialog_: {type: Boolean},

      /**
       * Prevents initiating update while update is in progress.
       */
      isUpdating_: {type: Boolean},
    };
  }

  extensions: chrome.developerPrivate.ExtensionInfo[] = [];
  delegate: ToolbarDelegate = new DummyToolbarDelegate();
  inDevMode: boolean = false;
  devModeControlledByPolicy: boolean = false;
  isChildAccount: boolean = false;

  narrow: boolean = false;
  canLoadUnpacked?: boolean;

  protected expanded_: boolean = false;
  protected showPackDialog_: boolean = false;
  private isUpdating_: boolean = false;

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.setAttribute('role', 'banner');
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('inDevMode')) {
      const previous = changedProperties.get('inDevMode')!;
      this.onInDevModeChanged_(this.inDevMode, previous);
    }
  }

  focusSearchInput() {
    this.$.toolbar.getSearchField().showAndFocus();
  }

  isSearchFocused(): boolean {
    return this.$.toolbar.getSearchField().isSearchFocused();
  }

  protected shouldDisableDevMode_(): boolean {
    return this.devModeControlledByPolicy || this.isChildAccount;
  }

  protected getTooltipText_(): string {
    return this.i18n(
        this.isChildAccount ? 'controlledSettingChildRestriction' :
                              'controlledSettingPolicy');
  }

  protected getIcon_(): string {
    return this.isChildAccount ? 'cr20:kite' : 'cr20:domain';
  }

  protected onDevModeToggleChange_(e: CustomEvent<boolean>) {
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

  protected onLoadUnpackedClick_() {
    this.delegate.loadUnpacked()
        .then((success) => {
          if (success) {
            const toastManager = getToastManager();
            toastManager.duration = 3000;
            toastManager.show(this.i18n('toolbarLoadUnpackedDone'));
          }
        })
        .catch(loadError => {
          this.fire('load-error', loadError);
        });
    chrome.metricsPrivate.recordUserAction('Options_LoadUnpackedExtension');
  }

  protected onPackClick_() {
    chrome.metricsPrivate.recordUserAction('Options_PackExtension');
    this.showPackDialog_ = true;
  }

  protected onPackDialogClose_() {
    this.showPackDialog_ = false;
    this.$.packExtensions.focus();
  }

  protected onUpdateNowClick_() {
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
              this.fire('load-error', loadError);
              toastManager.hide();
              this.isUpdating_ = false;
            });
  }

  protected onNarrowChanged_(e: CustomEvent<{value: boolean}>) {
    this.narrow = e.detail.value;
  }

  protected canLoadUnpacked_() {
    return this.canLoadUnpacked === undefined || this.canLoadUnpacked;
  }
}

// Exported to be used in the autogenerated Lit template file
export type ToolbarElement = ExtensionsToolbarElement;

declare global {
  interface HTMLElementTagNameMap {
    'extensions-toolbar': ExtensionsToolbarElement;
  }
}

customElements.define(ExtensionsToolbarElement.is, ExtensionsToolbarElement);
