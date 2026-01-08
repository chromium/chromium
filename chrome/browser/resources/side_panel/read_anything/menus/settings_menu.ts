// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement, type PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsPrefs} from '../content/read_anything_types.js';
import {DEFAULT_SETTINGS, SettingsOption, ToolbarEvent} from '../content/read_anything_types.js';
import {openMenu} from '../shared/common.js';

import {getCss} from './settings_menu.css.js';
import {getHtml} from './settings_menu.html.js';

export enum SettingsItemType {
  MENU = 1,
  TOGGLE = 2,
}

interface SettingsItem {
  id: SettingsOption;
  icon: string;
  title: string;
  itemType: SettingsItemType;
  // Whether the toggle is checked. Only used when itemType is TOGGLE
  enabled?: boolean;
  // Needed when the aria label should be different from the title
  ariaLabel?: string;
  className?: string;
}

const MENU_ITEM_DATA: Record<SettingsOption, SettingsItem> = {
  [SettingsOption.COLOR]: {
    id: SettingsOption.COLOR,
    icon: 'read-anything:color',
    title: 'themeTitle',
    itemType: SettingsItemType.MENU,
  },
  [SettingsOption.FONT]: {
    id: SettingsOption.FONT,
    icon: 'read-anything:font',
    title: 'fontNameTitle',
    itemType: SettingsItemType.MENU,
  },
  [SettingsOption.FONT_SIZE]: {
    id: SettingsOption.FONT_SIZE,
    icon: 'read-anything:font-size',
    title: 'fontSizeTitle',
    itemType: SettingsItemType.MENU,
  },
  [SettingsOption.IMAGES]: {
    id: SettingsOption.IMAGES,
    icon: 'read-anything:images-enabled',
    title: 'imagesLabel',
    itemType: SettingsItemType.TOGGLE,
  },
  [SettingsOption.LINKS]: {
    id: SettingsOption.LINKS,
    icon: 'read-anything:links-enabled',
    title: 'linksLabel',
    itemType: SettingsItemType.TOGGLE,
    className: 'hr',
  },
  [SettingsOption.LINE_SPACING]: {
    id: SettingsOption.LINE_SPACING,
    icon: 'read-anything:line-spacing',
    title: 'lineSpacingTitle',
    itemType: SettingsItemType.MENU,
  },
  [SettingsOption.LETTER_SPACING]: {
    id: SettingsOption.LETTER_SPACING,
    icon: 'read-anything:letter-spacing',
    title: 'letterSpacingTitle',
    itemType: SettingsItemType.MENU,
  },
  [SettingsOption.LINE_FOCUS]: {
    id: SettingsOption.LINE_FOCUS,
    icon: 'read-anything:line-focus',
    title: 'lineFocusLabel',
    itemType: SettingsItemType.MENU,
  },
  [SettingsOption.PRESENTATION]: {
    id: SettingsOption.PRESENTATION,
    icon: 'read-anything:view',
    title: 'viewLabel',
    itemType: SettingsItemType.MENU,
  },
  [SettingsOption.VOICE_SELECTION]: {
    id: SettingsOption.VOICE_SELECTION,
    icon: 'read-anything:voice-selection',
    title: 'voiceSelectionLabel',
    itemType: SettingsItemType.MENU,
  },
  [SettingsOption.VOICE_HIGHLIGHT]: {
    id: SettingsOption.VOICE_HIGHLIGHT,
    icon: 'read-anything:highlight-on',
    title: 'voiceHighlightLabel',
    itemType: SettingsItemType.MENU,
  },
};

export interface SettingsMenuElement {
  $: {
    lazyMenu: CrLazyRenderLitElement<CrActionMenuElement>,
  };
}

const SettingsMenuElementBase = WebUiListenerMixinLit(CrLitElement);

export class SettingsMenuElement extends SettingsMenuElementBase {
  static get is() {
    return 'settings-menu';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      settingsPrefs: {type: Object},
    };
  }

  accessor settingsPrefs: SettingsPrefs = DEFAULT_SETTINGS;

  protected options_: SettingsItem[] = [];
  private currentOpenId_: string|null = null;
  private blockedEvents_: string[] = ['click', 'pointerdown'];
  private pointerEventCallback_: (e: Event) => void = () => {};

  override connectedCallback() {
    super.connectedCallback();
    this.pointerEventCallback_ = this.onPointerEvent_.bind(this);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('settingsPrefs')) {
      this.initializeMenuOptions_();
    }
  }

  private initializeMenuOptions_() {
    let optionIDs = [
      SettingsOption.COLOR,
      SettingsOption.FONT,
      SettingsOption.LINE_SPACING,
      SettingsOption.LETTER_SPACING,
      SettingsOption.VOICE_SELECTION,
      SettingsOption.VOICE_HIGHLIGHT,
    ];

    if (chrome.readingMode.isLineFocusEnabled) {
      optionIDs.push(SettingsOption.LINE_FOCUS);
    }

    optionIDs =
        optionIDs.concat([SettingsOption.PRESENTATION, SettingsOption.LINKS]);

    if (chrome.readingMode.imagesFeatureEnabled) {
      optionIDs.push(SettingsOption.IMAGES);
    }

    this.options_ = optionIDs.map(id => {
      const original = MENU_ITEM_DATA[id];
      const title = loadTimeData.getString(original.title);
      let ariaLabel = title;
      let enabled = false;

      if (id === SettingsOption.IMAGES) {
        enabled = this.settingsPrefs.imagesEnabled;
        ariaLabel = this.getImageItemLabels();
      }

      if (id === SettingsOption.LINKS) {
        enabled = this.settingsPrefs.linksEnabled;
        ariaLabel = this.getLinkItemLabels();
      }

      return {
        ...original,
        id,
        title,
        ariaLabel,
        enabled,
      };
    });
  }

  private getLinkItemLabels() {
    if (chrome.readingMode.linksEnabled) {
      return loadTimeData.getString('disableLinksLabel');
    }

    return loadTimeData.getString('enableLinksLabel');
  }

  private getImageItemLabels() {
    if (chrome.readingMode.imagesEnabled) {
      return loadTimeData.getString('disableImagesLabel');
    }

    return loadTimeData.getString('enableImagesLabel');
  }

  protected onMenuItemClick_(e: Event) {
    e.stopImmediatePropagation();
    const currentTarget = e.currentTarget as HTMLElement;
    const index = Number.parseInt(currentTarget.dataset['index']!);
    const item = this.options_[index];
    if (!item) {
      return;
    }

    if (item.itemType === SettingsItemType.TOGGLE) {
      this.onToggleMenuItemClick_(item);
      return;
    }

    const newMenuId = item.id;
    if (this.currentOpenId_ === newMenuId) {
      return;
    }

    this.fire(ToolbarEvent.OPEN_SETTINGS_SUBMENU, {
      id: newMenuId,
      previousId: this.currentOpenId_,
      target: currentTarget,
    });
    this.currentOpenId_ = newMenuId;
  }

  private onToggleMenuItemClick_(item: SettingsItem) {
    if (item.itemType !== SettingsItemType.TOGGLE) {
      return;
    }

    if (item.id === SettingsOption.LINKS) {
      chrome.readingMode.onLinksEnabledToggled();
      this.fire(ToolbarEvent.LINKS);
      item.ariaLabel = this.getLinkItemLabels();
      item.enabled = chrome.readingMode.linksEnabled;
    } else if (item.id === SettingsOption.IMAGES) {
      chrome.readingMode.onImagesEnabledToggled();
      this.fire(ToolbarEvent.IMAGES);
      item.ariaLabel = this.getImageItemLabels();
      item.enabled = chrome.readingMode.imagesEnabled;
    }

    this.requestUpdate();
  }

  open(anchor: HTMLElement) {
    // TODO (crbug.com/470379596): Add keyboard navigation to settings menu
    openMenu(this.$.lazyMenu.get(), anchor);
    this.blockedEvents_.forEach(eventType => {
      window.addEventListener(
          eventType, this.pointerEventCallback_, {capture: true});
    });
  }

  close() {
    this.$.lazyMenu.get().close();
    document.body.classList.remove('read-anything-menu-open');
    this.blockedEvents_.forEach(eventType => {
      window.removeEventListener(
          eventType, this.pointerEventCallback_, {capture: true});
    });
    this.currentOpenId_ = null;
  }

  // Immersive design uses non-modal menus to support submenus. Since non-modal
  // menus don't automatically block background interactions, this handler
  // manually intercepts events to:
  // 1. Prevent interactions with elements outside the menu.
  // 2. Close the menu when clicking outside (simulating modal behavior).
  private onPointerEvent_(e: Event) {
    // TODO (crbug.com/470381025): Fix cursor style when settings menu is open
    const path = e.composedPath();
    const isInsideMenu = path.some(
        target =>
            target instanceof Element && (target.tagName === 'CR-ACTION-MENU'));

    if (isInsideMenu) {
      return;
    }

    e.preventDefault();
    e.stopPropagation();
    e.stopImmediatePropagation();


    // We return early to wait for the full 'click' event before closing the
    // menu.
    if (e.type === 'pointerdown') {
      return;
    }

    if (e.type === 'click') {
      this.fire(
          ToolbarEvent.CLOSE_ALL_MENUS, {previousId: this.currentOpenId_});
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-menu': SettingsMenuElement;
  }
}

customElements.define(SettingsMenuElement.is, SettingsMenuElement);
