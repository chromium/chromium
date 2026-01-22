// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import '//resources/cr_elements/cr_page_selector/cr_page_selector.js';
import '//resources/cr_elements/cr_toolbar/cr_toolbar.js';
import './icons.html.js';
import './user_skills_page.js';
import './discover_skills_page.js';

import type {CrMenuSelector} from '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import {CrRouter} from '//resources/js/cr_router.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

interface MenuItem {
  icon: string;
  name: string;
  page: Page;
}

export enum Page {
  USER_SKILLS = 'user-skills',
  DISCOVER_SKILLS = 'discover-skills',
}

export interface SkillsAppElement {
  $: {
    menu: CrMenuSelector,
    toolbar: CrToolbarElement,
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
      menuItems_: {type: Array},
      selectedPage_: {type: String},
      narrow_: {type: Boolean},
    };
  }

  // TODO(crbug.com/475607224): Instead of hardcoding, add resource strings for
  // the name.
  protected accessor selectedPage_: Page = Page.USER_SKILLS;
  protected accessor menuItems_: MenuItem[] = [
    {
      icon: 'skills:bolt',
      name: 'Your skills',
      page: Page.USER_SKILLS,
    },
    {
      icon: 'skills:explore',
      name: 'Discover skills',
      page: Page.DISCOVER_SKILLS,
    },
  ];
  protected accessor narrow_: boolean = false;

  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();
    const router = CrRouter.getInstance();
    // Initial load.
    this.onPathChanged_(router.getPath());
    // Listen for path changes.
    this.eventTracker_.add(
        router, 'cr-router-path-changed',
        (e: Event) => this.onPathChanged_((e as CustomEvent<string>).detail));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  // Called when the page is narrow & the menu button appears.
  // Clicking it should open a cr-drawer.
  protected onMenuButtonClick_() {
    // TODO(crbug.com/476409946): Implement this.
  }

  // Called whenever the text in the search input field changes.
  protected onSearchChanged_() {
    // TODO(crbug.com/475604659): Implement this.
  }

  // Called whenever the browser window drops below the defined
  // narrow-threshold.
  protected onNarrowChanged_(e: CustomEvent<{value: boolean}>) {
    this.narrow_ = e.detail.value;
    // TODO(crbug.com/476409946): Add additional impl here.
  }

  protected onMenuItemSelect_(e: CustomEvent<{item: HTMLAnchorElement}>): void {
    const newUrl = new URL(e.detail.item.href);
    CrRouter.getInstance().setPath(newUrl.pathname);
  }

  // Prevent clicks on sidebar items from navigating and reloading the page.
  // onMenuItemSelect_() handles loading new pages.
  protected onMenuItemClick_(e: Event) {
    e.preventDefault();
  }

  private onPathChanged_(newPath: string) {
    const path = newPath.substring(1);
    let menuItem = this.menuItems_.find(item => item.page === path);
    // If the menu item isn't found, replace the undefined path with the
    // first menu item.
    if (!menuItem) {
      window.history.replaceState(
          undefined, '', '/' + this.menuItems_[0]!.page);
      menuItem = this.menuItems_[0];
    }
    this.$.menu.selected = this.menuItems_.indexOf(menuItem!);
    this.selectedPage_ = menuItem!.page;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'skills-app': SkillsAppElement;
  }
}

customElements.define(SkillsAppElement.is, SkillsAppElement);
