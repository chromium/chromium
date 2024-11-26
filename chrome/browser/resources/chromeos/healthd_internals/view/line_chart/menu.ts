// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cr_button/cr_button.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DataSeries} from '../../model/data_series.js';
import {MENU_TEXT_COLOR_DARK, MENU_TEXT_COLOR_LIGHT} from '../../utils/line_chart_configs.js';

import {getTemplate} from './menu.html.js';

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

interface ButtonState {
  // Used to get the button visible state and color.
  data: DataSeries;
  // The element for menu button.
  element: HTMLElement;
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

  // Used to display buttons for changing visibility of each data.
  private buttons: ButtonState[] = [];

  getWidth(): number {
    return this.$.dataButtonsContainer.offsetWidth;
  }

  /**
   * Set up the list of data series for the menu.
   */
  setupDataSeriesList(dataSeriesList: DataSeries[]) {
    const buttonContainer = this.$.dataButtonsContainer;
    while (buttonContainer.lastElementChild) {
      buttonContainer.removeChild(buttonContainer.lastElementChild);
    }
    this.buttons = [];

    for (const dataSeries of dataSeriesList) {
      const button = this.createButton(dataSeries);
      buttonContainer.appendChild(button);
      this.buttons.push({data: dataSeries, element: button});
    }
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
    for (const button of this.buttons) {
      button.data.setVisible(true);
      this.updateButtonStyle(button.element, button.data);
    }
    this.fireMenuButtonsUpdatedEvent();
  }

  private onDisableAllButtonClicked() {
    for (const button of this.buttons) {
      button.data.setVisible(false);
      this.updateButtonStyle(button.element, button.data);
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
