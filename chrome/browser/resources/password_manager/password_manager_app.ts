// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_page_host_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './checkup_section.js';
import './checkup_details_section.js';
import './passwords_section.js';
import './password_details_section.js';
import './settings_section.js';
import './shared_style.css.js';
import './side_bar.js';
import './toolbar.js';

import {CrContainerShadowMixin} from 'chrome://resources/cr_elements/cr_container_shadow_mixin.js';
import {CrDrawerElement} from 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import {listenOnce} from 'chrome://resources/js/util_ts.js';
import {IronPagesElement} from 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import {DomIf, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './password_manager_app.html.js';
import {Page, Route, RouteObserverMixin} from './router.js';
import {SettingsSectionElement} from './settings_section.js';
import {PasswordManagerSideBarElement} from './side_bar.js';
import {PasswordManagerToolbarElement} from './toolbar.js';

export interface PasswordManagerAppElement {
  $: {
    content: IronPagesElement,
    drawer: CrDrawerElement,
    drawerTemplate: DomIf,
    settings: SettingsSectionElement,
    sidebar: PasswordManagerSideBarElement,
    toolbar: PasswordManagerToolbarElement,
  };
}

const PasswordManagerAppElementBase =
    CrContainerShadowMixin(RouteObserverMixin(PolymerElement));

export class PasswordManagerAppElement extends PasswordManagerAppElementBase {
  static get is() {
    return 'password-manager-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedPage_: String,

      narrow_: {
        type: Boolean,
        observer: 'onNarrowChanged_',
      },

      /*
       * Mirroring the enum so that it can be used from HTML bindings.
       */
      pagesValueEnum_: {
        type: Object,
        value: Page,
      },
    };
  }

  private selectedPage_: Page;
  private narrow_: boolean;

  override ready() {
    super.ready();

    // Lazy-create the drawer the first time it is opened or swiped into view.
    listenOnce(this.$.drawer, 'cr-drawer-opening', () => {
      this.$.drawerTemplate.if = true;
    });

    this.addEventListener('cr-toolbar-menu-tap', this.onMenuButtonTap_);
  }

  override currentRouteChanged(route: Route): void {
    this.selectedPage_ = route.page;
    setTimeout(() => {  // Async to allow page to load.
      if (route.page === Page.CHECKUP_DETAILS) {
        this.enableShadowBehavior(false);
        this.showDropShadows();
      } else {
        this.enableShadowBehavior(true);
      }
    }, 0);
  }

  private onNarrowChanged_() {
    if (this.$.drawer.open && !this.narrow_) {
      this.$.drawer.close();
    }
  }

  private onMenuButtonTap_() {
    this.$.drawer.toggle();
  }

  setNarrowForTesting(state: boolean) {
    this.narrow_ = state;
  }

  private showPage(currentPage: string, pageToShow: string): boolean {
    return currentPage === pageToShow;
  }
}
declare global {
  interface HTMLElementTagNameMap {
    'password-manager-app': PasswordManagerAppElement;
  }
}

customElements.define(PasswordManagerAppElement.is, PasswordManagerAppElement);
