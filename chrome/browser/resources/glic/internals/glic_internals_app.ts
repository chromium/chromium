// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_tabs/cr_tabs.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {ActuationEligibility, ActuationTarget, AllowedInflightNavigation, FeatureMode, FreOverride, InvocationSource} from '../glic.mojom-webui.js';
import {InternalsPageHandlerFactory, InternalsPageHandlerRemote} from '../glic_internals.mojom-webui.js';
import type {InternalsDataPayload, TriggerInvokeFromInternalsOptions} from '../glic_internals.mojom-webui.js';

import {getCss} from './glic_internals_app.css.js';
import {getHtml} from './glic_internals_app.html.js';


export class GlicInternalsAppElement extends CrLitElement {
  static get is() {
    return 'glic-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      data_: {type: Object},
      invokePrompt_: {type: String},
      invokeAutoSubmit_: {type: Boolean},
      invokeFreOverride_: {type: Number},
      invokeFeatureMode_: {type: Number},
      invokeInvocationSource_: {type: Number},
      invokeWaitForPanelOpen_: {type: Boolean},
      invokeLogs_: {type: Array},
      invokeSurfaceType_: {type: String},
      invokeZssOverride_: {type: Boolean},
      invokeZssAdditionalContent_: {type: String},
      invokeOpenInForeground_: {type: Boolean},
      invokeActuationTarget_: {type: Number},
      actuationTargetEnumValues_: {type: Array},
      invokeShowPanel_: {type: Boolean},

      selectedTabIndex_: {type: Number},
      tabNames_: {type: Array},
      featureModeEnumValues_: {type: Array},
    };
  }

  protected accessor data_: InternalsDataPayload|undefined;
  protected accessor invokePrompt_: string = '';
  protected accessor invokeAutoSubmit_: boolean = true;
  protected accessor invokeFreOverride_: FreOverride = FreOverride.kUnspecified;
  protected accessor invokeFeatureMode_: FeatureMode = FeatureMode.kUnspecified;
  protected accessor invokeInvocationSource_: InvocationSource =
      InvocationSource.kOsButton;
  protected accessor invokeWaitForPanelOpen_: boolean = false;
  protected accessor invokeLogs_: string[] = [];
  protected accessor invokeSurfaceType_: string = 'default';
  protected accessor invokeZssOverride_: boolean = false;
  protected accessor invokeZssAdditionalContent_: string = '';
  protected accessor invokeOpenInForeground_: boolean = true;
  protected accessor invokeActuationTarget_: ActuationTarget =
      ActuationTarget.kAgentDecides;
  protected accessor invokeShowPanel_: boolean = true;

  protected accessor selectedTabIndex_: number = 0;
  protected accessor tabNames_: string[] = ['General', 'Debug Controls'];
  protected accessor featureModeEnumValues_:
      Array<{name: string, value: number}> =
          Object.entries(FeatureMode)
              .filter(([key]) => isNaN(Number(key)))
              .map(([name, value]) => ({name, value: value as number}));
  protected accessor actuationTargetEnumValues_:
      Array<{name: string, value: number}> =
          Object.entries(ActuationTarget)
              .filter(([key]) => isNaN(Number(key)))
              .map(([name, value]) => ({name, value: value as number}));



  private pageHandler_ = new InternalsPageHandlerRemote();

  override connectedCallback() {
    super.connectedCallback();
    InternalsPageHandlerFactory.getRemote().createInternalsPageHandler(
        this.pageHandler_.$.bindNewPipeAndPassReceiver());

    this.pageHandler_.getInternalsDataPayload().then(
        ({internalsData}: {internalsData: InternalsDataPayload}) => {
          this.data_ = internalsData;
        });
  }

  protected onShowErrorAllowedChange(e: Event) {
    const allowed = (e.target as HTMLInputElement).checked;
    this.data_!.showErrorAllowed = allowed;
    this.pageHandler_.setShowErrorAllowed(allowed);
  }

  protected onExperimentalOptInClick_() {
    this.pageHandler_.showExperimentalOptIn();
  }

  protected onAutopushInputChange(e: Event) {
    this.data_!.config.autopushGuestUrl = (e.target as HTMLInputElement).value;
  }

  protected onStagingInputChange(e: Event) {
    this.data_!.config.stagingGuestUrl = (e.target as HTMLInputElement).value;
  }

  protected onPreprodInputChange(e: Event) {
    this.data_!.config.preprodGuestUrl = (e.target as HTMLInputElement).value;
  }

  protected onProdInputChange(e: Event) {
    this.data_!.config.prodGuestUrl = (e.target as HTMLInputElement).value;
  }

  protected onSavePresetsClick_() {
    const errorMsg =
        this.shadowRoot.querySelector<HTMLDivElement>('#inputErrorMsg');

    try {
      // Validate the URL. If we don't validate here, IPC will kill this
      // renderer on invalid URLs.
      new URL(this.data_!.config.autopushGuestUrl);
      new URL(this.data_!.config.stagingGuestUrl);
      new URL(this.data_!.config.preprodGuestUrl);
      new URL(this.data_!.config.prodGuestUrl);
    } catch {
      console.error('Invalid URL: no-op');
      errorMsg!.classList.remove('hiddenElement');
      return;
    }
    errorMsg!.classList.add('hiddenElement');
    this.pageHandler_.setGuestUrlPresets(
        this.data_!.config.autopushGuestUrl, this.data_!.config.stagingGuestUrl,
        this.data_!.config.preprodGuestUrl, this.data_!.config.prodGuestUrl);
  }

  protected onWebContinuityInputChange(e: Event) {
    this.data_!.config.webContinuityOriginatingHostUrl =
        (e.target as HTMLInputElement).value;
  }

  protected onSaveWebContinuityPresetClick_() {
    const errorMsg = this.shadowRoot.querySelector<HTMLDivElement>(
        '#webContinuityInputErrorMsg');
    const url = this.data_!.config.webContinuityOriginatingHostUrl;

    // Validate the URL. If we don't validate here, IPC will kill this
    // renderer on invalid URLs.
    if (url && URL.parse(url) === null) {
      console.error('Invalid URL: no-op');
      errorMsg!.classList.remove('hiddenElement');
      return;
    }
    errorMsg!.classList.add('hiddenElement');
    this.pageHandler_.setWebContinuityOriginatingHostUrlPreset(url);
  }

  protected getActuationEligibilityString_(eligibility: ActuationEligibility):
      string {
    switch (eligibility) {
      case ActuationEligibility.kEligible:
        return 'Eligible';
      case ActuationEligibility.kMissingAccountCapability:
        return 'Missing account capability';
      case ActuationEligibility.kMissingChromeBenefits:
        return 'Missing Chrome benefits';
      case ActuationEligibility.kDisabledByPolicy:
        return 'Disabled by policy';
      case ActuationEligibility.kPlatformUnsupported:
        return 'Platform unsupported';
      case ActuationEligibility.kEnterpriseWithoutManagement:
        return 'Enterprise account without management. Default pref disabled.';
      default:
        return 'unknown';
    }
  }

  protected getTableData_(): Array<{label: string, value: boolean}> {
    if (!this.data_ || !this.data_.enablement) {
      return [];
    }

    return [
      {
        label: 'Enabled by Chrome Flags',
        value: !this.data_.enablement.featureDisabled,
      },
      {
        label: 'Regular profile',
        value: !this.data_.enablement.notRegularProfile,
      },
      {
        label: 'Pref or flag based rollout (flag or pref) applies',
        value: !this.data_.enablement.notRolledOut,
      },
      {
        label: 'Account exists and has the Gemini in Chrome capability',
        value: !this.data_.enablement.primaryAccountNotCapable,
      },
      {
        label: 'Account exists and is fully signed-in',
        value: !this.data_.enablement.primaryAccountNotFullySignedIn,
      },
      {
        label:
            'Chrome Enterprise policy allows this feature (or doesn\'t apply)',
        value: !this.data_.enablement.disallowedByChromePolicy,
      },
      {
        label: 'Server side admin allows this feature',
        value: !this.data_.enablement.disallowedByRemoteAdmin,
      },
      {
        label: 'Server side allows this feature (Not admin policy)',
        value: !this.data_.enablement.disallowedByRemoteOther,
      },
      {
        label: 'User did pass the FRE',
        value: !this.data_.enablement.notConsented,
      },
      {
        label: 'User accepted actuation consent',
        value: !this.data_.enablement.actuationNotConsented,
      },
      {
        label: 'Passed country filter',
        value: !this.data_.enablement.disallowedByCountryFilter,
      },
      {
        label: 'Passed locale filter',
        value: !this.data_.enablement.disallowedByLocaleFilter,
      },
    ];
  }

  protected getInvocationSourceOptions_() {
    return Object.entries(InvocationSource)
        .filter(([_, value]) => typeof value === 'number')
        .map(([key, value]) => ({name: key, value: value}));
  }

  protected onInvokePromptInput_(e: Event) {
    this.invokePrompt_ = (e.target as HTMLInputElement).value;
  }

  protected onInvokeAutoSubmitChange_(e: Event) {
    this.invokeAutoSubmit_ = (e.target as HTMLInputElement).checked;
  }

  protected onInvokeFreOverrideChange_(e: Event) {
    this.invokeFreOverride_ = Number((e.target as HTMLSelectElement).value);
  }

  protected onInvokeFeatureModeChange_(e: Event) {
    this.invokeFeatureMode_ = Number((e.target as HTMLSelectElement).value);
  }

  protected onInvokeInvocationSourceChange_(e: Event) {
    this.invokeInvocationSource_ =
        Number((e.target as HTMLSelectElement).value);
  }
  protected onInvokeWaitForPanelOpenChange_(e: Event) {
    this.invokeWaitForPanelOpen_ = (e.target as HTMLInputElement).checked;
  }

  protected onInvokeSurfaceTypeChange_(e: Event) {
    this.invokeSurfaceType_ = (e.target as HTMLSelectElement).value;
  }

  protected onInvokeZssOverrideChange_(e: Event) {
    this.invokeZssOverride_ = (e.target as HTMLInputElement).checked;
  }

  protected onInvokeZssAdditionalContentInput_(e: Event) {
    this.invokeZssAdditionalContent_ = (e.target as HTMLInputElement).value;
  }

  protected onInvokeOpenInForegroundChange(e: Event) {
    this.invokeOpenInForeground_ = (e.target as HTMLInputElement).checked;
  }

  protected onInvokeActuationTargetChange_(e: Event) {
    this.invokeActuationTarget_ = Number((e.target as HTMLSelectElement).value);
  }

  protected onInvokeShowPanelChange_(e: Event) {
    this.invokeShowPanel_ = (e.target as HTMLInputElement).checked;
  }
  protected onTriggerInvokeClick_() {
    this.invokeLogs_ =
        [`[${new Date().toLocaleTimeString()}] TRIGGERING INVOKE...`];
    console.info(this.invokeLogs_[0]);

    const surface = this.invokeSurfaceType_ === 'newTab' ?
        {newTab: {openInForeground: this.invokeOpenInForeground_}} :
        {defaultSurface: {}};

    const options: TriggerInvokeFromInternalsOptions = {
      invocationSource: this.invokeInvocationSource_,
      prompts: this.invokePrompt_ ? [this.invokePrompt_] : [],
      additionalContext: null,
      conversation: {defaultConversation: {}},
      featureMode: this.invokeFeatureMode_,
      disableZss: false,
      zssConfig: this.invokeZssOverride_ ?
          {additionalContent: this.invokeZssAdditionalContent_ || null} :
          null,
      skillId: null,
      errorMessage: null,
      timeout: null,
      allowedInflightNavigation: AllowedInflightNavigation.kNone,
      autoSubmit: this.invokeAutoSubmit_,
      freOverride: this.invokeFreOverride_,
      waitForPanelOpen: this.invokeWaitForPanelOpen_,
      surface: surface,
      actuationTarget: this.invokeActuationTarget_,
      showPanel: this.invokeAutoSubmit_ ? this.invokeShowPanel_ : null,
    };

    this.pageHandler_.triggerInvokeFromInternalsAction(options).then(
        ({success, errorMessage}: {success: boolean, errorMessage: string}) => {
          const timestamp = new Date().toLocaleTimeString();
          const logEntry = `[${timestamp}] ${
              success ? 'SUCCESS' : 'ERROR: ' + errorMessage}`;
          this.invokeLogs_ = [...this.invokeLogs_, logEntry];
          console.info(logEntry);
        });
  }

  protected onSelectedTabIndexSelectedChanged_(
      e: CustomEvent<{value: number}>) {
    this.selectedTabIndex_ = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'glic-internals-app': GlicInternalsAppElement;
  }
}

customElements.define(GlicInternalsAppElement.is, GlicInternalsAppElement);
