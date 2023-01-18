// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-discovery-page' component shows the discovery UI of
 * the Nearby Share flow. It shows a list of devices to select from.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_lottie/cr_lottie.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import './shared/nearby_device.js';
import './shared/nearby_page_template.js';
import './shared/nearby_preview.js';
import './strings.m.js';

import {ConfirmationManagerInterface, DiscoveryObserverInterface, DiscoveryObserverReceiver, PayloadPreview, SelectShareTargetResult, ShareTarget, ShareTargetListenerCallbackRouter, StartDiscoveryResult, TransferUpdateListenerPendingReceiver} from '/mojo/nearby_share.mojom-webui.js';
import {assert, assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getDiscoveryManager, observeDiscoveryManager} from './discovery_manager.js';
import {getTemplate} from './nearby_discovery_page.html.js';

/**
 * Converts an unguessable token to a string.
 * @param {!UnguessableToken} token
 * @return {!string}
 */
function tokenToString(token) {
  return `${token.high.toString()}#${token.low.toString()}`;
}

/**
 * Compares two unguessable tokens.
 * @param {!UnguessableToken} a
 * @param {!UnguessableToken} b
 */
function tokensEqual(a, b) {
  return a.high === b.high && a.low === b.low;
}

/**
 * The pulse animation asset URL for light mode.
 * @type {string}
 */
const PULSE_ANIMATION_URL_LIGHT = 'nearby_share_pulse_animation_light.json';

/**
 * The pulse animation asset URL for dark mode.
 */
const PULSE_ANIMATION_URL_DARK = 'nearby_share_pulse_animation_dark.json';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const NearbyDiscoveryPageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class NearbyDiscoveryPageElement extends NearbyDiscoveryPageElementBase {
  static get is() {
    return 'nearby-discovery-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preview info for the file(s) to be shared.
       * @type {?PayloadPreview}
       */
      payloadPreview: {
        notify: true,
        type: Object,
        value: null,
      },

      /**
       * ConfirmationManager interface for the currently selected share target.
       * @type {?ConfirmationManagerInterface}
       */
      confirmationManager: {
        notify: true,
        type: Object,
        value: null,
      },

      /**
       * TransferUpdateListener interface for the currently selected share
       * target.
       * @type {?TransferUpdateListenerPendingReceiver}
       */
      transferUpdateListener: {
        notify: true,
        type: Object,
        value: null,
      },

      /**
       * The currently selected share target.
       * @type {?ShareTarget}
       */
      selectedShareTarget: {
        notify: true,
        type: Object,
        value: null,
      },

      /**
       * A list of all discovered nearby share targets.
       * @private {!Array<!ShareTarget>}
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

      /**
       * Whether the discovery page is being rendered in dark mode.
       * @private {boolean}
       */
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();

    /** @private {?ShareTargetListenerCallbackRouter} */
    this.mojoEventTarget_ = null;

    /** @private {Array<number>} */
    this.listenerIds_ = null;

    /** @private {Map<!string,!ShareTarget>} */
    this.shareTargetMap_ = null;

    /** @private {?DiscoveryObserverReceiver} */
    this.discoveryObserver_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.shareTargetMap_ = new Map();
    this.clearShareTargets_();
    this.discoveryObserver_ = observeDiscoveryManager(
        /** @type {!DiscoveryObserverInterface} */ (this));
  }


  /** @override */
  ready() {
    super.ready();

    this.addEventListener('next', this.onNext_);
    this.addEventListener('view-enter-start', this.onViewEnterStart_);
    this.addEventListener('view-exit-finish', this.onViewExitFinish_);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();

    this.stopDiscovery_();
    if (this.discoveryObserver_) {
      this.discoveryObserver_.$.close();
    }
  }

  /**
   * @return {!Array<!ShareTarget>}
   * @public
   */
  getShareTargetsForTesting() {
    return this.shareTargets_;
  }

  /**
   * @param {ShareTarget} shareTarget
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
  }

  /** @private */
  onViewEnterStart_() {
    this.startDiscovery_();
  }

  /** @private */
  onViewExitFinish_() {
    this.stopDiscovery_();
  }

  /** @private */
  startDiscovery_() {
    if (this.mojoEventTarget_) {
      return;
    }

    this.clearShareTargets_();

    this.mojoEventTarget_ = new ShareTargetListenerCallbackRouter();

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
            case StartDiscoveryResult.kErrorGeneric:
              this.errorTitle_ = this.i18n('nearbyShareErrorCantShare');
              this.errorDescription_ =
                  this.i18n('nearbyShareErrorSomethingWrong');
              return;
            case StartDiscoveryResult.kErrorInProgressTransferring:
              this.errorTitle_ = this.i18n('nearbyShareErrorCantShare');
              this.errorDescription_ =
                  this.i18n('nearbyShareErrorTransferInProgress');
              return;
            case StartDiscoveryResult.kNoConnectionMedium:
              this.errorTitle_ =
                  this.i18n('nearbyShareErrorNoConnectionMedium');
              this.errorDescription_ =
                  this.i18n('nearbyShareErrorNoConnectionMediumDescription');
              return;
          }
        });
  }

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
  }

  /**
   * Mojo callback when the Nearby utility process stops.
   * @public
   */
  onNearbyProcessStopped() {
    if (!this.errorTitle_) {
      this.errorTitle_ = this.i18n('nearbyShareErrorCantShare');
      this.errorDescription_ = this.i18n('nearbyShareErrorSomethingWrong');
    }
  }

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
  }

  /** @private */
  clearShareTargets_() {
    if (this.shareTargetMap_) {
      this.shareTargetMap_.clear();
    }
    this.shareTargets_ = [];
  }

  /**
   * Guides selection process for share target list, which is used by
   * screen readers to iterate and select items, when keys are pressed on
   * the share target list.
   * @param {Event} event containing the key
   * @private
   */
  onKeyDownForShareTarget_(event) {
    const currentShareTarget = event.currentTarget.shareTarget;
    const currentIndex = this.shareTargets_.findIndex(
        (target) => tokensEqual(target.id, currentShareTarget.id));
    event.stopPropagation();
    switch (event.code) {
      // Down arrow: bring into focus the next shareTarget in list.
      case 'ArrowDown':
        this.focusShareTarget_(currentIndex + 1);
        break;
      // Up arrow: bring into focus the previous shareTarget in list.
      case 'ArrowUp':
        this.focusShareTarget_(currentIndex - 1);
        break;
      // Space key: Select the shareTarget
      case 'Space':
      // Enter key: Select the shareTarget
      case 'Enter':
        this.selectShareTargetOnUserInput_(currentShareTarget);
        break;
    }
  }

  /**
   * Focuses the element that corresponds to the share target at |index|.
   * @param {number} index in |shareTargets_| and also the dom-repeat list.
   * @private
   */
  focusShareTarget_(index) {
    const container = this.shadowRoot.querySelector('.device-list-container');
    const nearbyDeviceElements = container.querySelectorAll('nearby-device');

    if (index >= 0 && index < nearbyDeviceElements.length) {
      nearbyDeviceElements[index].focus();
    }
  }

  /**
   * Selects the shareTarget when clicked.
   * @private
   * @param {Event} event
   */
  onShareTargetClicked_(event) {
    event.preventDefault();
    const currentShareTarget = event.currentTarget.shareTarget;
    this.selectShareTargetOnUserInput_(currentShareTarget);
  }

  /**
   * Selects the shareTarget when selected by the user, either through click
   * or key press.
   * @private
   * @param {!ShareTarget} shareTarget
   */
  selectShareTargetOnUserInput_(shareTarget) {
    if (this.isShareTargetSelected_(shareTarget)) {
      return;
    }

    this.selectedShareTarget = shareTarget;
    const selector = this.shadowRoot.querySelector('#selector');
    selector.select(this.selectedShareTarget);
  }

  /**
   * @private
   * @param {!ShareTarget} shareTarget
   * @return {boolean} True if shareTarget is currently selected
   */
  isShareTargetSelected_(shareTarget) {
    return !!this.selectedShareTarget && !!shareTarget &&
        tokensEqual(this.selectedShareTarget.id, shareTarget.id);
  }

  /**
   * @private
   * @param {!ShareTarget} shareTarget The discovered device.
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
  }

  /**
   * @private
   * @param {!ShareTarget} shareTarget The lost device.
   */
  onShareTargetLost_(shareTarget) {
    const index = this.shareTargets_.findIndex(
        (target) => tokensEqual(target.id, shareTarget.id));
    assert(index !== -1);
    this.splice('shareTargets_', index, 1);
    this.shareTargetMap_.delete(tokenToString(shareTarget.id));
    this.updateSelectedShareTarget_(shareTarget.id, /*shareTarget=*/ null);
  }

  /** @private */
  onNext_() {
    if (this.selectedShareTarget) {
      this.selectShareTarget_(this.selectedShareTarget);
    }
  }

  /**
   * Select the given share target and proceed to the confirmation page.
   * @param {!ShareTarget} shareTarget
   * @private
   */
  selectShareTarget_(shareTarget) {
    getDiscoveryManager().selectShareTarget(shareTarget.id).then(response => {
      const {result, transferUpdateListener, confirmationManager} = response;
      if (result !== SelectShareTargetResult.kOk) {
        this.errorTitle_ = this.i18n('nearbyShareErrorCantShare');
        this.errorDescription_ = this.i18n('nearbyShareErrorSomethingWrong');
        return;
      }

      this.confirmationManager = confirmationManager;
      this.transferUpdateListener = transferUpdateListener;
      this.dispatchEvent(new CustomEvent(
          'change-page',
          {bubles: true, composed: true, detail: {page: 'confirmation'}}));
    });
  }

  /** @private */
  onSelectedShareTargetChanged_() {
    const deviceList = this.shadowRoot.querySelector('#deviceList');
    const selector = this.shadowRoot.querySelector('#selector');
    if (!deviceList) {
      // deviceList is in dom-if and may not be found
      return;
    }

    this.selectedShareTarget = selector.selectedItem;
  }

  /**
   * @param {!ShareTarget} shareTarget
   * @return {string}
   * @private
   */
  isShareTargetSelectedToString_(shareTarget) {
    return this.isShareTargetSelected_(shareTarget).toString();
  }

  /**
   * @return {boolean}
   * @private
   */
  isShareTargetsEmpty_() {
    return this.shareTargets_.length === 0;
  }

  /**
   * Updates the selected share target to |shareTarget| if its id matches |id|.
   * @param {!UnguessableToken} id
   * @param {?ShareTarget} shareTarget
   * @private
   */
  updateSelectedShareTarget_(id, shareTarget) {
    if (this.selectedShareTarget &&
        tokensEqual(this.selectedShareTarget.id, id)) {
      this.selectedShareTarget = shareTarget;
      const selector = this.shadowRoot.querySelector('#selector');
      selector.select(this.selectedShareTarget);
    }
  }

  /**
   * If the shareTarget is the first in the list, it's tab index should be 0
   * if there is no current selected share target. Otherwise, the tab index
   * should be 0 if it is the current selected share target. Tab index of 0
   *  allows users to navigate to it with tabs, and others should be -1
   * so users will not navigate to by tab.
   * @param {?ShareTarget} shareTarget
   * @return {string}
   * @private
   */
  getTabIndexOfShareTarget_(shareTarget) {
    if ((!this.selectedShareTarget && shareTarget === this.shareTargets_[0]) ||
        (shareTarget === this.selectedShareTarget)) {
      return '0';
    }
    return '-1';
  }

  /**
   * Builds the html for the help text, applying the appropriate aria labels,
   * and setting the href of the link. This function is largely
   * copied from getAriaLabelledContent_ in <localized-link>, which
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
      if (node.nodeType === Node.TEXT_NODE) {
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
      if (node.nodeType === Node.ELEMENT_NODE && node.nodeName === 'A') {
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
    if (anchorTags.length === 0) {
      return localizedString;
    }

    assert(
        anchorTags.length === 1,
        'nearbyShareDiscoveryPageInfo should contain exactly one anchor tag');
    const anchorTag = anchorTags[0];
    anchorTag.setAttribute('aria-labelledby', ariaLabelledByIds.join(' '));
    anchorTag.href = linkUrl;
    anchorTag.target = '_blank';

    return tempEl.innerHTML;
  }

  /**
   * Returns the URL for the asset that defines the discovery page's
   * pulsing background animation
   */
  getAnimationUrl_() {
    return this.isDarkModeActive_ ? PULSE_ANIMATION_URL_DARK :
                                    PULSE_ANIMATION_URL_LIGHT;
  }
}

customElements.define(
    NearbyDiscoveryPageElement.is, NearbyDiscoveryPageElement);
