// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import 'chrome://resources/cr_components/customize_themes/customize_themes.js';
import './customize_backgrounds.js';
import './customize_shortcuts.js';
import './customize_modules.js';

import {CustomizeThemesElement} from 'chrome://resources/cr_components/customize_themes/customize_themes.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CustomizeBackgroundsElement} from './customize_backgrounds.js';
import {getTemplate} from './customize_dialog.html.js';
import {CustomizeDialogPage} from './customize_dialog_types.js';
import {loadTimeData} from './i18n_setup.js';
import {BackgroundCollection, CustomizeDialogAction, PageHandlerRemote, Theme} from './new_tab_page.mojom-webui.js';
import {NewTabPageProxy} from './new_tab_page_proxy.js';
import {createScrollBorders} from './utils.js';


export interface CustomizeDialogElement {
  $: {
    backgrounds: CustomizeBackgroundsElement,
    bottomPageScrollBorder: HTMLElement,
    customizeThemes: CustomizeThemesElement,
    dialog: CrDialogElement,
    menu: HTMLElement,
    pages: HTMLElement,
    refreshToggle: CrToggleElement,
    topPageScrollBorder: HTMLElement,
  };
}

/**
 * Dialog that lets the user customize the NTP such as the background color or
 * image.
 */
export class CustomizeDialogElement extends PolymerElement {
  static get is() {
    return 'ntp-customize-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      theme: Object,

      selectedPage: {
        type: String,
        observer: 'onSelectedPageChange_',
      },

      selectedCollection_: Object,

      showTitleNavigation_: {
        type: Boolean,
        computed:
            'computeShowTitleNavigation_(selectedPage, selectedCollection_)',
        value: false,
      },

      isRefreshToggleChecked_: {
        type: Boolean,
        computed: `computeIsRefreshToggleChecked_(theme, selectedCollection_)`,
      },

      shortcutsEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('shortcutsEnabled'),
      },

      modulesEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('modulesEnabled'),
      },
    };
  }

  theme: Theme;
  selectedPage: CustomizeDialogPage;
  private selectedCollection_: BackgroundCollection|null;
  private showTitleNavigation_: boolean;
  private isRefreshToggleChecked_: boolean;
  private shortcutsEnabled_: boolean;
  private modulesEnabled_: boolean;

  private pageHandler_: PageHandlerRemote;
  private intersectionObservers_: IntersectionObserver[] = [];

  constructor() {
    super();
    this.pageHandler_ = NewTabPageProxy.getInstance().handler;
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.intersectionObservers_.forEach(observer => {
      observer.disconnect();
    });
    this.intersectionObservers_ = [];
  }

  override ready() {
    super.ready();
    this.intersectionObservers_ = [
      createScrollBorders(
          this.$.menu, this.$.topPageScrollBorder,
          this.$.bottomPageScrollBorder, 'show-1'),
      createScrollBorders(
          this.$.pages, this.$.topPageScrollBorder,
          this.$.bottomPageScrollBorder, 'show-2'),
    ];
    this.pageHandler_.onCustomizeDialogAction(
        CustomizeDialogAction.kOpenClicked);
  }

  private onCancel_() {
    this.$.backgrounds.revertBackgroundChanges();
    this.$.customizeThemes.revertThemeChanges();
  }

  private onCancelClick_() {
    this.pageHandler_.onCustomizeDialogAction(
        CustomizeDialogAction.kCancelClicked);
    this.$.dialog.cancel();
  }

  private onDoneClick_() {
    this.$.backgrounds.confirmBackgroundChanges();
    this.$.customizeThemes.confirmThemeChanges();
    this.shadowRoot!.querySelector('ntp-customize-shortcuts')!.apply();
    if (this.modulesEnabled_) {
      this.shadowRoot!.querySelector('ntp-customize-modules')!.apply();
    }
    this.pageHandler_.onCustomizeDialogAction(
        CustomizeDialogAction.kDoneClicked);
    this.$.dialog.close();
  }

  private onMenuItemKeyDown_(e: KeyboardEvent) {
    if (!['Enter', ' '].includes(e.key)) {
      return;
    }
    e.preventDefault();
    e.stopPropagation();
    this.selectedPage = (e.target as HTMLElement).getAttribute('page-name') as
        CustomizeDialogPage;
  }

  private onSelectedPageChange_() {
    this.$.pages.scrollTop = 0;
  }

  private computeIsRefreshToggleChecked_(): boolean {
    if (!this.selectedCollection_) {
      return false;
    }
    return !!this.theme && this.theme.dailyRefreshEnabled &&
        this.selectedCollection_!.id === this.theme.backgroundImageCollectionId;
  }

  private computeShowTitleNavigation_(): boolean {
    return this.selectedPage === CustomizeDialogPage.BACKGROUNDS &&
        !!this.selectedCollection_;
  }

  private onBackClick_() {
    this.selectedCollection_ = null;
    this.pageHandler_.onCustomizeDialogAction(
        CustomizeDialogAction.kBackgroundsBackClicked);
    this.$.pages.scrollTop = 0;
  }

  private onBackgroundDailyRefreshToggleChange_() {
    if (this.$.refreshToggle.checked) {
      this.pageHandler_.setDailyRefreshCollectionId(
          this.selectedCollection_!.id);
    } else {
      this.pageHandler_.setDailyRefreshCollectionId('');
    }
    this.pageHandler_.onCustomizeDialogAction(
        CustomizeDialogAction.kBackgroundsRefreshToggleClicked);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-customize-dialog': CustomizeDialogElement;
  }
}

customElements.define(CustomizeDialogElement.is, CustomizeDialogElement);
