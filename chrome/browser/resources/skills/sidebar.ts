// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './sidebar.css.js';
import {getHtml} from './sidebar.html.js';

interface MenuItem {
  icon: string;
  name: string;
  page: Page;
}

export enum Page {
  USER_SKILLS = 'yourSkills',
  DISCOVER_SKILLS = 'browse',
}

export class SkillsSidebarElement extends CrLitElement {
  static get is() {
    return 'skills-sidebar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selectedPage: {type: String},
    };
  }

  readonly menuItems: MenuItem[] = [
    {
      icon: 'skills:bolt',
      name: loadTimeData.getString('userSkillsTitle'),
      page: Page.USER_SKILLS,
    },
    {
      icon: 'skills:explore',
      name: loadTimeData.getString('browseSkillsTitle'),
      page: Page.DISCOVER_SKILLS,
    },
  ];

  protected accessor selectedPage: Page = Page.USER_SKILLS;

  protected onIronActivate_(e: CustomEvent<{item: HTMLAnchorElement}>): void {
    const newUrl = new URL(e.detail.item.href);
    this.fire('route-click', {path: newUrl.pathname});
  }

  // Prevent clicks on sidebar items from navigating and reloading the page.
  // onMenuItemSelect_() handles loading new pages.
  protected onMenuItemClick_(e: Event) {
    e.preventDefault();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'skills-sidebar': SkillsSidebarElement;
  }
}

customElements.define(SkillsSidebarElement.is, SkillsSidebarElement);
