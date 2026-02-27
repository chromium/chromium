// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_page_selector/cr_page_selector.js';
import '//resources/cr_elements/cr_toolbar/cr_toolbar.js';
import '//resources/cr_elements/cr_drawer/cr_drawer.js';
import './icons.html.js';
import './user_skills_page.js';
import './discover_skills_page.js';
import './error_page.js';
import './sidebar.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import type {CrDrawerElement} from '//resources/cr_elements/cr_drawer/cr_drawer.js';
import type {CrToolbarElement} from '//resources/cr_elements/cr_toolbar/cr_toolbar.js';
import {CrRouter} from '//resources/js/cr_router.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {DiscoverSkillsPageElement} from './discover_skills_page.js';
import type {SkillsSidebarElement} from './sidebar.js';
import {Page} from './sidebar.js';
import {SkillsPageBrowserProxy} from './skills_page_browser_proxy.js';
import type {UserSkillsPageElement} from './user_skills_page.js';

export interface SkillsAppElement {
  $: {
    menu: SkillsSidebarElement,
    toolbar: CrToolbarElement,
    userSkillsPage: UserSkillsPageElement,
    discoverSkillsPage: DiscoverSkillsPageElement,
    drawer: CrDrawerElement,
  };
}

export class SkillsAppElement extends CrLitElement {
  static get is() {
    return 'skills-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selectedPage_: {type: String},
      narrow_: {type: Boolean},
      isDrawerOpen_: {type: Boolean},
      shouldShowErrorPage_: {type: Boolean},
    };
  }

  protected accessor selectedPage_: Page = Page.USER_SKILLS;
  protected accessor narrow_: boolean = false;
  protected accessor isDrawerOpen_: boolean = false;
  protected accessor shouldShowErrorPage_: boolean =
      !loadTimeData.getBoolean('isGlicEnabled');
  private eventTracker_: EventTracker = new EventTracker();
  private proxy_: SkillsPageBrowserProxy = SkillsPageBrowserProxy.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    ColorChangeUpdater.forDocument().start();
    const router = CrRouter.getInstance();
    // Dwell time prevents polluting the browser history with rapid changes.
    // 0 to make sure every click is able to be navigated back to.
    router.setDwellTime(0);
    // Initial load.
    this.onPathChanged_(router.getPath());
    // Listen for path changes, only triggers via popstate.
    this.eventTracker_.add(
        router, 'cr-router-path-changed',
        (e: Event) => this.onPathChanged_((e as CustomEvent<string>).detail));
    // Event-driven navigation changes.
    this.eventTracker_.add(document, 'route-click', (e: Event) => {
      const detail = (e as CustomEvent<{path: string}>).detail;
      // CRRouter sets the path, but doesn't trigger a popstate event, so we
      // need to manually update the page.
      CrRouter.getInstance().setPath(detail.path);
      this.onPathChanged_(detail.path);
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  override updated(changedProperties: PropertyValues) {
    super.updated(changedProperties as PropertyValues<this>);

    if (changedProperties.has('selectedPage_') &&
        this.selectedPage_ === Page.DISCOVER_SKILLS) {
      this.proxy_.handler.request1PSkills();
    }
  }

  // Called when the page is narrow & the menu button appears.
  // Clicking it should open a cr-drawer.
  protected onCrToolbarMenuClick_() {
    this.$.drawer.openDrawer();
    this.isDrawerOpen_ = this.$.drawer.open;
  }

  // Called whenever the text in the search input field changes.
  protected onSearchChanged_(e: CustomEvent<string>) {
    const searchTerm = e.detail;
    this.$.userSkillsPage.onSearchChanged(searchTerm);
    this.$.discoverSkillsPage.onSearchChanged(searchTerm);
  }

  // Called whenever the browser window drops below the defined
  // narrow-threshold.
  protected onNarrowChanged_(e: CustomEvent<{value: boolean}>) {
    this.narrow_ = e.detail.value;
    if (this.isDrawerOpen_ && !this.narrow_) {
      this.$.drawer.close();
    }
    if (this.narrow_) {
      this.style.setProperty('--sidebar-width', '0px');
    } else {
      this.style.removeProperty('--sidebar-width');
    }
  }

  protected onDrawerClose_() {
    this.isDrawerOpen_ = this.$.drawer.open;
  }

  protected shouldShowToolbarMenu_() {
    return this.narrow_ && !this.shouldShowErrorPage_;
  }

  private onPathChanged_(newPath: string) {
    if (this.shouldShowErrorPage_) {
      return;
    }
    const path = newPath.substring(1);
    let menuItem = this.$.menu.menuItems.find(item => item.page === path);
    // If the menu item isn't found, replace the undefined path with the
    // first menu item.
    if (!menuItem) {
      window.history.replaceState(
          undefined, '', '/' + this.$.menu.menuItems[0]!.page);
      menuItem = this.$.menu.menuItems[0];
    }
    this.selectedPage_ = menuItem!.page;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'skills-app': SkillsAppElement;
  }
}

customElements.define(SkillsAppElement.is, SkillsAppElement);
