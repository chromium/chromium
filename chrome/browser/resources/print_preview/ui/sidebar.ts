// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import './advanced_options_settings.js';
import './button_strip.js';
import './color_settings.js';
import './copies_settings.js';
import './destination_settings.js';
import './dpi_settings.js';
import './duplex_settings.js';
import './header.js';
import './layout_settings.js';
import './media_size_settings.js';
import './margins_settings.js';
import './more_settings.js';
import './other_options_settings.js';
import './pages_per_sheet_settings.js';
import './pages_settings.js';
import './print_preview_vars.css.js';
import './scaling_settings.js';
import '/strings.m.js';
import './link_container.js';

import {CrContainerShadowMixinLit} from 'chrome://resources/cr_elements/cr_container_shadow_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {DarkModeMixin} from '../dark_mode_mixin.js';
import type {Cdd} from '../data/cdd.js';
import type {Destination} from '../data/destination.js';
import type {Settings} from '../data/model.js';
import type {Error} from '../data/state.js';
import {State} from '../data/state.js';
import {MetricsContext, PrintSettingsUiBucket} from '../metrics.js';

import {DestinationState} from './destination_settings.js';
import type {PrintPreviewDestinationSettingsElement} from './destination_settings.js';
import {SettingsMixin} from './settings_mixin.js';
import {getCss} from './sidebar.css.js';
import {getHtml} from './sidebar.html.js';

/**
 * Number of settings sections to show when "More settings" is collapsed.
 */
const MAX_SECTIONS_TO_SHOW: number = 6;

// Settings to observe their `available` field to decide whether to show the
// "More settings" collapsible section.
const SETTINGS_TO_OBSERVE: Array<keyof Settings> = [
  'color',
  'copies',
  'dpi',
  'duplex',
  'layout',
  'margins',
  'mediaSize',
  'otherOptions',
  'pages',
  'pagesPerSheet',
  'scaling',
  'vendorItems',
];


export interface PrintPreviewSidebarElement {
  $: {
    destinationSettings: PrintPreviewDestinationSettingsElement,
  };
}

const PrintPreviewSidebarElementBase = CrContainerShadowMixinLit(
    WebUiListenerMixinLit(SettingsMixin(DarkModeMixin(CrLitElement))));

export class PrintPreviewSidebarElement extends PrintPreviewSidebarElementBase {
  static get is() {
    return 'print-preview-sidebar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      controlsManaged: {type: Boolean},

      destination: {
        type: Object,
        notify: true,
      },

      destinationCapabilities_: {type: Object},

      destinationState: {
        type: Number,
        notify: true,
      },

      error: {
        type: Number,
        notify: true,
      },

      isPdf: {type: Boolean},
      pageCount: {type: Number},
      state: {type: Number},
      controlsDisabled_: {type: Boolean},
      firstLoad_: {type: Boolean},
      isInAppKioskMode_: {type: Boolean},
      settingsExpandedByUser_: {type: Boolean},
      shouldShowMoreSettings_: {type: Boolean},
      settingsAvailable_: {type: Object},
    };
  }

  accessor controlsManaged: boolean = false;
  accessor destination: Destination|null = null;
  accessor destinationCapabilities_: Cdd|null = null;
  accessor destinationState: DestinationState = DestinationState.INIT;
  accessor error: Error|null = null;
  accessor isPdf: boolean = false;
  accessor pageCount: number = 0;
  accessor state: State = State.NOT_READY;
  protected accessor settingsAvailable_: Record<keyof Settings, boolean>;
  protected accessor controlsDisabled_: boolean = false;
  protected accessor firstLoad_: boolean = true;
  protected accessor isInAppKioskMode_: boolean = false;
  protected accessor settingsExpandedByUser_: boolean = false;
  protected accessor shouldShowMoreSettings_: boolean = false;

  constructor() {
    super();

    this.settingsAvailable_ = {} as Record<keyof Settings, boolean>;
    for (const key of SETTINGS_TO_OBSERVE) {
      this.settingsAvailable_[key] = true;
    }
  }

  override connectedCallback() {
    super.connectedCallback();

    for (const key of SETTINGS_TO_OBSERVE) {
      this.addSettingObserver(`${key}.available`, (value: boolean) => {
        this.settingsAvailable_[key] = value;
        this.updateShouldShowMoreSettings_();
        this.requestUpdate();
      });
    }
    this.updateShouldShowMoreSettings_();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('state')) {
      this.controlsDisabled_ = this.state !== State.READY;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('state')) {
      this.onStateChanged_();
    }
  }

  /**
   * @param defaultPrinter The system default printer ID.
   * @param serializedDestinationSelectionRulesStr String with rules
   *     for selecting the default destination.
   * @param pdfPrinterDisabled Whether the PDF printer is disabled.
   */
  init(
      appKioskMode: boolean, defaultPrinter: string,
      serializedDestinationSelectionRulesStr: string|null,
      pdfPrinterDisabled: boolean) {
    this.isInAppKioskMode_ = appKioskMode;
    pdfPrinterDisabled = this.isInAppKioskMode_ || pdfPrinterDisabled;

    this.$.destinationSettings.init(
        defaultPrinter, pdfPrinterDisabled,
        serializedDestinationSelectionRulesStr);
  }

  private updateShouldShowMoreSettings_() {
    // Destination settings is always available. See if the total number of
    // available sections exceeds the maximum number to show.
    this.shouldShowMoreSettings_ =
        SETTINGS_TO_OBSERVE.reduce((count, setting) => {
          return this.settingsAvailable_[setting] ? count + 1 : count;
        }, 1) > MAX_SECTIONS_TO_SHOW;
  }

  /**
   * @return Whether the "more settings" collapse should be expanded.
   */
  protected shouldExpandSettings_(): boolean {
    if (this.settingsExpandedByUser_ === undefined ||
        this.shouldShowMoreSettings_ === undefined) {
      return false;
    }

    // Expand the settings if the user has requested them expanded or if more
    // settings is not displayed (i.e. less than 6 total settings available).
    return this.settingsExpandedByUser_ || !this.shouldShowMoreSettings_;
  }

  protected onPrintButtonFocused_() {
    this.firstLoad_ = false;
  }

  private onStateChanged_() {
    if (this.state !== State.PRINTING) {
      return;
    }

    if (this.shouldShowMoreSettings_) {
      MetricsContext.printSettingsUi().record(
          this.settingsExpandedByUser_ ?
              PrintSettingsUiBucket.PRINT_WITH_SETTINGS_EXPANDED :
              PrintSettingsUiBucket.PRINT_WITH_SETTINGS_COLLAPSED);
    }
  }

  /** @return Whether the system dialog link is available. */
  systemDialogLinkAvailable(): boolean {
    const linkContainer =
        this.shadowRoot.querySelector('print-preview-link-container');
    return !!linkContainer && linkContainer.systemDialogLinkAvailable();
  }

  protected onDestinationChanged_(e: CustomEvent<{value: Destination}>) {
    this.destination = e.detail.value;
    this.destinationCapabilities_ = null;
  }

  protected onDestinationCapabilitiesChanged_(e: CustomEvent<Destination>) {
    assert(this.destination);
    assert(e.detail.id === this.destination.id);
    // When `this.destination.capabilities` changes it is always a new object.
    this.destinationCapabilities_ = this.destination.capabilities;
  }

  protected onDestinationStateChanged_(
      e: CustomEvent<{value: DestinationState}>) {
    this.destinationState = e.detail.value;
  }

  protected onErrorChanged_(e: CustomEvent<{value: Error}>) {
    this.error = e.detail.value;
  }

  protected onSettingsExpandedByUserChanged_(e: CustomEvent<{value: boolean}>) {
    this.settingsExpandedByUser_ = e.detail.value;
  }
}

export type SidebarElement = PrintPreviewSidebarElement;

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-sidebar': PrintPreviewSidebarElement;
  }
}

customElements.define(
    PrintPreviewSidebarElement.is, PrintPreviewSidebarElement);
