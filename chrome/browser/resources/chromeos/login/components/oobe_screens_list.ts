// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/icons.html.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {DomRepeatEvent, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeI18nMixin} from './mixins/oobe_i18n_mixin.js';

import {getTemplate} from './oobe_screens_list.html.js';

/**
 * Data that is passed to the component during initialization.
 */
export interface OobeScreensListScreen {
  screenId: string;
  title: string;
  subtitle?: string;
  icon: string;
  selected: boolean;
  isRevisitable: boolean;
  isSynced: boolean;
  isCompleted: boolean;
}

export interface OobeScreensListData extends Array<OobeScreensListScreen>{}

const OobeScreensListBase = OobeI18nMixin(PolymerElement);

export class OobeScreensList extends OobeScreensListBase {
  static get is() {
    return 'oobe-screens-list' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * List of screens to display.
       */
      screensList: {
        type: Array,
        value: [],
        notify: true,
      },
      /**
       * List of selected screens.
       */
      screensSelected: {
        type: Array,
        value: [],
      },
      /**
       * Number of selected screens.
       */
      selectedScreensCount: {
        type: Number,
        value: 0,
        notify: true,
      },
    };
  }

  private screensList: OobeScreensListData;
  private screensSelected: string[];
  private selectedScreensCount: number;

  /**
   * Initialize the list of screens.
   */
  init(screens: OobeScreensListData): void {
    this.screensList = screens;
    this.screensSelected = [];
    this.selectedScreensCount = 0;
  }

  /**
   * Return the list of selected screens.
   */
  getScreenSelected(): string[] {
    return this.screensSelected;
  }

  private onClick(e: DomRepeatEvent<OobeScreensListScreen, MouseEvent>): void {
    const clickedScreen = e.model.item;
    const previousSelectedState = clickedScreen.selected;
    const currentSelectedState = !previousSelectedState;
    const path =
        `screensList.${this.screensList.indexOf(clickedScreen)}.selected`;
    this.set(path, currentSelectedState);
    (e.currentTarget as HTMLElement)?.
      setAttribute('checked', String(currentSelectedState));

    if (currentSelectedState) {
      this.selectedScreensCount++;
      this.screensSelected.push(clickedScreen.screenId);
    } else {
      this.selectedScreensCount--;
      this.screensSelected.splice(
          this.screensSelected.indexOf(clickedScreen.screenId), 1);
    }
    this.notifyPath('screensList');
  }

  private getSubtitle(locale: string, screenSubtitle: string|null,
      screenId: string): string {
    if (screenSubtitle) {
      // display size screen is special case as the subtitle include directly
      // the percentage  and will be placed in the message placeholder.
      if (screenId === 'display-size') {
        return this.i18nDynamic(
            locale, 'choobeDisplaySizeSubtitle', screenSubtitle);
      }
      return this.i18nDynamic(locale, screenSubtitle);
    }
    return '';
  }

  private isScreenDisabled(isRevisitable: boolean,
      isCompleted: boolean): boolean {
    return !isRevisitable && isCompleted;
  }

  private isSyncedIconHidden(isSynced: boolean, isCompleted: boolean,
      isSelected: boolean): boolean {
    return !isSynced || isSelected || isCompleted;
  }

  private isScreenVisited(isSelected: boolean, isCompleted: boolean): boolean {
    return isCompleted && !isSelected;
  }

  private getScreenId(screenId: string): string {
    return 'cr-button-' + screenId;
  }

  override focus(): void {
    const screens = this.shadowRoot?.querySelectorAll('.screen-item');
    if (!screens || screens.length < 1) {
      this.focus();
    } else {
      // Focus the first enabled screen in the list
      for (const screen of screens) {
        if (!this.isScreenDisabled(
                screen.hasAttribute('isRevisitable'),
                screen.hasAttribute('isCompleted'))) {
          (screen as HTMLElement).focus();
          return;
        }
      }
    }
  }

  private getAriaLabelToggleButtons(
      locale: string, screenTitle: string, screenSubtitle: string|null,
      screenIsSynced: boolean, screenIsCompleted: boolean, screenId: string,
      screenIsSelected: boolean): string {
    let ariaLabel = this.i18nDynamic(locale, screenTitle);
    if (screenSubtitle) {
      if (screenId === 'display-size') {
        ariaLabel = ariaLabel + '.' + screenSubtitle;
      } else {
        ariaLabel = ariaLabel + '.' + this.i18nDynamic(locale, screenSubtitle);
      }
    }
    if (!screenIsSelected && screenIsCompleted) {
      ariaLabel =
          ariaLabel + '.' + this.i18nDynamic(locale, 'choobeVisitedTile');
    }
    if (!screenIsSelected && !screenIsCompleted && screenIsSynced) {
      ariaLabel =
          ariaLabel + '.' + this.i18nDynamic(locale, 'choobeSyncedTile');
    }
    return ariaLabel;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OobeScreensList.is]: OobeScreensList;
  }
}

customElements.define(OobeScreensList.is, OobeScreensList);
