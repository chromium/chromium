// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-discovery-page' component shows the discovery UI of
 * the Nearby Share flow. It shows a list of devices to select from.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_lottie/cr_lottie.m.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import './shared/nearby_device.m.js';
import './mojo/nearby_share_target_types.mojom-lite.js';
import './mojo/nearby_share_share_type.mojom-lite.js';
import './mojo/nearby_share.mojom-lite.js';
import './shared/nearby_page_template.m.js';
import './shared/nearby_preview.m.js';
import './strings.m.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getDiscoveryManager, observeDiscoveryManager} from './discovery_manager.js';

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
     * Preview info for the file(s) to be shared.
     * @type {?nearbyShare.mojom.PayloadPreview}
     */
    payloadPreview: {
      notify: true,
      type: Object,
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

    /**
     * Header text for error. The error section is not displayed if this is
     * falsey.
     * @private {?string}
     */
    errorTitle_: {
      type: String,
      value: null,
    },

    /**
     * Description text for error, displayed under the error title.
     * @private {?string}
     */
    errorDescription_: {
      type: String,
      value: null,
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

  /** @private {?nearbyShare.mojom.DiscoveryObserverReceiver} */
  discoveryObserver_: null,

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
    this.discoveryObserver_ = observeDiscoveryManager(
        /** @type {!nearbyShare.mojom.DiscoveryObserverInterface} */ (this));
  },

  /** @override */
  detached() {
    this.stopDiscovery_();
    this.resizeObserver_.disconnect();
    if (this.discoveryObserver_) {
      this.discoveryObserver_.$.close();
    }
  },

  /**
   * @return {!Array<!nearbyShare.mojom.ShareTarget>}
   * @public
   */
  getShareTargetsForTesting() {
    return this.shareTargets_;
  },

  /**
   * @param {nearbyShare.mojom.ShareTarget} shareTarget
   * @return {boolean} True if share target found
   * @public
   */
  selectShareTargetForTesting(shareTarget) {
    const token = tokenToString(shareTarget.id);
    if (this.shareTargetMap_.has(token)) {
      this.selectShareTarget_(this.shareTargetMap_.get(token));
      return true;
    }
    return false;
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

    getDiscoveryManager().getPayloadPreview().then(result => {
      this.payloadPreview = result.payloadPreview;
    });

    getDiscoveryManager()
        .startDiscovery(this.mojoEventTarget_.$.bindNewPipeAndPassRemote())
        .then(response => {
          switch (response.result) {
            case nearbyShare.mojom.StartDiscoveryResult.kErrorGeneric:
              this.errorTitle_ = this.i18n('nearbyShareErrorCantShare');
              this.errorDescription_ =
                  this.i18n('nearbyShareErrorSomethingWrong');
              return;
            case nearbyShare.mojom.StartDiscoveryResult
                .kErrorInProgressTransferring:
              this.errorTitle_ = this.i18n('nearbyShareErrorCantShare');
              this.errorDescription_ =
                  this.i18n('nearbyShareErrorTransferInProgress');
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

  /**
   * Mojo callback when the Nearby utility process stops.
   * @public
   */
  onNearbyProcessStopped() {
    if (!this.errorTitle_) {
      this.errorTitle_ = this.i18n('nearbyShareErrorCantShare');
      this.errorDescription_ = this.i18n('nearbyShareErrorSomethingWrong');
    }
  },

  /**
   * Mojo callback when discovery is started.
   * @param {boolean} success
   * @public
   */
  onStartDiscoveryResult(success) {
    if (!success && !this.errorTitle_) {
      this.errorTitle_ = this.i18n('nearbyShareErrorCantShare');
      this.errorDescription_ = this.i18n('nearbyShareErrorSomethingWrong');
    }
  },

  /** @private */
  clearShareTargets_() {
    if (this.shareTargetMap_) {
      this.shareTargetMap_.clear();
    }
    this.lastSelectedShareTarget_ = null;
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
    if (this.selectedShareTarget) {
      this.selectShareTarget_(this.selectedShareTarget);
    }
  },

  /**
   * Select the given share target and proceed to the confirmation page.
   * @param {!nearbyShare.mojom.ShareTarget} shareTarget
   * @private
   */
  selectShareTarget_(shareTarget) {
    getDiscoveryManager().selectShareTarget(shareTarget.id).then(response => {
      const {result, transferUpdateListener, confirmationManager} = response;
      if (result !== nearbyShare.mojom.SelectShareTargetResult.kOk) {
        this.errorTitle_ = this.i18n('nearbyShareErrorCantShare');
        this.errorDescription_ = this.i18n('nearbyShareErrorSomethingWrong');
        return;
      }

      this.confirmationManager = confirmationManager;
      this.transferUpdateListener = transferUpdateListener;
      this.fire('change-page', {page: 'confirmation'});
    });
  },

  /** @private */
  onSelectedShareTargetChanged_() {
    const deviceList = this.$$('#deviceList');
    if (!deviceList) {
      // deviceList is in dom-if and may not be found
      return;
    }

    // <iron-list> causes |selectedItem| to be null if tapped a second time.
    // Manually reselect the last item to preserve selection.
    if (!deviceList.selectedItem && this.lastSelectedShareTarget_) {
      // Use async to make sure this happens after |selectedItem| gets its
      // final value.
      this.async(() => {
        const deviceList = this.$$('#deviceList');
        if (!deviceList.selectedItem) {
          deviceList.selectItem(this.lastSelectedShareTarget_);
        }
      });
    } else {
      this.lastSelectedShareTarget_ = deviceList.selectedItem;
    }
  },

  /**
   * @param {boolean} bool
   * @return {string}
   * @private
   */
  boolToString_(bool) {
    return bool.toString();
  },

  /**
   * @return {boolean}
   * @private
   */
  isShareTargetsEmpty_() {
    return this.shareTargets_.length === 0;
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

  /**
   * Builds the html for the help text, applying the appropriate aria labels,
   * and setting the href of the link. This function is largely
   * copied from getAriaLabelledContent_ in <settings-localized-link>, which
   * can't be used directly because this isn't part of settings.
   * TODO(crbug.com/1170849): Extract this logic into a general method.
   * @return {string}
   * @private
   */
  getAriaLabelledHelpText_() {
    const tempEl = document.createElement('div');
    const localizedString = this.i18nAdvanced('nearbyShareDiscoveryPageInfo');
    const linkUrl = this.i18n('nearbyShareLearnMoreLink');
    tempEl.innerHTML = localizedString;

    const ariaLabelledByIds = [];
    tempEl.childNodes.forEach((node, index) => {
      // Text nodes should be aria-hidden and associated with an element id
      // that the anchor element can be aria-labelledby.
      if (node.nodeType == Node.TEXT_NODE) {
        const spanNode = document.createElement('span');
        spanNode.textContent = node.textContent;
        spanNode.id = `helpText${index}`;
        ariaLabelledByIds.push(spanNode.id);
        spanNode.setAttribute('aria-hidden', true);
        node.replaceWith(spanNode);
        return;
      }
      // The single element node with anchor tags should also be aria-labelledby
      // itself in-order with respect to the entire string.
      if (node.nodeType == Node.ELEMENT_NODE && node.nodeName == 'A') {
        node.id = `helpLink`;
        ariaLabelledByIds.push(node.id);
        return;
      }

      // Only text and <a> nodes are allowed.
      assertNotReached('nearbyShareDiscoveryPageInfo has invalid node types');
    });

    const anchorTags = tempEl.getElementsByTagName('a');
    // In the event the localizedString contains only text nodes, populate the
    // contents with the localizedString.
    if (anchorTags.length == 0) {
      return localizedString;
    }

    assert(
        anchorTags.length == 1,
        'nearbyShareDiscoveryPageInfo should contain exactly one anchor tag');
    const anchorTag = anchorTags[0];
    anchorTag.setAttribute('aria-labelledby', ariaLabelledByIds.join(' '));
    anchorTag.href = linkUrl;
    anchorTag.target = '_blank';

    return tempEl.innerHTML;
  },
});
