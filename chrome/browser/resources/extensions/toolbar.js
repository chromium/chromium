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
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @interface */
export class ToolbarDelegate {
  /**
   * Toggles whether or not the profile is in developer mode.
   * @param {boolean} inDevMode
   */
  setProfileInDevMode(inDevMode) {}

  /**
   * Opens the dialog to load unpacked extensions.
   * @return {!Promise}
   */
  loadUnpacked() {}

  /**
   * Updates all extensions.
   * @param {!Array<!chrome.developerPrivate.ExtensionInfo>} extensions
   * @return {!Promise}
   */
  updateAllExtensions(extensions) {}
}

Polymer({
  is: 'extensions-toolbar',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {!Array<!chrome.developerPrivate.ExtensionInfo>} */
    extensions: Array,

    /** @type {ToolbarDelegate} */
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

    /** @private */
    expanded_: Boolean,

    /** @private */
    showPackDialog_: Boolean,

    /**
     * Prevents initiating update while update is in progress.
     * @private
     */
    isUpdating_: {type: Boolean, value: false}
  },

  behaviors: [I18nBehavior],

  hostAttributes: {
    role: 'banner',
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldDisableDevMode_() {
    return this.devModeControlledByPolicy || this.isSupervised;
  },

  /**
   * @return {string}
   * @private
   */
  getTooltipText_() {
    return this.i18n(
        this.isSupervised ? 'controlledSettingChildRestriction' :
                            'controlledSettingPolicy');
  },

  /**
   * @return {string}
   * @private
   */
  getIcon_() {
    return this.isSupervised ? 'cr20:kite' : 'cr20:domain';
  },

  /**
   * @param {!CustomEvent<boolean>} e
   * @private
   */
  onDevModeToggleChange_(e) {
    this.delegate.setProfileInDevMode(e.detail);
    chrome.metricsPrivate.recordUserAction(
        'Options_ToggleDeveloperMode_' + (e.detail ? 'Enabled' : 'Disabled'));
  },

  /**
   * @param {boolean} current
   * @param {boolean} previous
   * @private
   */
  onInDevModeChanged_(current, previous) {
    const drawer = this.$.devDrawer;
    if (this.inDevMode) {
      if (drawer.hidden) {
        drawer.hidden = false;
        // Requesting the offsetTop will cause a reflow (to account for
        // hidden).
        /** @suppress {suspiciousCode} */ drawer.offsetTop;
      }
    } else {
      if (previous === undefined) {
        drawer.hidden = true;
        return;
      }

      listenOnce(drawer, 'transitionend', e => {
        if (!this.inDevMode) {
          drawer.hidden = true;
        }
      });
    }
    this.expanded_ = !this.expanded_;
  },

  /** @private */
  onLoadUnpackedTap_() {
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
  },

  /** @private */
  onPackTap_() {
    chrome.metricsPrivate.recordUserAction('Options_PackExtension');
    this.showPackDialog_ = true;
  },

  /** @private */
  onPackDialogClose_() {
    this.showPackDialog_ = false;
    this.$.packExtensions.focus();
  },

  // <if expr="chromeos">
  /** @private */
  onKioskTap_() {
    this.fire('kiosk-tap');
  },
  // </if>

  /** @private */
  onUpdateNowTap_() {
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
  },
});
