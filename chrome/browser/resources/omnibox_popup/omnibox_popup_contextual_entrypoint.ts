// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox_lens_search.js';
import '//resources/cr_components/composebox/current_tab_chip.js';
import './omnibox_popup_contextual_entrypoint_button.js';

import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {TabAttachmentSource} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {InputType} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {browserProxyFactory} from './omnibox_popup.mojom-webui.js';
import type {BrowserProxy} from './omnibox_popup.mojom-webui.js';
import {getCss} from './omnibox_popup_contextual_entrypoint.css.js';
import {getHtml} from './omnibox_popup_contextual_entrypoint.html.js';
import type {OmniboxPopupContextualEntrypointButtonElement} from './omnibox_popup_contextual_entrypoint_button.js';

export class OmniboxPopupContextualEntrypointElement extends CrLitElement {
  static get is() {
    return 'omnibox-popup-contextual-entrypoint';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      dropdownIsVisible: {type: Boolean},
      inputState: {type: Object},
      isLensSearchEligible: {type: Boolean},
      isLensIconEligible: {type: Boolean},
      isContentSharingEnabled: {type: Boolean},
      isAimPopupEligible: {type: Boolean},
      searchboxLayoutMode_: {
        type: String,
        reflect: true,
      },
      showContextEntrypoint_: {
        type: Boolean,
        reflect: true,
      },
      isLensChipShown_: {type: Boolean},
      isLensIconShown_: {type: Boolean},
      currentTabForChip_: {type: Object},
      isCurrentTabChipShown_: {type: Boolean},
      applyContextButtonBackground_: {type: Boolean},
      usePecApi_: {type: Boolean},
      isOblongShape_: {type: Boolean},
      showContextButtonSuggestionLabel_: {type: Boolean},
    };
  }

  accessor dropdownIsVisible: boolean = true;
  accessor inputState: InputState|null = null;
  accessor isLensSearchEligible: boolean = false;
  accessor isLensIconEligible: boolean = false;
  accessor isContentSharingEnabled: boolean = false;
  accessor isAimPopupEligible: boolean = false;

  protected accessor searchboxLayoutMode_: string =
      loadTimeData.getString('searchboxLayoutMode');
  protected accessor showContextEntrypoint_: boolean = false;
  protected accessor isLensChipShown_: boolean = false;
  protected accessor isLensIconShown_: boolean = false;
  protected accessor currentTabForChip_: TabInfo|null = null;
  protected accessor isCurrentTabChipShown_: boolean = false;
  protected accessor applyContextButtonBackground_: boolean = false;
  protected accessor usePecApi_: boolean =
      loadTimeData.getBoolean('contextualMenuUsePecApi');
  protected accessor isOblongShape_: boolean =
      loadTimeData.getBoolean('contextButtonShapeIsOblong');
  protected accessor showContextButtonSuggestionLabel_: boolean =
      loadTimeData.getBoolean('omniboxShowContextButtonSuggestionLabel');

  private isLensSearchEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowLensSearchChip');
  private isLensIconEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowLensIcon');
  private hideContextButton_: boolean =
      loadTimeData.getBoolean('hideClassicContextButton');
  private isCurrentTabChipEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowCurrentTabChip');
  private contextButtonHasBackground_: boolean =
      loadTimeData.getBoolean('contextButtonHasBackground');

  private browserProxy_: BrowserProxy;
  private searchboxBrowserProxy_: SearchboxBrowserProxy;
  private popupListenerIds_: number[] = [];
  private searchboxListenerIds_: number[] = [];

  get showContextEntrypoint(): boolean {
    return this.showContextEntrypoint_ && !this.shouldHideEntrypointButton_();
  }

  constructor() {
    super();
    this.searchboxBrowserProxy_ = SearchboxBrowserProxy.getInstance();
    this.browserProxy_ = browserProxyFactory.getInstance();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.popupListenerIds_ = [
      this.browserProxy_.callbackRouter.onShow.addListener(
          this.onShow_.bind(this)),
    ];

    const callbackRouter = this.searchboxBrowserProxy_.callbackRouter;
    this.searchboxListenerIds_ = [
      callbackRouter.updateLensSearchEligibility.addListener(
          (eligible: boolean) => {
            this.isLensSearchEligible = this.isLensSearchEnabled_ && eligible;
            this.isLensIconEligible = this.isLensIconEnabled_ && eligible;
          }),
      callbackRouter.updateContentSharingPolicy.addListener(
          (enabled: boolean) => {
            this.isContentSharingEnabled = enabled;
          }),
      callbackRouter.onInputStateChanged.addListener(
          (inputState: InputState) => {
            this.inputState = inputState;
          }),
    ];
    if (!this.hideContextButton_) {
      this.searchboxListenerIds_.push(
          callbackRouter.updateAimPopupEligibility.addListener(
              (eligible: boolean) => {
                this.isAimPopupEligible = eligible;
              }));
    }

    // Fetch the initial input state async, but only apply it if
    // `onInputStateChanged` push updates haven't already populated it. This
    // prevents a race condition where a late-resolving `getInputState` response
    // could clobber newer state received via push notifications.
    this.searchboxBrowserProxy_.handler.getInputState().then(({state}) => {
      if (this.inputState === null && state) {
        this.inputState = state;
      }
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    for (const listenerId of this.popupListenerIds_) {
      this.browserProxy_.callbackRouter.removeListener(listenerId);
    }
    this.popupListenerIds_ = [];
    for (const listenerId of this.searchboxListenerIds_) {
      this.searchboxBrowserProxy_.callbackRouter.removeListener(listenerId);
    }
    this.searchboxListenerIds_ = [];
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedProps = changedProperties as Map<PropertyKey, unknown>;
    if (changedProps.has('isAimPopupEligible') ||
        changedProps.has('searchboxLayoutMode_') ||
        changedProps.has('isLensSearchEligible') ||
        changedProps.has('dropdownIsVisible')) {
      this.showContextEntrypoint_ = this.computeShowContextEntrypoint_();
    }

    if (changedProps.has('isContentSharingEnabled') ||
        changedProps.has('isLensSearchEligible') ||
        changedProps.has('isLensIconEligible') ||
        changedProps.has('currentTabForChip_') ||
        changedProps.has('inputState')) {
      this.isCurrentTabChipShown_ = this.isContentSharingEnabled &&
          this.isLensSearchEligible && this.computeShowCurrentTabChip_();
      this.isLensIconShown_ =
          this.isContentSharingEnabled && this.isLensIconEligible;
      this.isLensChipShown_ = this.isContentSharingEnabled &&
          this.isLensSearchEligible && !this.isCurrentTabChipShown_;
      this.applyContextButtonBackground_ =
          this.contextButtonHasBackground_ && !this.isLensChipShown_;
    }
  }

  getContextEntrypointElement(): OmniboxPopupContextualEntrypointButtonElement
      |null {
    if (this.showContextEntrypoint) {
      return this.shadowRoot
                 ?.querySelector<OmniboxPopupContextualEntrypointButtonElement>(
                     '#context') ??
          null;
    }
    return null;
  }

  protected shouldHideEntrypointButton_(): boolean {
    return this.searchboxLayoutMode_ === 'Compact';
  }

  private computeShowContextEntrypoint_(): boolean {
    if (!this.dropdownIsVisible || this.hideContextButton_ ||
        !this.isAimPopupEligible) {
      return false;
    }

    if (this.searchboxLayoutMode_.startsWith('Tall')) {
      return true;
    }

    if (this.searchboxLayoutMode_ === 'Compact') {
      return this.isLensSearchEligible;
    }

    return false;
  }

  protected onLensSearchClick_() {
    this.searchboxBrowserProxy_.handler.openLensSearch();
  }

  protected async refreshCurrentTabForChip_() {
    // TODO (b/537859769) - Replace getRecentTabs with a dedicated handler for
    // current tab chip.
    const {tabs} = await this.searchboxBrowserProxy_.handler.getRecentTabs();
    this.currentTabForChip_ =
        tabs.find(tab => tab.showInCurrentTabChip) || null;
  }

  protected onAddTabContext_(e: CustomEvent<{
    id: number,
    title: string,
    url: Url,
  }>) {
    this.searchboxBrowserProxy_.handler.addTabContext(
        e.detail.id, /*delayUpload=*/ false,
        TabAttachmentSource.kCurrentTabChip);
  }

  protected computeShowCurrentTabChip_() {
    const browserTabsAllowedByPecApi = !this.usePecApi_ ||
        (!!this.inputState &&
         this.inputState.allowedInputTypes.includes(InputType.kBrowserTab));
    return this.isCurrentTabChipEnabled_ && !!this.currentTabForChip_ &&
        browserTabsAllowedByPecApi;
  }

  private onShow_() {
    this.refreshCurrentTabForChip_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-popup-contextual-entrypoint':
        OmniboxPopupContextualEntrypointElement;
  }
}

customElements.define(
    OmniboxPopupContextualEntrypointElement.is,
    OmniboxPopupContextualEntrypointElement);
