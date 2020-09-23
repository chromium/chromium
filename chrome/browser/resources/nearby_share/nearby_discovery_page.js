// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-discovery-page' component shows the discovery UI of
 * the Nearby Share flow. It shows a list of devices to select from.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import './nearby_device.js';
import './nearby_preview.js';
import './nearby_share_target_types.mojom-lite.js';
import './nearby_share.mojom-lite.js';
import './shared/nearby_page_template.m.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getDiscoveryManager} from './discovery_manager.js';

/**
 * Converts an unguessable token to a string.
 * @param {!mojoBase.mojom.UnguessableToken} token
 * @return {!string}
 */
function tokenToString(token) {
  return `${token.high.toString()}#${token.low.toString()}`;
}

/**
 * Compares two unguessable tokens.
 * @param {!mojoBase.mojom.UnguessableToken} a
 * @param {!mojoBase.mojom.UnguessableToken} b
 */
function tokensEqual(a, b) {
  return a.high === b.high && a.low === b.low;
}

Polymer({
  is: 'nearby-discovery-page',

  behaviors: [I18nBehavior],

  _template: html`{__html_template__}`,

  properties: {
    /**
     * The description of the attachments
     * @type {?string}
     */
    attachmentsDescription: {
      type: String,
      value: null,
    },

    /**
     * ConfirmationManager interface for the currently selected share target.
     * @type {?nearbyShare.mojom.ConfirmationManagerInterface}
     */
    confirmationManager: {
      notify: true,
      type: Object,
      value: null,
    },

    /**
     * TransferUpdateListener interface for the currently selected share target.
     * @type {?nearbyShare.mojom.TransferUpdateListenerPendingReceiver}
     */
    transferUpdateListener: {
      notify: true,
      type: Object,
      value: null,
    },

    /**
     * The currently selected share target.
     * @type {?nearbyShare.mojom.ShareTarget}
     */
    selectedShareTarget: {
      notify: true,
      type: Object,
      value: null,
    },

    /**
     * A list of all discovered nearby share targets.
     * @private {!Array<!nearbyShare.mojom.ShareTarget>}
     */
    shareTargets_: {
      type: Array,
      value: [],
    },
  },

  listeners: {
    'next': 'onNext_',
    'view-enter-start': 'onViewEnterStart_',
    'view-exit-finish': 'onViewExitFinish_',
  },

  /** @private {?nearbyShare.mojom.ShareTargetListenerCallbackRouter} */
  mojoEventTarget_: null,

  /** @private {Array<number>} */
  listenerIds_: null,

  /** @private {Map<!string,!nearbyShare.mojom.ShareTarget>} */
  shareTargetMap_: null,

  /** @private {?nearbyShare.mojom.ShareTarget} */
  lastSelectedShareTarget_: null,

  /** @type {ResizeObserver} used to observer size changes to this element */
  resizeObserver_: null,

  /** @override */
  attached() {
    this.shareTargetMap_ = new Map();
    this.clearShareTargets_();

    // This is a required work around to get the iron-list to display on first
    // view. Currently iron-list won't generate item elements on attach if the
    // element is not visible. Because we are hosted in a cr-view-manager for
    // on-boarding, this component is not visible when the items are bound. To
    // fix this issue, we listen for resize events (which happen when display is
    // switched from none to block by the view manager) and manually call
    // notifyResize on the iron-list
    this.resizeObserver_ = new ResizeObserver(entries => {
      const deviceList =
          /** @type {IronListElement} */ (this.$$('#deviceList'));
      if (deviceList) {
        deviceList.notifyResize();
      }
    });
    this.resizeObserver_.observe(this);
  },

  /** @override */
  detached() {
    this.stopDiscovery_();
    this.resizeObserver_.disconnect();
  },

  /** @private */
  onViewEnterStart_() {
    this.startDiscovery_();
  },

  /** @private */
  onViewExitFinish_() {
    this.stopDiscovery_();
  },

  /** @private */
  startDiscovery_() {
    if (this.mojoEventTarget_) {
      return;
    }

    this.clearShareTargets_();

    this.mojoEventTarget_ =
        new nearbyShare.mojom.ShareTargetListenerCallbackRouter();

    this.listenerIds_ = [
      this.mojoEventTarget_.onShareTargetDiscovered.addListener(
          this.onShareTargetDiscovered_.bind(this)),
      this.mojoEventTarget_.onShareTargetLost.addListener(
          this.onShareTargetLost_.bind(this)),
    ];

    getDiscoveryManager().getSendPreview().then(result => {
      this.attachmentsDescription = result.sendPreview.description;
      // TODO (vecore): Setup icon and handle case of more than one attachment.
    });

    getDiscoveryManager()
        .startDiscovery(this.mojoEventTarget_.$.bindNewPipeAndPassRemote())
        .then(response => {
          if (!response.success) {
            // TODO(crbug.com/1123934): Show error.
            return;
          }
        });
  },

  /** @private */
  stopDiscovery_() {
    if (!this.mojoEventTarget_) {
      return;
    }

    this.clearShareTargets_();
    this.listenerIds_.forEach(
        id => assert(this.mojoEventTarget_.removeListener(id)));
    this.mojoEventTarget_.$.close();
    this.mojoEventTarget_ = null;
  },

  /** @private */
  clearShareTargets_() {
    this.shareTargetMap_.clear();
    this.lastSelectedShareTarget_ = null;
    this.selectedShareTarget = null;
    this.shareTargets_ = [];
  },

  /**
   * @private
   * @param {!nearbyShare.mojom.ShareTarget} shareTarget The discovered device.
   */
  onShareTargetDiscovered_(shareTarget) {
    const shareTargetId = tokenToString(shareTarget.id);
    if (!this.shareTargetMap_.has(shareTargetId)) {
      this.push('shareTargets_', shareTarget);
    } else {
      const index = this.shareTargets_.findIndex(
          (target) => tokensEqual(target.id, shareTarget.id));
      assert(index !== -1);
      this.splice('shareTargets_', index, 1, shareTarget);
      this.updateSelectedShareTarget_(shareTarget.id, shareTarget);
    }
    this.shareTargetMap_.set(shareTargetId, shareTarget);
  },

  /**
   * @private
   * @param {!nearbyShare.mojom.ShareTarget} shareTarget The lost device.
   */
  onShareTargetLost_(shareTarget) {
    const index = this.shareTargets_.findIndex(
        (target) => tokensEqual(target.id, shareTarget.id));
    assert(index !== -1);
    this.splice('shareTargets_', index, 1);
    this.shareTargetMap_.delete(tokenToString(shareTarget.id));
    this.updateSelectedShareTarget_(shareTarget.id, /*shareTarget=*/ null);
  },

  /** @private */
  onNext_() {
    if (!this.selectedShareTarget) {
      return;
    }

    getDiscoveryManager()
        .selectShareTarget(this.selectedShareTarget.id)
        .then(response => {
          const {result, transferUpdateListener, confirmationManager} =
              response;
          if (result !== nearbyShare.mojom.SelectShareTargetResult.kOk) {
            // TODO(crbug.com/crbug.com/1123934): Show error.
            return;
          }

          this.confirmationManager = confirmationManager;
          this.transferUpdateListener = transferUpdateListener;
          this.fire('change-page', {page: 'confirmation'});
        });
  },

  /** @private */
  onSelectedShareTargetChanged_() {
    // <iron-list> causes |this.$.deviceList.selectedItem| to be null if tapped
    // a second time. Manually reselect the last item to preserve selection.
    if (!this.$.deviceList.selectedItem && this.lastSelectedShareTarget_) {
      this.$.deviceList.selectItem(this.lastSelectedShareTarget_);
    }
    this.lastSelectedShareTarget_ = this.$.deviceList.selectedItem;
  },

  /**
   * @param {!nearbyShare.mojom.ShareTarget} shareTarget
   * @return {boolean}
   * @private
   */
  isShareTargetSelected_(shareTarget) {
    return this.selectedShareTarget === shareTarget;
  },

  /**
   * Updates the selected share target to |shareTarget| if its id matches |id|.
   * @param {!mojoBase.mojom.UnguessableToken} id
   * @param {?nearbyShare.mojom.ShareTarget} shareTarget
   * @private
   */
  updateSelectedShareTarget_(id, shareTarget) {
    if (this.selectedShareTarget &&
        tokensEqual(this.selectedShareTarget.id, id)) {
      this.lastSelectedShareTarget_ = shareTarget;
      this.selectedShareTarget = shareTarget;
    }
  },
});
