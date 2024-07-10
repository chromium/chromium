// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview guest tos screen implementation.
 */

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/oobe_icons.html.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';

import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {ContentType, WebViewHelper} from '../../components/web_view_helper.js';
import {WebViewLoader} from '../../components/web_view_loader.js';

import {getTemplate} from './guest_tos.html.js';


// Enum that describes the current state of the Guest ToS screen
enum GuestTosScreenState {
  LOADING = 'loading',
  OVERVIEW = 'overview',
  GOOGLE_EULA = 'google-eula',
  CROS_EULA = 'cros-eula',
}

/**
 * URL to use when online page is not available.
 */
const GUEST_TOS_EULA_TERMS_URL = 'chrome://terms';

/**
 * Timeout to load online ToS.
 */
const GUEST_TOS_ONLINE_LOAD_TIMEOUT_IN_MS = 10000;

const GuestTosScreenElementBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));


/**
 * Data that is passed to the screen during onBeforeShow.
 */
interface GuestTosScreenData {
  googleEulaUrl: string;
  crosEulaUrl: string;
}

export class GuestTos extends GuestTosScreenElementBase {
  static get is() {
    return 'guest-tos-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      usageChecked: {
        type: Boolean,
        value: true,
      },
    };
  }

  private usageChecked: boolean;
  private googleEulaUrl: string;
  private crosEulaUrl: string;

  constructor() {
    super();

    // Online URLs
    this.googleEulaUrl = '';
    this.crosEulaUrl = '';
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): GuestTosScreenState {
    return GuestTosScreenState.OVERVIEW;
  }

  override get UI_STEPS() {
    return GuestTosScreenState;
  }
  // clang-format on

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('GuestTosScreen');
    this.updateLocalizedContent();
  }

  /**
   * @param data Screen init payload.
   */
  override onBeforeShow(data: GuestTosScreenData): void {
    super.onBeforeShow(data);
    this.googleEulaUrl = data['googleEulaUrl'];
    this.crosEulaUrl = data['crosEulaUrl'];

    // Call updateLocalizedContent() to ensure that the listeners of the click
    // events on the ToS links are added.
    this.updateLocalizedContent();
  }

  /** Initial UI State for screen */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.HIDDEN;
  }

  override updateLocalizedContent(): void {
    const googleEulaLink =
        this.shadowRoot?.querySelector<HTMLAnchorElement>('#googleEulaLink');
    assert(googleEulaLink);
    googleEulaLink.addEventListener(
        'click', () => this.onGoogleEulaLinkClick());

    const crosEulaLink =
        this.shadowRoot?.querySelector<HTMLAnchorElement>('#crosEulaLink');
    assert(crosEulaLink);
    crosEulaLink.addEventListener('click', () => this.onCrosEulaLinkClick());
  }

  private showGoogleEula(): void {
    this.setUIStep(GuestTosScreenState.LOADING);
    const guestTosGoogleEulaWebview =
        this.shadowRoot?.querySelector<chrome.webviewTag.WebView>(
            '#guestTosGoogleEulaWebview');
    assert(guestTosGoogleEulaWebview);
    this.loadEulaWebview(
        guestTosGoogleEulaWebview, this.googleEulaUrl,
        false /* clear_anchors */);
  }

  private loadEulaWebview(
      webview: chrome.webviewTag.WebView, onlineTosUrl: string,
      clearAnchors: boolean): void {
    const loadFailureCallback = () => {
      WebViewHelper.loadUrlContentToWebView(
          webview, GUEST_TOS_EULA_TERMS_URL, ContentType.HTML);
    };

    const tosLoader = new WebViewLoader(
        webview, GUEST_TOS_ONLINE_LOAD_TIMEOUT_IN_MS, loadFailureCallback,
        clearAnchors, true /* inject_css */);
    tosLoader.setUrl(onlineTosUrl);
  }

  private onGoogleEulaContentLoad(): void {
    this.setUIStep(GuestTosScreenState.GOOGLE_EULA);
  }

  private showCrosEula(): void {
    this.setUIStep(GuestTosScreenState.LOADING);
    const guestTosCrosEulaWebview =
        this.shadowRoot?.querySelector<chrome.webviewTag.WebView>(
            '#guestTosCrosEulaWebview');
    assert(guestTosCrosEulaWebview);
    this.loadEulaWebview(
        guestTosCrosEulaWebview, this.crosEulaUrl, true /* clear_anchors */);
  }

  private onCrosEulaContentLoad(): void {
    this.setUIStep(GuestTosScreenState.CROS_EULA);
  }

  private getTerms(locale: string): TrustedHTML {
    const terms = document.createElement('div');
    terms.innerHTML =
        this.i18nAdvancedDynamic(locale, 'guestTosTerms', {attrs: ['id']});

    const googleEulaLink = terms.querySelector('#googleEulaLink');
    assert(googleEulaLink);
    googleEulaLink.setAttribute('is', 'action-link');
    googleEulaLink.classList.add('oobe-local-link');

    const crosEulaLink = terms.querySelector('#crosEulaLink');
    assert(crosEulaLink);
    crosEulaLink.setAttribute('is', 'action-link');
    crosEulaLink.classList.add('oobe-local-link');

    return sanitizeInnerHtml(
        terms.innerHTML, {tags: ['a'], attrs: ['id', 'is', 'class']});
  }

  private getUsageLearnMoreText(locale: string): TrustedHTML {
    return this.i18nAdvancedDynamic(locale, 'guestTosUsageOptInLearnMore');
  }

  private onGoogleEulaLinkClick(): void {
    this.showGoogleEula();
  }

  private onCrosEulaLinkClick(): void {
    this.showCrosEula();
  }

  private onUsageLearnMoreClick(): void {
    const usageLearnMorePopUp =
        this.shadowRoot?.querySelector<OobeModalDialog>('#usageLearnMorePopUp');
    if (usageLearnMorePopUp instanceof OobeModalDialog) {
      usageLearnMorePopUp.showDialog();
    }
  }

  private onTermsStepOkClick(): void {
    this.setUIStep(GuestTosScreenState.OVERVIEW);
  }

  private onAcceptClick(): void {
    this.userActed(['guest-tos-accept', this.usageChecked]);
  }

  private onBackClick(): void {
    this.userActed('back-button');
  }

  private cancel(): void {
    this.userActed('cancel');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [GuestTos.is]: GuestTos;
  }
}

customElements.define(GuestTos.is, GuestTos);
