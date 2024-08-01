// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import './config_slider.js';

import type {CrDialogElement} from '//resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {HealthdInternalsConfigSliderElement} from './config_slider.js';
import {getTemplate} from './settings_dialog.html.js';

export interface HealthdInternalsSettingsDialogElement {
  $: {
    dialog: CrDialogElement,
    dataPollingCycleSlider: HealthdInternalsConfigSliderElement,
    dataRetentionDurationSlider: HealthdInternalsConfigSliderElement,
    uiUpdateIntervalSlider: HealthdInternalsConfigSliderElement,
  };
}

export class HealthdInternalsSettingsDialogElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-settings-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      uiUpdateInterval: {
        type: Number,
        observer: 'onUiUpdateIntervalChanged',
      },
      dataPollingCycle: {
        type: Number,
        observer: 'onDataPollingCycleChanged',
      },
      dataRetentionDuration: {
        type: Number,
        observer: 'onDataRetentionDurationChanged',
      },
    };
  }

  override connectedCallback() {
    super.connectedCallback();

    this.$.uiUpdateIntervalSlider.initSlider(1, 5, 1);
    this.$.uiUpdateIntervalSlider.initTitle('UI update interval (second)');
    this.$.dataPollingCycleSlider.initSlider(100, 5000, 100);
    this.$.dataPollingCycleSlider.initTitle(
        'Healthd data polling cycle (millisecond)');
    this.$.dataRetentionDurationSlider.initSlider(1, 12, 1);
    this.$.dataRetentionDurationSlider.initTitle(
        'Data retention duration for line charts (hour)');
  }

  // UI update interval. Default: 3 seconds.
  private uiUpdateInterval: number = 3;
  // Healthd data polling cycle. Default: 1000 milliseconds.
  private dataPollingCycle: number = 1000;
  // Data retention duration for line charts. Default: 6 hours.
  private dataRetentionDuration: number = 6;

  openSettingsDialog() {
    this.$.uiUpdateIntervalSlider.setTickValue(this.uiUpdateInterval);
    this.$.dataPollingCycleSlider.setTickValue(this.dataPollingCycle);
    this.$.dataRetentionDurationSlider.setTickValue(this.dataRetentionDuration);
    this.$.dialog.showModal();
  }

  // Get the UI update interval in seconds.
  getUiUpdateInterval(): number {
    return this.uiUpdateInterval;
  }

  // Get the Healthd data polling cycle in milliseconds.
  getHealthdDataPollingCycle(): number {
    return this.dataPollingCycle;
  }

  // Get the data retention duration for the line chart in hours.
  getDataRetentionDuration(): number {
    return this.dataRetentionDuration;
  }

  private onCancelButtonClicked() {
    this.$.dialog.close();
  }

  private onConfirmButtonClick() {
    this.uiUpdateInterval = this.$.uiUpdateIntervalSlider.getTickValue();
    this.dataPollingCycle = this.$.dataPollingCycleSlider.getTickValue();
    this.dataRetentionDuration =
        this.$.dataRetentionDurationSlider.getTickValue();
    this.$.dialog.close();
  }

  private onUiUpdateIntervalChanged() {
    this.dispatchEvent(new CustomEvent(
        'ui-update-interval-updated', {bubbles: true, composed: true}));
  }

  private onDataPollingCycleChanged() {
    this.dispatchEvent(new CustomEvent(
        'polling-cycle-updated', {bubbles: true, composed: true}));
  }

  private onDataRetentionDurationChanged() {
    this.dispatchEvent(new CustomEvent(
        'data-retention-updated', {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-settings-dialog': HealthdInternalsSettingsDialogElement;
  }
}

customElements.define(
    HealthdInternalsSettingsDialogElement.is,
    HealthdInternalsSettingsDialogElement);
