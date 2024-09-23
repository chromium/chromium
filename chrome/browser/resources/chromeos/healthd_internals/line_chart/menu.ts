// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cr_button/cr_button.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MENU_TEXT_COLOR_DARK, MENU_TEXT_COLOR_LIGHT} from './configs.js';
import {getTemplate} from './menu.html.js';
import {DataSeries} from './utils/data_series.js';

/**
 * Creates an element of a specified type with a specified class name.
 */
function createElementWithClassName(
    type: string, className: string): HTMLElement {
  const element = document.createElement(type);
  element.className = className;
  return element;
}

export interface HealthdInternalsLineChartMenuElement {
  $: {
    dataButtonsContainer: HTMLElement,
  };
}

/**
 * A menu as a button container to control the visibility of each `DataSeries`
 * in current line chart.
 */
export class HealthdInternalsLineChartMenuElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-line-chart-menu';
  }

  static get template() {
    return getTemplate();
  }

  // Buttons of data series.
  private displayedButtons: HTMLElement[] = [];

  // Used to display buttons for changing visibility of each data.
  private dataSeries: DataSeries[] = [];

  getWidth(): number {
    return this.$.dataButtonsContainer.offsetWidth;
  }

  /**
   * Add a data series to the menu.
   */
  addDataSeries(dataSeries: DataSeries) {
    const idx: number = this.dataSeries.indexOf(dataSeries);
    if (idx !== -1) {
      return;
    }
    const button: HTMLElement = this.createButton(dataSeries);
    this.$.dataButtonsContainer.appendChild(button);
    this.dataSeries.push(dataSeries);
    this.displayedButtons.push(button);

    this.fireMenuButtonsUpdatedEvent();
  }

  private fireMenuButtonsUpdatedEvent() {
    this.dispatchEvent(new CustomEvent(
        'menu-buttons-updated', {bubbles: true, composed: true}));
  }

  /**
   * Create a button to control the data series.
   */
  private createButton(dataSeries: DataSeries): HTMLElement {
    const buttonInner: HTMLElement =
        createElementWithClassName('span', 'line-chart-menu-button-inner-span');
    buttonInner.innerText = dataSeries.getTitle();
    const button: HTMLElement =
        createElementWithClassName('div', 'line-chart-menu-button');
    button.appendChild(buttonInner);
    this.setupButtonOnClickHandler(button, dataSeries);
    this.updateButtonStyle(button, dataSeries);
    return button;
  }

  /**
   * Add a onclick handler to the button.
   */
  private setupButtonOnClickHandler(
      button: HTMLElement, dataSeries: DataSeries) {
    button.addEventListener('click', () => {
      dataSeries.setVisible(!dataSeries.getVisible());
      this.updateButtonStyle(button, dataSeries);
      this.fireMenuButtonsUpdatedEvent();
    });
  }

  /**
   * Update the button style with the visibility and color from data series.
   */
  private updateButtonStyle(button: HTMLElement, dataSeries: DataSeries) {
    if (dataSeries.getVisible()) {
      button.style.backgroundColor = dataSeries.getColor();
      button.style.color = MENU_TEXT_COLOR_LIGHT;
    } else {
      button.style.backgroundColor = MENU_TEXT_COLOR_LIGHT;
      button.style.color = MENU_TEXT_COLOR_DARK;
    }
  }

  private onEnableAllButtonClick() {
    assert(this.dataSeries.length === this.displayedButtons.length);
    for (let i: number = 0; i < this.dataSeries.length; ++i) {
      this.dataSeries[i].setVisible(true);
      this.updateButtonStyle(this.displayedButtons[i], this.dataSeries[i]);
    }
    this.fireMenuButtonsUpdatedEvent();
  }

  private onDisableAllButtonClicked() {
    assert(this.dataSeries.length === this.displayedButtons.length);
    for (let i: number = 0; i < this.dataSeries.length; ++i) {
      this.dataSeries[i].setVisible(false);
      this.updateButtonStyle(this.displayedButtons[i], this.dataSeries[i]);
    }
    this.fireMenuButtonsUpdatedEvent();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-line-chart-menu': HealthdInternalsLineChartMenuElement;
  }
}

customElements.define(
    HealthdInternalsLineChartMenuElement.is,
    HealthdInternalsLineChartMenuElement);
