// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-location/iron-location.js';
import '//resources/polymer/v3_0/iron-pages/iron-pages.js';

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
    buttonContainer: HTMLElement,
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
    return this.$.buttonContainer.offsetWidth;
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
    this.displayedButtons.push(button);
    this.$.buttonContainer.appendChild(button);
    this.dataSeries.push(dataSeries);

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

    const visible: boolean = dataSeries.getVisible();
    this.updateButtonStyle(button, dataSeries, visible);
    return button;
  }

  /**
   * Add a onclick handler to the button.
   */
  private setupButtonOnClickHandler(
      button: HTMLElement, dataSeries: DataSeries) {
    button.addEventListener('click', () => {
      const newVisible: boolean = !dataSeries.getVisible();
      dataSeries.setVisible(newVisible);
      this.updateButtonStyle(button, dataSeries, newVisible);
      this.fireMenuButtonsUpdatedEvent();
    });
  }

  /**
   * Update the button style with the visibility of data series.
   */
  private updateButtonStyle(
      button: HTMLElement, dataSeries: DataSeries, visible: boolean) {
    if (visible) {
      button.style.backgroundColor = dataSeries.getColor();
      button.style.color = MENU_TEXT_COLOR_LIGHT;
    } else {
      button.style.backgroundColor = MENU_TEXT_COLOR_LIGHT;
      button.style.color = MENU_TEXT_COLOR_DARK;
    }
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
