// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-discovery-page' component shows the discovery UI of
 * the Nearby Share flow. It shows a list of devices to select from.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_lottie/cr_lottie.js';
import 'chrome://resources/cros_components/lottie_renderer/lottie-renderer.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '/shared/nearby_device.js';
import '/shared/nearby_page_template.js';
import '/shared/nearby_preview.js';
import './strings.m.js';

import type {NearbyDeviceElement} from '/shared/nearby_device.js';
import type {ConfirmationManagerInterface, DiscoveryObserverReceiver, PayloadPreview, ShareTarget, TransferUpdateListenerPendingReceiver} from '/shared/nearby_share.mojom-webui.js';
import {SelectShareTargetResult, ShareTargetListenerCallbackRouter, StartDiscoveryResult} from '/shared/nearby_share.mojom-webui.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import type {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import type {ArraySelector} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getDiscoveryManager, observeDiscoveryManager} from './discovery_manager.js';
import {getTemplate} from './nearby_discovery_page.html.js';

/**
 * Converts an unguessable token to a string.
 */
function tokenToString(token: UnguessableToken): string {
  return `${token.high.toString()}#${token.low.toString()}`;
}

/**
 * Compares two unguessable tokens.
 */
function tokensEqual(a: UnguessableToken, b: UnguessableToken): boolean {
  return a.high === b.high && a.low === b.low;
}

/**
 * The pulse animation asset URL.
 */
const PULSE_ANIMATION_URL: string = 'nearby_share_pulse_animation.json';

const NearbyDiscoveryPageElementBase = I18nMixin(PolymerElement);

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
       */
      payloadPreview: {
        notify: true,
        type: Object,
        value: null,
      },

      /**
       * ConfirmationManager interface for the currently selected share target.
       */
      confirmationManager: {
        notify: true,
        type: Object,
        value: null,
      },

      /**
       * TransferUpdateListener interface for the currently selected share
       * target.
       */
      transferUpdateListener: {
        notify: true,
        type: Object,
        value: null,
      },

      /**
       * The currently selected share target.
       */
      selectedShareTarget: {
        notify: true,
        type: Object,
        value: null,
      },

      /**
       * A list of all discovered nearby share targets.
       */
      shareTargets_: {
        type: Array,
        value: () => [],
      },

      /**
       * A list of all discovered nearby self-share targets.
       * Used only if isSelfShareEnabled is true, otherwise only |shareTargets_|
       * is used.
       */
      selfShareTargets_: {
        type: Array,
        value: () => [],
      },

      /**
       * A list of all discovered nearby non-self-share targets.
       * Used only if isSelfShareEnabled is true, otherwise only |shareTargets_|
       * is used.
       */
      nonSelfShareTargets_: {
        type: Array,
        value: () => [],
      },

      /**
       * Header text for error. The error section is not displayed if this is
       * falsey.
       */
      errorTitle_: {
        type: String,
        value: null,
      },

      /**
       * Description text for error, displayed under the error title.
       */
      errorDescription_: {
        type: String,
        value: null,
      },
    };
  }

  payloadPreview: PayloadPreview|null;
  confirmationManager: ConfirmationManagerInterface|null;
  transferUpdateListener: TransferUpdateListenerPendingReceiver|null;
  selectedShareTarget: ShareTarget|null;

  private shareTargets_: ShareTarget[];
  private selfShareTargets_: ShareTarget[];
  private nonSelfShareTargets_: ShareTarget[];
  private errorTitle_: string|null;
  private errorDescription_: string|null;

  private mojoEventTarget_: ShareTargetListenerCallbackRouter|null = null;
  private listenerIds_: number[]|null = null;
  private shareTargetMap_: Map<string, ShareTarget>|null = null;
  private discoveryObserver_: DiscoveryObserverReceiver|null = null;

  override connectedCallback() {
    super.connectedCallback();

    this.shareTargetMap_ = new Map();
    this.clearShareTargets_();
    this.discoveryObserver_ = observeDiscoveryManager(this);
  }

  override ready() {
    super.ready();

    this.addEventListener('next', this.onNext_);
    this.addEventListener('view-enter-start', this.onViewEnterStart_);
    this.addEventListener('view-exit-finish', this.onViewExitFinish_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.stopDiscovery_();
    if (this.discoveryObserver_) {
      this.discoveryObserver_.$.close();
    }
  }

  getShareTargetsForTesting(): ShareTarget[] {
    return this.shareTargets_;
  }

  /**
   * @return True if share target found
   */
  selectShareTargetForTesting(shareTarget: ShareTarget): boolean {
    const token = tokenToString(shareTarget.id);
    assert(this.shareTargetMap_);
    if (this.shareTargetMap_.has(token)) {
      this.selectShareTarget_(this.shareTargetMap_.get(token)!);
      return true;
    }
    return false;
  }

  private onViewEnterStart_() {
    this.startDiscovery_();
  }

  private onViewExitFinish_() {
    this.stopDiscovery_();
  }

  private startDiscovery_() {
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

  private stopDiscovery_() {
    if (!this.mojoEventTarget_) {
      return;
    }

    this.clearShareTargets_();
    assert(this.listenerIds_);
    this.listenerIds_.forEach(
        id => assert(this.mojoEventTarget_!.removeListener(id)));
    this.mojoEventTarget_.$.close();
    this.mojoEventTarget_ = null;
  }

  /**
   * Mojo callback when the Nearby utility process stops.
   */
  onNearbyProcessStopped() {
    if (!this.errorTitle_) {
      this.errorTitle_ = this.i18n('nearbyShareErrorCantShare');
      this.errorDescription_ = this.i18n('nearbyShareErrorSomethingWrong');
    }
  }

  /**
   * Mojo callback when discovery is started.
   */
  onStartDiscoveryResult(success: boolean) {
    if (!success && !this.errorTitle_) {
      this.errorTitle_ = this.i18n('nearbyShareErrorCantShare');
      this.errorDescription_ = this.i18n('nearbyShareErrorSomethingWrong');
    }
  }

  private clearShareTargets_() {
    if (this.shareTargetMap_) {
      this.shareTargetMap_.clear();
    }
    this.shareTargets_ = [];
    this.selfShareTargets_ = [];
    this.nonSelfShareTargets_ = [];
  }

  /**
   * Guides selection process for share target list, which is used by
   * screen readers to iterate and select items, when keys are pressed on
   * the share target list.
   */
  private onKeyDownForShareTarget_(event: KeyboardEvent) {
    const currentShareTarget =
        (event.currentTarget as NearbyDeviceElement).shareTarget;
    assert(currentShareTarget);
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
   * @param index in |shareTargets_| and also the dom-repeat list.
   */
  private focusShareTarget_(index: number) {
    const container = this.shadowRoot!.querySelector('#deviceLists');
    assert(container);
    const nearbyDeviceElements = container.querySelectorAll('nearby-device');

    if (index >= 0 && index < nearbyDeviceElements.length) {
      nearbyDeviceElements[index].focus();
    }
  }

  /**
   * Selects the shareTarget when clicked.
   */
  private onShareTargetClicked_(event: Event) {
    event.preventDefault();
    const currentShareTarget =
        (event.currentTarget as NearbyDeviceElement).shareTarget;
    assert(currentShareTarget);
    this.selectShareTargetOnUserInput_(currentShareTarget);
  }

  /**
   * Selects the shareTarget when selected by the user, either through click
   * or key press.
   */
  private selectShareTargetOnUserInput_(shareTarget: ShareTarget) {
    if (this.isShareTargetSelected_(shareTarget)) {
      return;
    }

    this.selectedShareTarget = shareTarget;
    const selector = this.shadowRoot!.querySelector<ArraySelector>('.selector');
    assert(selector);
    selector.select(this.selectedShareTarget);
  }

  /**
   * @return True if shareTarget is currently selected
   */
  private isShareTargetSelected_(shareTarget: ShareTarget): boolean {
    return !!this.selectedShareTarget && !!shareTarget &&
        tokensEqual(this.selectedShareTarget.id, shareTarget.id);
  }

  private onShareTargetDiscovered_(shareTarget: ShareTarget) {
    assert(this.shareTargetMap_);
    if (shareTarget.forSelfShare) {
      this.updateShareTargetList_(
          this.selfShareTargets_, 'selfShareTargets_', shareTarget);
    } else {
      this.updateShareTargetList_(
          this.nonSelfShareTargets_, 'nonSelfShareTargets_', shareTarget);
    }
    this.shareTargets_ =
        this.selfShareTargets_.concat(this.nonSelfShareTargets_);
    this.shareTargetMap_.set(tokenToString(shareTarget.id), shareTarget);
  }

  private updateShareTargetList_(
      shareTargetList: ShareTarget[], shareTargetListString: string,
      shareTarget: ShareTarget) {
    assert(this.shareTargetMap_);
    if (this.shareTargetMap_.has(tokenToString(shareTarget.id))) {
      const index = shareTargetList.findIndex(
          (target) => tokensEqual(target.id, shareTarget.id));
      assert(index !== -1);
      this.splice(shareTargetListString, index, 1, shareTarget);
      this.updateSelectedShareTarget_(shareTarget.id, shareTarget);
    } else {
      this.push(shareTargetListString, shareTarget);
    }
  }

  private onShareTargetLost_(shareTarget: ShareTarget) {
    if (shareTarget.forSelfShare) {
      // Remove target from `selfShareTargets_`.
      const index = this.selfShareTargets_.findIndex(
          (target) => tokensEqual(target.id, shareTarget.id));
      assert(index !== -1);
      this.splice('selfShareTargets_', index, 1);
    } else {
      // Remove target from `nonSelfShareTargets_`.
      const index = this.nonSelfShareTargets_.findIndex(
          (target) => tokensEqual(target.id, shareTarget.id));
      assert(index !== -1);
      this.splice('nonSelfShareTargets_', index, 1);
    }

    this.set(
        'shareTargets_',
        this.selfShareTargets_.concat(this.nonSelfShareTargets_));

    assert(this.shareTargetMap_);
    this.shareTargetMap_.delete(tokenToString(shareTarget.id));
    this.updateSelectedShareTarget_(shareTarget.id, /*shareTarget=*/ null);
  }

  private onNext_() {
    if (this.selectedShareTarget) {
      this.selectShareTarget_(this.selectedShareTarget);
    }
  }

  /**
   * Select the given share target and proceed to the confirmation page.
   */
  private selectShareTarget_(shareTarget: ShareTarget) {
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
          {bubbles: true, composed: true, detail: {page: 'confirmation'}}));
    });
  }

  private onSelectedShareTargetChanged_() {
    const deviceList = this.shadowRoot!.querySelector('#deviceList');
    if (!deviceList) {
      // deviceList is in dom-if and may not be found
      return;
    }

    const selector = this.shadowRoot!.querySelector<ArraySelector>('.selector');
    assert(selector);
    this.selectedShareTarget = selector.selectedItem as (ShareTarget | null);
  }

  private isShareTargetSelectedToString_(shareTarget: ShareTarget): string {
    return this.isShareTargetSelected_(shareTarget).toString();
  }

  private isShareTargetsEmpty_(): boolean {
    return this.shareTargets_.length === 0;
  }

  /**
   * Updates the selected share target to |shareTarget| if its id matches |id|.
   */
  private updateSelectedShareTarget_(
      id: UnguessableToken, shareTarget: ShareTarget|null) {
    if (this.selectedShareTarget &&
        tokensEqual(this.selectedShareTarget.id, id)) {
      this.selectedShareTarget = shareTarget;
      const selector =
          this.shadowRoot!.querySelector<ArraySelector>('.selector');
      assert(selector);
      selector.select(this.selectedShareTarget);
    }
  }

  /**
   * If the shareTarget is the first in the list, it's tab index should be 0
   * if there is no current selected share target. Otherwise, the tab index
   * should be 0 if it is the current selected share target. Tab index of 0
   *  allows users to navigate to it with tabs, and others should be -1
   * so users will not navigate to by tab.
   */
  private getTabIndexOfShareTarget_(shareTarget: ShareTarget|null): string {
    if (this.selectedShareTarget && shareTarget === this.selectedShareTarget) {
      return '0';
    }

    if (this.selfShareTargets_.length !== 0 &&
        shareTarget === this.selfShareTargets_[0]) {
      return '0';
    }

    if (this.nonSelfShareTargets_.length !== 0 &&
        shareTarget === this.nonSelfShareTargets_[0]) {
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
   */
  private getAriaLabelledHelpText_(): TrustedHTML {
    const tempEl = document.createElement('div');
    const localizedString = this.i18nAdvanced('nearbyShareDiscoveryPageInfo');
    const linkUrl = this.i18n('nearbyShareLearnMoreLink');
    tempEl.innerHTML = localizedString;

    const ariaLabelledByIds: string[] = [];
    tempEl.childNodes.forEach((node, index) => {
      // Text nodes should be aria-hidden and associated with an element id
      // that the anchor element can be aria-labelledby.
      if (node.nodeType === Node.TEXT_NODE) {
        const spanNode = document.createElement('span');
        spanNode.textContent = node.textContent;
        spanNode.id = `helpText${index}`;
        ariaLabelledByIds.push(spanNode.id);
        spanNode.setAttribute('aria-hidden', 'true');
        node.replaceWith(spanNode);
        return;
      }
      // The single element node with anchor tags should also be aria-labelledby
      // itself in-order with respect to the entire string.
      if (node.nodeType === Node.ELEMENT_NODE && node.nodeName === 'A') {
        const n = node as HTMLElement;
        n.id = `helpLink`;
        ariaLabelledByIds.push(n.id);
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

    return sanitizeInnerHtml(
        tempEl.innerHTML, {attrs: ['id', 'aria-hidden', 'aria-labelledby']});
  }

  /**
   * Returns the URL for the asset that defines the discovery page's
   * pulsing background animation
   */
  private getAnimationUrl_(): string {
    return PULSE_ANIMATION_URL;
  }
}

customElements.define(
    NearbyDiscoveryPageElement.is, NearbyDiscoveryPageElement);
