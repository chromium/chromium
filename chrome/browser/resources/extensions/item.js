// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link_css.m.js';
import './icons.js';
import './shared_style.js';
import './shared_vars.js';
import './strings.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';

import {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.m.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ItemBehavior} from './item_behavior.js';
import {computeInspectableViewLabel, EnableControl, getEnableControl, getItemSource, getItemSourceString, isEnabled, SourceType, userCanChangeEnablement} from './item_util.js';
import {navigation, Page} from './navigation_helper.js';

/** @interface */
export class ItemDelegate {
  /** @param {string} id */
  deleteItem(id) {}

  /**
   * @param {string} id
   * @param {boolean} isEnabled
   */
  setItemEnabled(id, isEnabled) {}

  /**
   * @param {string} id
   * @param {boolean} isAllowedIncognito
   */
  setItemAllowedIncognito(id, isAllowedIncognito) {}

  /**
   * @param {string} id
   * @param {boolean} isAllowedOnFileUrls
   */
  setItemAllowedOnFileUrls(id, isAllowedOnFileUrls) {}

  /**
   * @param {string} id
   * @param {!chrome.developerPrivate.HostAccess} hostAccess
   */
  setItemHostAccess(id, hostAccess) {}

  /**
   * @param {string} id
   * @param {boolean} collectsErrors
   */
  setItemCollectsErrors(id, collectsErrors) {}

  /**
   * @param {string} id
   * @param {chrome.developerPrivate.ExtensionView} view
   */
  inspectItemView(id, view) {}

  /**
   * @param {string} url
   */
  openUrl(url) {}

  /**
   * @param {string} id
   * @return {!Promise}
   */
  reloadItem(id) {}

  /** @param {string} id */
  repairItem(id) {}

  /** @param {!chrome.developerPrivate.ExtensionInfo} extension */
  showItemOptionsPage(extension) {}

  /** @param {string} id */
  showInFolder(id) {}

  /**
   * @param {string} id
   * @return {!Promise<string>}
   */
  getExtensionSize(id) {}

  /**
   * @param {string} id
   * @param {string} host
   * @return {!Promise<void>}
   */
  addRuntimeHostPermission(id, host) {}

  /**
   * @param {string} id
   * @param {string} host
   * @return {!Promise<void>}
   */
  removeRuntimeHostPermission(id, host) {}

  // TODO(tjudkins): This function is not specific to items, so should be pulled
  // out to a more generic place when we need to access it from elsewhere.
  /** @param {string} metricName */
  recordUserAction(metricName) {}
}

Polymer({
  is: 'extensions-item',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, ItemBehavior],

  properties: {
    // The item's delegate, or null.
    delegate: {
      type: Object,
    },

    // Whether or not dev mode is enabled.
    inDevMode: {
      type: Boolean,
      value: false,
    },

    // The underlying ExtensionInfo itself. Public for use in declarative
    // bindings.
    /** @type {chrome.developerPrivate.ExtensionInfo} */
    data: {
      type: Object,
    },

    // Whether or not the expanded view of the item is shown.
    /** @private */
    showingDetails_: {
      type: Boolean,
      value: false,
    },
  },

  /** Prevents reloading the same item while it's already being reloaded. */
  isReloading_: false,

  observers: [
    'observeIdVisibility_(inDevMode, showingDetails_, data.id)',
  ],

  /** @return {!HTMLElement} The "Details" button. */
  getDetailsButton() {
    return /** @type {!HTMLElement} */ (this.$.detailsButton);
  },

  /** @return {?HTMLElement} The "Errors" button, if it exists. */
  getErrorsButton() {
    return /** @type {?HTMLElement} */ (this.$$('#errors-button'));
  },

  /** @private string */
  a11yAssociation_() {
    // Don't use I18nBehavior.i18n because of additional checks it performs.
    // Polymer ensures that this string is not stamped into arbitrary HTML.
    // |this.data.name| can contain any data including html tags.
    // ex: "My <video> download extension!"
    return loadTimeData.getStringF('extensionA11yAssociation', this.data.name);
  },

  /** @private */
  observeIdVisibility_(inDevMode, showingDetails, id) {
    flush();
    const idElement = this.$$('#extension-id');
    if (idElement) {
      assert(this.data);
      idElement.innerHTML = this.i18n('itemId', this.data.id);
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowErrorsButton_() {
    // When the error console is disabled (happens when
    // --disable-error-console command line flag is used or when in the
    // Stable/Beta channel), |installWarnings| is populated.
    if (this.data.installWarnings && this.data.installWarnings.length > 0) {
      return true;
    }

    // When error console is enabled |installedWarnings| is not populated.
    // Instead |manifestErrors| and |runtimeErrors| are used.
    return this.data.manifestErrors.length > 0 ||
        this.data.runtimeErrors.length > 0;
  },

  /** @private */
  onRemoveTap_() {
    this.delegate.deleteItem(this.data.id);
  },

  /** @private */
  onEnableToggleChange_() {
    this.delegate.setItemEnabled(this.data.id, this.$.enableToggle.checked);
    this.$.enableToggle.checked = this.isEnabled_();
  },

  /** @private */
  onErrorsTap_() {
    if (this.data.installWarnings && this.data.installWarnings.length > 0) {
      this.fire('show-install-warnings', this.data.installWarnings);
      return;
    }

    navigation.navigateTo({page: Page.ERRORS, extensionId: this.data.id});
  },

  /** @private */
  onDetailsTap_() {
    navigation.navigateTo({page: Page.DETAILS, extensionId: this.data.id});
  },

  /**
   * @param {!{model: !{item: !chrome.developerPrivate.ExtensionView}}} e
   * @private
   */
  onInspectTap_(e) {
    this.delegate.inspectItemView(this.data.id, this.data.views[0]);
  },

  /** @private */
  onExtraInspectTap_() {
    navigation.navigateTo({page: Page.DETAILS, extensionId: this.data.id});
  },

  /** @private */
  onReloadTap_() {
    // Don't reload if in the middle of an update.
    if (this.isReloading_) {
      return;
    }

    this.isReloading_ = true;

    const toastManager = getToastManager();
    // Keep the toast open indefinitely.
    toastManager.duration = 0;
    toastManager.show(this.i18n('itemReloading'));
    this.delegate.reloadItem(this.data.id)
        .then(
            () => {
              toastManager.hide();
              toastManager.duration = 3000;
              toastManager.show(this.i18n('itemReloaded'));
              this.isReloading_ = false;
            },
            loadError => {
              this.fire('load-error', loadError);
              toastManager.hide();
              this.isReloading_ = false;
            });
  },

  /** @private */
  onRepairTap_() {
    this.delegate.repairItem(this.data.id);
  },

  /**
   * @return {boolean}
   * @private
   */
  isEnabled_() {
    return isEnabled(this.data.state);
  },

  /**
   * @return {boolean}
   * @private
   */
  isEnableToggleEnabled_() {
    return userCanChangeEnablement(this.data);
  },

  /**
   * Returns true if the reload button should be shown.
   * @return {boolean}
   * @private
   */
  showReloadButton_() {
    return getEnableControl(this.data) === EnableControl.RELOAD;
  },

  /**
   * Returns true if the repair button should be shown.
   * @return {boolean}
   * @private
   */
  showRepairButton_() {
    return getEnableControl(this.data) === EnableControl.REPAIR;
  },


  /**
   * Returns true if the enable toggle should be shown.
   * @return {boolean}
   * @private
   */
  showEnableToggle_() {
    return getEnableControl(this.data) === EnableControl.ENABLE_TOGGLE;
  },

  /**
   * return {string}
   * @private
   */
  computeClasses_() {
    let classes = this.isEnabled_() ? 'enabled' : 'disabled';
    if (this.inDevMode) {
      classes += ' dev-mode';
    }
    return classes;
  },

  /**
   * @return {string}
   * @private
   */
  computeSourceIndicatorIcon_() {
    switch (getItemSource(this.data)) {
      case SourceType.POLICY:
        return 'extensions-icons:business';
      case SourceType.SIDELOADED:
        return 'extensions-icons:input';
      case SourceType.UNKNOWN:
        // TODO(dpapad): Ask UX for a better icon for this case.
        return 'extensions-icons:input';
      case SourceType.UNPACKED:
        return 'extensions-icons:unpacked';
      case SourceType.WEBSTORE:
        return '';
    }
    assertNotReached();
  },

  /**
   * @return {string}
   * @private
   */
  computeSourceIndicatorText_() {
    if (this.data.locationText) {
      return this.data.locationText;
    }

    const sourceType = getItemSource(this.data);
    return sourceType === SourceType.WEBSTORE ? '' :
                                                getItemSourceString(sourceType);
  },

  /**
   * @return {boolean}
   * @private
   */
  computeInspectViewsHidden_() {
    return !this.data.views || this.data.views.length === 0;
  },

  /**
   * @return {string}
   * @private
   */
  computeFirstInspectTitle_() {
    // Note: theoretically, this wouldn't be called without any inspectable
    // views (because it's in a dom-if="!computeInspectViewsHidden_()").
    // However, due to the recycling behavior of iron list, it seems that
    // sometimes it can. Even when it is, the UI behaves properly, but we
    // need to handle the case gracefully.
    return this.data.views.length > 0 ?
        computeInspectableViewLabel(this.data.views[0]) :
        '';
  },

  /**
   * @return {string}
   * @private
   */
  computeFirstInspectLabel_() {
    const label = this.computeFirstInspectTitle_();
    return label && this.data.views.length > 1 ? label + ',' : label;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeExtraViewsHidden_() {
    return this.data.views.length <= 1;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeDevReloadButtonHidden_() {
    // Only display the reload spinner if the extension is unpacked and
    // enabled or disabled for reload. If an extension fails to reload (due to
    // e.g. a parsing error), it will
    // remain disabled with the "reloading" reason. We show the reload button
    // when it's disabled for reload to enable developers to reload the fixed
    // version. (Note that trying to reload an extension that is currently
    // trying to reload is a no-op.) For other
    // disableReasons, there's no point in reloading a disabled extension, and
    // we'll show a crashed reload button if it's terminated.
    const showIcon =
        this.data.location === chrome.developerPrivate.Location.UNPACKED &&
        (this.data.state === chrome.developerPrivate.ExtensionState.ENABLED ||
         this.data.disableReasons.reloading);
    return !showIcon;
  },

  /**
   * @return {string}
   * @private
   */
  computeExtraInspectLabel_() {
    return this.i18n(
        'itemInspectViewsExtra', (this.data.views.length - 1).toString());
  },

  /**
   * @return {boolean}
   * @private
   */
  hasWarnings_() {
    return this.data.disableReasons.corruptInstall ||
        this.data.disableReasons.suspiciousInstall ||
        this.data.runtimeWarnings.length > 0 || !!this.data.blacklistText;
  },

  /**
   * @return {string}
   * @private
   */
  computeWarningsClasses_() {
    return this.data.blacklistText ? 'severe' : 'mild';
  },
});
