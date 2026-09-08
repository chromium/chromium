// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_tabs/cr_tabs.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {ActuationTarget, FeatureMode, FormFactor, FreOverride, InvocationSource, Platform} from '../glic_enums.mojom-webui.js';
import {ActuationEligibility, browserProxyFactory, FreCompletionWaitMode, GlicExperimentalTriggeringState} from '../glic_internals.mojom-webui.js';
import type {BrowserProxy, InternalsDataPayload, TriggerInvokeFromInternalsOptions} from '../glic_internals.mojom-webui.js';

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
      invokeFocusOnShow_: {type: Boolean},
      invokeTimeoutMs_: {type: String},
      invokeLogs_: {type: Array},
      invokeSurfaceType_: {type: String},
      invokeZssOverride_: {type: Boolean},
      invokeZssAdditionalContent_: {type: String},
      invokeOpenInForeground_: {type: Boolean},
      invokeActuationTarget_: {type: Number},
      actuationTargetEnumValues_: {type: Array},
      invokeShowPanel_: {type: Boolean},
      invokePayloadUniversalCartMetadata_: {type: String},
      invokeFreCompletionWaitMode_: {type: Number},
      freCompletionWaitModeEnumValues_: {type: Array},
      invokeTakeScreenshot_: {type: Boolean},
      invokeSupersedeIfInProgress_: {type: Boolean},
      invokePublicKey_: {type: String},
      invokeAuthSecret_: {type: String},

      selectedTabIndex_: {type: Number},
      invokeConversationType_: {type: String},
      invokeConversationId_: {type: String},
      invokeSpecificTabIndex_: {type: Number},
      invokeSpecificTabsToShareIndices_: {type: Array},
      availableTabs_: {type: Array},
      tabNames_: {type: Array},
      featureModeEnumValues_: {type: Array},
      copyButtonText_: {type: String},
    };
  }

  protected accessor copyButtonText_: string = 'Copy Diagnostics';
  private copyResetTimeoutId_: number|null = null;

  protected accessor data_: InternalsDataPayload|undefined;
  protected accessor invokePrompt_: string = '';
  protected accessor invokeAutoSubmit_: boolean = true;
  protected accessor invokeFreOverride_: FreOverride = FreOverride.kUnspecified;
  protected accessor invokeFeatureMode_: FeatureMode = FeatureMode.kUnspecified;
  protected accessor invokeInvocationSource_: InvocationSource =
      InvocationSource.kOsButton;
  protected accessor invokeWaitForPanelOpen_: boolean = false;
  protected accessor invokeFocusOnShow_: boolean = true;
  protected accessor invokeTimeoutMs_: string = '';
  protected accessor invokeLogs_: string[] = [];
  protected accessor invokeSurfaceType_: string = 'default';
  protected accessor invokeZssOverride_: boolean = false;
  protected accessor invokeZssAdditionalContent_: string = '';
  protected accessor invokeOpenInForeground_: boolean = true;
  protected accessor invokeActuationTarget_: ActuationTarget =
      ActuationTarget.kAgentDecides;
  protected accessor invokeShowPanel_: boolean = true;
  protected accessor invokePayloadUniversalCartMetadata_: string = '';
  protected accessor invokeFreCompletionWaitMode_: FreCompletionWaitMode =
      FreCompletionWaitMode.kDefault;
  protected accessor invokeTakeScreenshot_: boolean = false;
  protected accessor invokeSupersedeIfInProgress_: boolean = false;
  protected accessor invokePublicKey_: string =
      'BFlvj1VrkwP8pxa1zSiJZzZ7yeMEO1DOPS' +
      'bNw6XV8NK3Xo++7ql9NTcxNaciYM2eQ/G1ebnwrtRrHyMXEDhN5ck=';
  protected accessor invokeAuthSecret_: string = 'aaaaaaaaaaaaaaaa';
  protected accessor invokeConversationType_: string = 'default';
  protected accessor invokeConversationId_: string = '';
  protected accessor invokeSpecificTabIndex_: number = 0;
  protected accessor invokeSpecificTabsToShareIndices_: number[] = [];
  protected accessor availableTabs_: string[] = [];

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
  protected accessor freCompletionWaitModeEnumValues_:
      Array<{name: string, value: number}> =
          Object.entries(FreCompletionWaitMode)
              .filter(([key]) => isNaN(Number(key)))
              .map(([name, value]) => ({name, value: value as number}));



  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.fetchInternalsData_();

    this.refreshOpenTabs_();
  }

  private fetchInternalsData_() {
    this.browserProxy_.handler.getInternalsDataPayload().then(
        ({internalsData}: {internalsData: InternalsDataPayload}) => {
          this.data_ = internalsData;
        });
  }

  protected onShowErrorAllowedChange(e: Event) {
    const allowed = (e.target as HTMLInputElement).checked;
    this.data_!.showErrorAllowed = allowed;
    this.browserProxy_.handler.setShowErrorAllowed(allowed);
  }

  protected onExperimentalOptInClick_() {
    this.browserProxy_.handler.showExperimentalOptIn();
  }

  protected isAllConsentMet_(): boolean {
    if (!this.data_?.enablement) {
      return false;
    }
    return this.data_.enablement.glicExperimentalTriggeringState ===
        GlicExperimentalTriggeringState.kReady &&
        this.data_.enablement.freIsConsented &&
        this.data_.enablement.actuationIsConsented;
  }

  protected onGlicConsentChange_(e: CustomEvent<{value: boolean}>) {
    if (!(e.target as HTMLInputElement).checked) {
      this.browserProxy_.handler.revokeGlicConsent();
      this.fetchInternalsData_();
    }
  }

  protected onActuationConsentChange_(e: CustomEvent<{value: boolean}>) {
    if (!(e.target as HTMLInputElement).checked) {
      this.browserProxy_.handler.revokeActuationConsent();
      this.fetchInternalsData_();
    }
  }

  protected onExperimentalConsentChange_(e: CustomEvent<{value: boolean}>) {
    if (!(e.target as HTMLInputElement).checked) {
      this.browserProxy_.handler.revokeExperimentalTriggeringConsent();
      this.fetchInternalsData_();
    }
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
    this.browserProxy_.handler.setGuestUrlPresets(
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
    this.browserProxy_.handler.setWebContinuityOriginatingHostUrlPreset(url);
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

  protected getExperimentalTriggeringStateString_(
      state: GlicExperimentalTriggeringState): string {
    return GlicExperimentalTriggeringState[state] || 'Unknown';
  }

  protected isActuationEligible_(): boolean {
    return this.data_?.enablement?.actuationEligibility ===
        ActuationEligibility.kEligible;
  }

  protected isGlicApiActuationEligible_(): boolean {
    return this.data_?.enablement?.glicApiActuationEligibility ===
        ActuationEligibility.kEligible;
  }

  protected computeOverallStatus_(): {enabled: boolean, reason?: string} {
    if (!this.data_ || !this.data_.enablement) {
      return {enabled: false, reason: 'Loading or unavailable'};
    }
    const e = this.data_.enablement;
    if (!e.featureEnabled) {
      return {enabled: false, reason: 'Disabled by Chrome feature flag'};
    }
    if (!e.isRegularProfile) {
      return {
        enabled: false,
        reason: 'Not a regular user profile (e.g. Incognito or Guest)',
      };
    }
    if (!e.allowedByCountryFilter) {
      return {enabled: false, reason: 'Blocked by country filter'};
    }
    if (!e.allowedByLocaleFilter) {
      return {enabled: false, reason: 'Blocked by UI locale filter'};
    }
    if (!e.allowedByChromePolicy) {
      return {enabled: false, reason: 'Disabled by enterprise Chrome policy'};
    }
    if (!e.allowedByRemoteAdmin) {
      return {enabled: false, reason: 'Disabled by server-side admin policy'};
    }
    if (!e.allowedByRemoteOther) {
      return {enabled: false, reason: 'Disabled by server-side configuration'};
    }
    if (!e.isRolledOut) {
      return {enabled: false, reason: 'Profile not in rollout group'};
    }
    if (!e.primaryAccountIsCapable) {
      return {
        enabled: false,
        reason: 'Primary account lacks Gemini capabilities',
      };
    }
    if (!e.primaryAccountIsFullySignedIn) {
      return {enabled: false, reason: 'Primary account not fully signed in'};
    }
    if (!e.freIsConsented) {
      return {
        enabled: false,
        reason: 'First Run Experience (FRE) consent not completed',
      };
    }
    return {enabled: true};
  }

  protected onRefreshClick_() {
    this.fetchInternalsData_();
    this.refreshOpenTabs_();
  }

  protected getChromeVersion_(): string {
    if (loadTimeData.isInitialized()) {
      try {
        const browserVersion = loadTimeData.getString('browserVersion');
        if (browserVersion) {
          return browserVersion;
        }
      } catch {
        // Fall back below if key is not found.
      }
      try {
        const version = loadTimeData.getString('version');
        if (version) {
          const modifier = loadTimeData.getString('version_modifier');
          const arch = loadTimeData.getString('version_processor_variation');
          return [version, modifier, arch].filter(Boolean).join(' ');
        }
      } catch {
        // Fall back below.
      }
    }
    const match = navigator.userAgent.match(/Chrome\/([0-9.]+)/);
    return match?.[1] ?? 'Unknown';
  }

  protected getDiagnosticsMarkdown_(): string {
    if (!this.data_) {
      return '# Glic Internals Diagnostics\n\nNo diagnostics data available.';
    }

    const lines: string[] = [];
    const overall = this.computeOverallStatus_();

    lines.push('# Glic Internals Diagnostics');
    lines.push('');
    lines.push(`- **Timestamp:** ${new Date().toISOString()}`);
    lines.push(`- **Chrome Version:** ${this.getChromeVersion_()}`);
    if (loadTimeData.isInitialized()) {
      try {
        const revision = loadTimeData.getString('cl');
        if (revision) {
          lines.push(`- **Revision:** ${revision}`);
        }
      } catch {
        // Ignore if key is not present.
      }
    }
    lines.push(`- **Overall Status:** ${
        overall.enabled ? 'Eligible & Enabled' : 'Blocked / Ineligible'}`);
    if (!overall.enabled && overall.reason) {
      lines.push(`- **Blocking Reason:** ${overall.reason}`);
    }
    if (this.data_.debugInfo) {
      lines.push(`- **Platform:** ${
          this.getPlatformString_(this.data_.debugInfo.platform)}`);
      lines.push(`- **Form Factor:** ${
          this.getFormFactorString_(this.data_.debugInfo.formFactor)}`);
    }
    lines.push(`- **User Agent:** ${navigator.userAgent}`);
    lines.push('');

    const generalContents = this.shadowRoot?.querySelector('#general-contents');
    if (generalContents) {
      const cards = generalContents.querySelectorAll('.card');
      for (const card of cards) {
        const headerEl = card.querySelector('.card-header');
        const headerText = headerEl?.textContent?.trim();
        const table = card.querySelector('table');
        if (!table) {
          continue;
        }
        const tableMd = this.tableToMarkdown_(table);
        if (!tableMd) {
          continue;
        }
        if (headerText) {
          lines.push(`## ${headerText}`);
          lines.push('');
        }
        lines.push(tableMd);
        lines.push('');
      }
    }

    return lines.join('\n');
  }

  private cleanTableCellText_(text: string): string {
    return text.replace(/\s+/g, ' ').replace(/\|/g, '\\|').trim();
  }

  private tableToMarkdown_(table: HTMLTableElement): string {
    const headerCells = Array.from(table.querySelectorAll('thead th'));
    if (headerCells.length === 0) {
      return '';
    }

    const headers =
        headerCells.map(th => this.cleanTableCellText_(th.textContent ?? ''));
    const lines: string[] = [
      `| ${headers.join(' | ')} |`,
      `| ${headers.map(() => ':---').join(' | ')} |`,
    ];

    const rowElements = Array.from(table.querySelectorAll('tbody tr'));
    for (const row of rowElements) {
      const cells =
          Array.from(row.querySelectorAll('td'))
              .map(td => this.cleanTableCellText_(td.textContent ?? ''));
      if (cells.length === 0) {
        continue;
      }
      while (cells.length < headers.length) {
        cells.push('');
      }
      lines.push(`| ${cells.join(' | ')} |`);
    }

    return lines.join('\n');
  }

  protected async onCopyDiagnosticsClick_() {
    await this.updateComplete;
    const text = this.getDiagnosticsMarkdown_();
    try {
      await navigator.clipboard.writeText(text);
      if (this.copyResetTimeoutId_ !== null) {
        window.clearTimeout(this.copyResetTimeoutId_);
      }
      this.copyButtonText_ = 'Copied!';
      this.copyResetTimeoutId_ = window.setTimeout(() => {
        this.copyButtonText_ = 'Copy Diagnostics';
        this.copyResetTimeoutId_ = null;
      }, 2000);
    } catch (err) {
      console.error('Failed to copy diagnostics to clipboard:', err);
    }
  }

  protected isExperimentalOptInConsentMet_(): boolean {
    return this.data_?.enablement?.glicExperimentalTriggeringState ===
        GlicExperimentalTriggeringState.kReady;
  }

  protected getTableData_(): Array<{label: string, value: boolean}> {
    if (!this.data_ || !this.data_.enablement) {
      return [];
    }

    return [
      {
        label: 'Enabled by Chrome Flags',
        value: this.data_.enablement.featureEnabled,
      },
      {
        label: 'Regular profile',
        value: this.data_.enablement.isRegularProfile,
      },
      {
        label: 'Pref or flag based rollout (flag or pref) applies',
        value: this.data_.enablement.isRolledOut,
      },
      {
        label: 'Account exists and has the Gemini in Chrome capability',
        value: this.data_.enablement.primaryAccountIsCapable,
      },
      {
        label: 'Account exists and is fully signed-in',
        value: this.data_.enablement.primaryAccountIsFullySignedIn,
      },
      {
        label:
            'Chrome Enterprise policy allows this feature (or doesn\'t apply)',
        value: this.data_.enablement.allowedByChromePolicy,
      },
      {
        label: 'Server side admin allows this feature',
        value: this.data_.enablement.allowedByRemoteAdmin,
      },
      {
        label: 'Server side allows this feature (Not admin policy)',
        value: this.data_.enablement.allowedByRemoteOther,
      },
      {
        label: 'User did pass the FRE',
        value: this.data_.enablement.freIsConsented,
      },
      {
        label: 'User accepted actuation consent',
        value: this.data_.enablement.actuationIsConsented,
      },
      {
        label: 'Passed country filter',
        value: this.data_.enablement.allowedByCountryFilter,
      },
      {
        label: 'Passed locale filter',
        value: this.data_.enablement.allowedByLocaleFilter,
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
  protected onInvokeFocusOnShowChange_(e: Event) {
    this.invokeFocusOnShow_ = (e.target as HTMLInputElement).checked;
  }
  protected onInvokeTimeoutMsInput_(e: Event) {
    this.invokeTimeoutMs_ = (e.target as HTMLInputElement).value;
  }

  protected onInvokeConversationTypeChange_(e: Event) {
    this.invokeConversationType_ = (e.target as HTMLSelectElement).value;
  }

  protected onInvokeConversationIdInput_(e: Event) {
    this.invokeConversationId_ = (e.target as HTMLInputElement).value;
  }

  protected onPayloadUniversalCartMetadataInput_(e: Event) {
    this.invokePayloadUniversalCartMetadata_ =
        (e.target as HTMLInputElement).value;
  }

  protected async onInvokeSurfaceTypeChange_(e: Event) {
    this.invokeSurfaceType_ = (e.target as HTMLSelectElement).value;
    if (this.invokeSurfaceType_ === 'specificTab') {
      await this.refreshOpenTabs_();
    }
  }

  protected async refreshOpenTabs_() {
    const {tabTitles} = await this.browserProxy_.handler.getOpenTabs();
    this.availableTabs_ = tabTitles;
    this.invokeSpecificTabIndex_ = 0;
  }

  protected onRefreshTabsClick_() {
    this.refreshOpenTabs_();
  }

  protected onInvokeSpecificTabIndexChange_(e: Event) {
    this.invokeSpecificTabIndex_ =
        Number((e.target as HTMLSelectElement).value);
  }

  protected onInvokeSpecificTabsToShareIndexChange_(e: Event) {
    const select = e.target as HTMLSelectElement;
    const indexInArray = Number(select.dataset['index']);
    const newIndices = [...this.invokeSpecificTabsToShareIndices_];
    newIndices[indexInArray] = Number(select.value);
    this.invokeSpecificTabsToShareIndices_ = newIndices;
  }

  protected onRemoveTabsToShareIndexClick_(e: Event) {
    const button = e.target as HTMLElement;
    const indexToRemove = Number(button.dataset['index']);
    this.invokeSpecificTabsToShareIndices_ =
        this.invokeSpecificTabsToShareIndices_.filter(
            (_, index) => index !== indexToRemove);
  }

  protected onAddTabsToShareIndexClick_() {
    this.invokeSpecificTabsToShareIndices_ =
        [...this.invokeSpecificTabsToShareIndices_, 0];
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
  protected onInvokeFreCompletionWaitModeChange_(e: Event) {
    this.invokeFreCompletionWaitMode_ =
        Number((e.target as HTMLSelectElement).value);
  }
  protected onInvokeTakeScreenshotChange_(e: Event) {
    this.invokeTakeScreenshot_ = (e.target as HTMLInputElement).checked;
  }
  protected onInvokeSupersedeIfInProgressChange_(e: Event) {
    this.invokeSupersedeIfInProgress_ = (e.target as HTMLInputElement).checked;
  }
  protected onInvokePublicKeyInput_(e: Event) {
    this.invokePublicKey_ = (e.target as HTMLInputElement).value;
  }
  protected onInvokeAuthSecretInput_(e: Event) {
    this.invokeAuthSecret_ = (e.target as HTMLInputElement).value;
  }
  protected onTriggerInvokeClick_() {
    let surface: TriggerInvokeFromInternalsOptions['surface'];
    if (this.invokeSurfaceType_ === 'newTab') {
      surface = {newTab: {openInForeground: this.invokeOpenInForeground_}};
    } else {
      surface = {defaultSurface: {}};
    }

    let payload: TriggerInvokeFromInternalsOptions['payload'] = null;
    if (this.invokeInvocationSource_ === InvocationSource.kUniversalCart) {
      const bytes = this.invokePayloadUniversalCartMetadata_ ?
          Array.from(
              atob(this.invokePayloadUniversalCartMetadata_),
              c => c.charCodeAt(0)) :
          [];
      payload = {
        universalCart: {
          serializedMetadata: bytes,
        },
      };
    }

    let conversationSelection:
        TriggerInvokeFromInternalsOptions['conversation'] = {
          defaultConversation: {},
        };
    if (this.invokeConversationType_ === 'new') {
      conversationSelection = {newConversation: {}};
    } else if (this.invokeConversationType_ === 'conversationId') {
      conversationSelection = {conversationId: this.invokeConversationId_};
    }

    const options: TriggerInvokeFromInternalsOptions = {
      invocationSource: this.invokeInvocationSource_,
      prompts: this.invokePrompt_ ? [this.invokePrompt_] : [],
      conversation: conversationSelection,
      featureMode: this.invokeFeatureMode_,
      disableZss: false,
      zssAdditionalContent: this.invokeZssOverride_ ?
          (this.invokeZssAdditionalContent_ || null) :
          null,
      skillId: null,
      errorMessage: null,
      timeout: this.invokeTimeoutMs_ ?
          {microseconds: BigInt(Number(this.invokeTimeoutMs_) * 1000)} :
          null,
      autoSubmit: this.invokeAutoSubmit_,
      freOverride: this.invokeFreOverride_,
      waitForPanelOpen: this.invokeWaitForPanelOpen_,
      focusOnShow: this.invokeFocusOnShow_,
      freCompletionWaitMode: this.invokeFreCompletionWaitMode_,
      surface: surface,
      specificTabIndex: this.invokeSurfaceType_ === 'specificTab' ?
          this.invokeSpecificTabIndex_ :
          null,
      specificTabsToShareIndices:
          this.invokeSpecificTabsToShareIndices_.length > 0 ?
          this.invokeSpecificTabsToShareIndices_ :
          null,
      actuationTarget: this.invokeActuationTarget_,
      showPanel: this.invokeAutoSubmit_ ? this.invokeShowPanel_ : null,
      payload: payload,
      takeScreenshot: this.invokeTakeScreenshot_,
      supersedeIfInProgress: this.invokeSupersedeIfInProgress_,
      keyConfig: (this.invokePublicKey_ || this.invokeAuthSecret_) ? {
        publicKey: this.invokePublicKey_,
        authSecret: this.invokeAuthSecret_,
      } :
                                                                     null,
    };

    const invocationSourceMap =
        InvocationSource as unknown as Record<number, string>;
    const featureModeMap = FeatureMode as unknown as Record<number, string>;
    const freOverrideMap = FreOverride as unknown as Record<number, string>;
    const freCompletionWaitModeMap =
        FreCompletionWaitMode as unknown as Record<number, string>;
    const actuationTargetMap =
        ActuationTarget as unknown as Record<number, string>;

    const optionsString = JSON.stringify(options, (key, value) => {
      if (typeof value === 'bigint') {
        value = value.toString();
      }
      if (value === null || value === undefined) {
        return undefined;
      }
      if (Array.isArray(value) && value.length === 0) {
        return undefined;
      }
      if (key === 'conversation') {
        if (value.defaultConversation &&
            Object.keys(value.defaultConversation).length === 0) {
          return undefined;
        }
      }
      if (key === 'surface') {
        if (value.defaultSurface &&
            Object.keys(value.defaultSurface).length === 0) {
          return undefined;
        }
      }
      if (key === 'freOverride' && value === FreOverride.kUnspecified) {
        return undefined;
      }
      if (key === 'featureMode' && value === FeatureMode.kUnspecified) {
        return undefined;
      }
      if (key === 'actuationTarget' &&
          value === ActuationTarget.kAgentDecides) {
        return undefined;
      }
      if (key === 'disableZss' && value === false) {
        return undefined;
      }
      if (key === 'waitForPanelOpen' && value === false) {
        return undefined;
      }
      if (key === 'focusOnShow' && value === true) {
        return undefined;
      }

      if (key === 'invocationSource') {
        return `${value} (${invocationSourceMap[value as number]})`;
      }
      if (key === 'featureMode') {
        return `${value} (${featureModeMap[value as number]})`;
      }
      if (key === 'freOverride') {
        return `${value} (${freOverrideMap[value as number]})`;
      }
      if (key === 'freCompletionWaitMode') {
        return `${value} (${freCompletionWaitModeMap[value as number]})`;
      }
      if (key === 'actuationTarget') {
        return `${value} (${actuationTargetMap[value as number]})`;
      }
      return value;
    }, 2);

    this.invokeLogs_ = [
      `[${new Date().toLocaleTimeString()}] TRIGGERING INVOKE with options:\n${
          optionsString}`,
    ];
    console.info(this.invokeLogs_[0]);

    this.browserProxy_.handler.triggerInvokeFromInternalsAction(options).then(
        ({success, errorMessage}: {success: boolean, errorMessage: string}) => {
          const timestamp = new Date().toLocaleTimeString();
          const logEntry = `[${timestamp}] ${
              success ? 'SUCCESS' : 'ERROR: ' + errorMessage}`;
          this.invokeLogs_ = [...this.invokeLogs_, logEntry];
          console.info(logEntry);
        });
  }

  protected getPlatformString_(platform: Platform): string {
    switch (platform) {
      case Platform.kMacOS:
        return 'macOS';
      case Platform.kWindows:
        return 'Windows';
      case Platform.kLinux:
        return 'Linux';
      case Platform.kChromeOS:
        return 'ChromeOS';
      case Platform.kAndroid:
        return 'Android';
      default:
        return 'Unknown';
    }
  }

  protected getFormFactorString_(formFactor: FormFactor): string {
    switch (formFactor) {
      case FormFactor.kDesktop:
        return 'Desktop';
      case FormFactor.kPhone:
        return 'Phone';
      case FormFactor.kTablet:
        return 'Tablet';
      default:
        return 'Unknown';
    }
  }

  protected getDebugSettingsData_():
      Array<{label: string, value: string|boolean}> {
    if (!this.data_ || !this.data_.debugInfo) {
      return [];
    }

    const debugInfo = this.data_.debugInfo;
    const settings: Array<{label: string, value: string | boolean}> = [
      {
        label: 'GlicActor Feature Flag',
        value: debugInfo.glicActorFeatureEnabled,
      },
      {
        label: 'GlicRollout Feature Flag',
        value: debugInfo.glicRolloutFeatureEnabled,
      },
      {
        label: 'GlicTieredRollout Feature Flag',
        value: debugInfo.glicTieredRolloutFeatureEnabled,
      },
      {
        label: 'GlicTieredRolloutV2 Feature Flag',
        value: debugInfo.glicTieredRolloutV2FeatureEnabled,
      },
      {
        label: 'Platform',
        value: this.getPlatformString_(debugInfo.platform),
      },
      {
        label: 'Form Factor',
        value: this.getFormFactorString_(debugInfo.formFactor),
      },
      {
        label: 'Launcher Hotkey',
        value: debugInfo.hotkey || 'None',
      },
      {
        label: 'Locale',
        value: debugInfo.locale || 'None',
      },
      {
        label: 'Permanent Country Code',
        value: debugInfo.permanentCountryCode || 'None',
      },
      {
        label: 'Session Country Code',
        value: debugInfo.sessionCountryCode || 'None',
      },
      {
        label: 'System Requirement Met',
        value: debugInfo.systemRequirementMet,
      },
      {
        label: 'OS Version Supported',
        value: debugInfo.osVersionSupported,
      },
      {
        label: 'Anchor Entrypoint Override Active',
        value: debugInfo.anchorEntrypointOverrideActive,
      },
      {
        label: 'Primary Account Needs Signed In',
        value: debugInfo.primaryAccountNeedsSignedIn,
      },
      {
        label: 'Dogfood Client Status',
        value: debugInfo.dogfoodStatus,
      },
    ];

    if (debugInfo.booleanSettings) {
      for (const [key, val] of Object.entries(debugInfo.booleanSettings)) {
        settings.push({
          label: key,
          value: val,
        });
      }
    }

    return settings;
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
