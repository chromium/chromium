// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-multidevice-task-continuation-item' encapsulates
 * special logic for the phonehub task continuation item used in the multidevice
 * subpage.
 *
 * Task continuation depends on the 'Open Tabs' Chrome sync type being
 * activated. This component uses sync proxies from the people page to check
 * whether chrome sync is enabled.
 *
 * If it is enabled the multidevice feature item is used in the standard way,
 * otherwise the feature-controller and localized-link slots are overridden with
 * a disabled toggle and the task continuation localized string component that
 * is a special case containing two links.
 */

import './multidevice_feature_item.js';
import './multidevice_task_continuation_disabled_link.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import '../../settings_shared.css.js';

import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SyncBrowserProxyImpl} from '../../people_page/sync_browser_proxy.js';

import {MultiDeviceFeatureBehavior, MultiDeviceFeatureBehaviorInterface} from './multidevice_feature_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {MultiDeviceFeatureBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsMultideviceTaskContinuationItemElementBase = mixinBehaviors(
    [
      MultiDeviceFeatureBehavior,
      WebUIListenerBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsMultideviceTaskContinuationItemElement extends
    SettingsMultideviceTaskContinuationItemElementBase {
  static get is() {
    return 'settings-multidevice-task-continuation-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      isChromeTabsSyncEnabled_: {
        type: Boolean,
        value: false,
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!SyncBrowserProxy} */
    this.syncBrowserProxy_ = SyncBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener(
        'sync-prefs-changed', this.handleSyncPrefsChanged_.bind(this));

    // Cause handleSyncPrefsChanged_() to be called when the element is first
    // attached so that the state of |syncPrefs.tabsSynced| is known.
    this.syncBrowserProxy_.sendSyncPrefsChanged();
  }

  /** @override */
  focus() {
    if (!this.isChromeTabsSyncEnabled_) {
      this.shadowRoot.querySelector('cr-toggle').focus();
    } else {
      this.$.phoneHubTaskContinuationItem.focus();
    }
  }

  /**
   * Handler for when the sync preferences are updated.
   * @param {!SyncPrefs} syncPrefs
   * @private
   */
  handleSyncPrefsChanged_(syncPrefs) {
    this.isChromeTabsSyncEnabled_ = !!syncPrefs && syncPrefs.tabsSynced;
  }
}

customElements.define(
    SettingsMultideviceTaskContinuationItemElement.is,
    SettingsMultideviceTaskContinuationItemElement);
