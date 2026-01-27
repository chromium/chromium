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
import './sidebar.js';

import type {CrDrawerElement} from '//resources/cr_elements/cr_drawer/cr_drawer.js';
import type {CrToolbarElement} from '//resources/cr_elements/cr_toolbar/cr_toolbar.js';
import {CrRouter} from '//resources/js/cr_router.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {SkillsSidebarElement} from './sidebar.js';
import {Page} from './sidebar.js';
import type {UserSkillsPageElement} from './user_skills_page.js';

export interface SkillsAppElement {
  $: {
    menu: SkillsSidebarElement,
    toolbar: CrToolbarElement,
    userSkillsPage: UserSkillsPageElement,
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
    };
  }

  protected accessor selectedPage_: Page = Page.USER_SKILLS;
  protected accessor narrow_: boolean = false;
  protected accessor isDrawerOpen_: boolean = false;
  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();
    const router = CrRouter.getInstance();
    // Initial load.
    this.onPathChanged_(router.getPath());
    // Listen for path changes, only triggers via popstate.
    this.eventTracker_.add(
        router, 'cr-router-path-changed',
        (e: Event) => this.onPathChanged_((e as CustomEvent<string>).detail));
    // Event-driven navigation changes.
    this.eventTracker_.add(document, 'navigate-to', (e: Event) => {
      const detail = (e as CustomEvent<{path: string}>).detail;
      this.onPathChanged_(detail.path);
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  // Called when the page is narrow & the menu button appears.
  // Clicking it should open a cr-drawer.
  protected onMenuButtonClick_() {
    this.$.drawer.openDrawer();
    this.isDrawerOpen_ = this.$.drawer.open;
  }

  // Called whenever the text in the search input field changes.
  protected onSearchChanged_() {
    // TODO(b/475604659): Implement this.
  }

  // Called whenever the browser window drops below the defined
  // narrow-threshold.
  protected onNarrowChanged_(e: CustomEvent<{value: boolean}>) {
    this.narrow_ = e.detail.value;
    if (this.isDrawerOpen_ && !this.narrow_) {
      this.$.drawer.close();
    }
  }

  protected onDrawerClose_() {
    this.isDrawerOpen_ = this.$.drawer.open;
  }

  private onPathChanged_(newPath: string) {
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
