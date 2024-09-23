// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/nearby_onboarding_one_page.js';
import '/shared/nearby_onboarding_page.js';
import '/shared/nearby_visibility_page.js';
import './nearby_confirmation_page.js';
import './nearby_discovery_page.js';
import 'chrome://resources/ash/common/cr_elements/cr_view_manager/cr_view_manager.js';

import type {ConfirmationManagerInterface, PayloadPreview, ShareTarget, TransferUpdateListenerPendingReceiver} from '/shared/nearby_share.mojom-webui.js';
import {NearbyShareSettingsMixin} from '/shared/nearby_share_settings_mixin.js';
import {CloseReason} from '/shared/types.js';
import type {CrViewManagerElement} from 'chrome://resources/ash/common/cr_elements/cr_view_manager/cr_view_manager.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

/**
 * @fileoverview The 'nearby-share' component is the entry point for the Nearby
 * Share flow. It is used as a standalone dialog via chrome://nearby and as part
 * of the ChromeOS share sheet.
 */

enum Page {
  CONFIRMATION = 'confirmation',
  DISCOVERY = 'discovery',
  ONBOARDING = 'onboarding',
  ONEPAGE_ONBOARDING = 'onboarding-one',
  VISIBILITY = 'visibility',
}

const NearbyShareAppElementBase = NearbyShareSettingsMixin(PolymerElement);

export interface NearbyShareAppElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

export class NearbyShareAppElement extends NearbyShareAppElementBase {
  static get is() {
    return 'nearby-share-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Mirroring the enum so that it can be used from HTML bindings. */
      Page: {
        type: Object,
        value: Page,
      },

      /**
       * Set by the nearby-discovery-page component when switching to the
       * nearby-confirmation-page.
       */
      confirmationManager_: {
        type: Object,
        value: null,
      },

      /**
       * Set by the nearby-discovery-page component when switching to the
       * nearby-confirmation-page.
       */
      transferUpdateListener_: {
        type: Object,
        value: null,
      },

      /**
       * The currently selected share target set by the nearby-discovery-page
       * component when the user selects a device.
       */
      selectedShareTarget_: {
        type: Object,
        value: null,
      },

      /**
       * Preview info of attachment to be sent, set by the
       * nearby-discovery-page.
       */
      payloadPreview_: {
        type: Object,
        value: null,
      },
    };
  }

  private confirmationManager_: ConfirmationManagerInterface|null;
  private transferUpdateListener_: TransferUpdateListenerPendingReceiver|null;
  private selectedShareTarget_: ShareTarget|null;
  private payloadPreview_: PayloadPreview|null;

  override ready() {
    super.ready();

    this.addEventListener(
        'change-page', e => this.onChangePage_(e as CustomEvent<{page: Page}>));
    this.addEventListener(
        'close', e => this.onClose_(e as CustomEvent<{reason: CloseReason}>));
    this.addEventListener('onboarding-complete', this.onOnboardingComplete_);

    ColorChangeUpdater.forDocument().start();
  }

  /**
   * Called whenever view changes.
   * ChromeVox screen reader requires focus on #pageContainer to read
   * dialog.
   */
  private focusOnPageContainer_(page: string) {
    this.shadowRoot!.querySelector(`nearby-${
        page}-page`)!.shadowRoot!.querySelector('nearby-page-template')!
        .shadowRoot!.querySelector<HTMLElement>('#pageContainer')!.focus();
  }

  /**
   * Determines if the feature flag for One-page onboarding workflow is enabled.
   * @return Whether the one-page onboarding is enabled
   */
  private isOnePageOnboardingEnabled_(): boolean {
    return loadTimeData.getBoolean('isOnePageOnboardingEnabled');
  }

  /**
   * Called when component is attached and all settings values have been
   * retrieved.
   */
  override onSettingsRetrieved() {
    if (this.settings.isOnboardingComplete) {
      if (!this.settings.enabled) {
        // When a new share is triggered, if the user has completed onboarding
        // previously, then silently enable the feature and continue to
        // discovery page directly.
        this.set('settings.enabled', true);
      }
      this.$.viewManager.switchView(Page.DISCOVERY);
      this.focusOnPageContainer_(Page.DISCOVERY);

      return;
    }

    const onboardingPage = this.isOnePageOnboardingEnabled_() ?
        Page.ONEPAGE_ONBOARDING :
        Page.ONBOARDING;
    this.$.viewManager.switchView(onboardingPage);
    this.focusOnPageContainer_(onboardingPage);
  }

  /**
   * Handler for the change-page event.
   */
  private onChangePage_(event: CustomEvent<{page: Page}>) {
    this.$.viewManager.switchView(event.detail.page);
    this.focusOnPageContainer_(event.detail.page);
  }

  /**
   * Handler for the close event.
   */
  private onClose_(event: CustomEvent<{reason: CloseReason}>) {
    // TODO(b/237796007): Handle the case of null |event.detail|
    const reason =
        event.detail.reason == null ? CloseReason.UNKNOWN : event.detail.reason;
    chrome.send('close', [reason]);
  }

  /**
   * Handler for when onboarding is completed.
   */
  private onOnboardingComplete_() {
    this.$.viewManager.switchView(Page.DISCOVERY);
    this.focusOnPageContainer_(Page.DISCOVERY);
  }
}

customElements.define(NearbyShareAppElement.is, NearbyShareAppElement);
