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
import {isActivationKey, isBackwardArrow, isForwardArrow, isVerticalArrow} from '../shared/keyboard_util.js';

import {getCss} from './settings_menu.css.js';
import {getHtml} from './settings_menu.html.js';

// Delay, in ms, between when menus are selected or moused over and the menu
// appears. It mirrors the value in ui/views/controls/menu/menu_config.h
export const MENU_SHOW_DELAY_MS = 400;
// Delay, in ms, between when a submenu is shown and when hovering should
// trigger opening another submenu. This is used to prevent accidental
// opens of submenus.
export const SUBMENU_SHOW_DELAY_MS = 800;

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
  showSeparator?: boolean;
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
    showSeparator: true,
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
  [SettingsOption.PINNED_TO_TOOLBAR]: {
    id: SettingsOption.PINNED_TO_TOOLBAR,
    icon: 'read-anything:pin',
    title: 'pinLabel',
    itemType: SettingsItemType.TOGGLE,
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

export const KEYBOARD_NAV_CLASS = 'keyboard-nav';

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

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isImmersiveMode: {type: Boolean},
      isReadAnythingPinned: {type: Boolean},
      settingsPrefs: {type: Object},
    };
  }

  accessor isImmersiveMode: boolean = false;
  accessor isReadAnythingPinned: boolean = false;
  accessor settingsPrefs: SettingsPrefs = DEFAULT_SETTINGS;

  protected options_: SettingsItem[] = [];
  private currentOpenId_: string|null = null;
  private interceptedEvents_: string[] =
      ['click', 'pointerdown', 'pointermove'];
  private openTimer_: number|null = null;
  private closeTimer_: number|null = null;
  // Used to prevent accidental triggers of other submenus after a submenu
  // has recently been opened.
  private lastMenuOpenTime_: number = 0;
  private pointerEventCallback_: (e: Event) => void = () => {};
  private keyDownCallback_: (e: KeyboardEvent) => void = () => {};

  // Used to check if focus is currently on the PreviewPlayButton of the
  // VOICE_SELECTION submenu.
  private isOnPreviewPlayButton = false;

  override connectedCallback() {
    super.connectedCallback();
    this.pointerEventCallback_ = this.onPointerEvent_.bind(this);
    this.keyDownCallback_ = this.onKeyDown_.bind(this);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('settingsPrefs') ||
        changedProperties.has('isImmersiveMode') ||
        changedProperties.has('isReadAnythingPinned')) {
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

    optionIDs.push(SettingsOption.PRESENTATION);

    // If Readability is enabled but ReadabilityWithLinks is not enabled,
    // don't show the links toggle.
    if (!chrome.readingMode.isReadabilityEnabled ||
        chrome.readingMode.isReadabilityWithLinksEnabled) {
      optionIDs = optionIDs.concat([SettingsOption.LINKS]);
    }

    if (chrome.readingMode.imagesFeatureEnabled) {
      optionIDs.push(SettingsOption.IMAGES);
    }

    if (this.isImmersiveMode) {
      optionIDs.push(SettingsOption.PINNED_TO_TOOLBAR);
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

      if (id === SettingsOption.PINNED_TO_TOOLBAR) {
        enabled = this.isReadAnythingPinned;
        ariaLabel = this.getPinItemLabels();
      }

      return {
        ...original,
        id,
        title,
        ariaLabel,
        enabled,
      };
    });

    // There should be a separator between the menu items and the toggle items.
    // The base combination is on the Links toggle, but that's not
    // always the first toggle.
    this.options_.forEach(option => {
      if (option.itemType === SettingsItemType.TOGGLE) {
        option.showSeparator = false;
      }
    });

    // Add the separator to the first toggle.
    const firstToggle =
        this.options_.find(item => item.itemType === SettingsItemType.TOGGLE);
    if (firstToggle) {
      firstToggle.showSeparator = true;
    }
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

  private getPinItemLabels() {
    if (this.isReadAnythingPinned) {
      return loadTimeData.getString('enablePinLabel');
    }

    return loadTimeData.getString('disablePinLabel');
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

    this.clearTimers_();

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
    this.lastMenuOpenTime_ = Date.now();
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
    } else if (item.id === SettingsOption.PINNED_TO_TOOLBAR) {
      chrome.readingMode.togglePinState();
      chrome.readingMode.sendPinStateRequest();
    }

    this.requestUpdate();
  }

  protected onPointerenter_(e: PointerEvent) {
    this.clearTimers_();

    const currentTarget = e.currentTarget as HTMLElement;
    if (!currentTarget) {
      return;
    }

    const activeItems =
        this.shadowRoot?.querySelectorAll<HTMLElement>('.active');
    for (const activeItem of activeItems) {
      activeItem.classList.remove('active');
    }

    const index = Number.parseInt(currentTarget.dataset['index']!);
    const item = this.options_[index];
    if (!item || item.itemType === SettingsItemType.TOGGLE) {
      return;
    }

    const newMenuId = item.id;
    if (this.currentOpenId_ === newMenuId) {
      return;
    }

    // If the menu was just opened, add a delay to prevent accidental switching.
    const timeSinceLastOpen = Date.now() - this.lastMenuOpenTime_;
    const delay = timeSinceLastOpen < SUBMENU_SHOW_DELAY_MS ?
        SUBMENU_SHOW_DELAY_MS :
        MENU_SHOW_DELAY_MS;

    this.openTimer_ = window.setTimeout(() => {
      this.fire(ToolbarEvent.OPEN_SETTINGS_SUBMENU, {
        id: newMenuId,
        previousId: this.currentOpenId_,
        target: currentTarget,
      });
      this.currentOpenId_ = newMenuId;
      this.lastMenuOpenTime_ = Date.now();
    }, delay);
  }

  protected onPointerleave_() {
    // Clear the open timer so that submenus aren't opened after the cursor
    // stops hovering.
    this.clearOpenTimer_();
    this.startCloseTimer_();
  }

  private startCloseTimer_() {
    if (this.closeTimer_) {
      return;
    }

    this.closeTimer_ = window.setTimeout(() => {
      this.fire(
          ToolbarEvent.OPEN_SETTINGS_SUBMENU,
          {id: null, previousId: this.currentOpenId_, target: null});
      this.closeTimer_ = null;
      this.currentOpenId_ = null;
    }, MENU_SHOW_DELAY_MS);
  }

  private clearTimers_() {
    this.clearOpenTimer_();
    this.clearCloseTimer_();
  }

  private clearOpenTimer_() {
    if (this.openTimer_) {
      clearTimeout(this.openTimer_);
      this.openTimer_ = null;
    }
  }

  private clearCloseTimer_() {
    if (this.closeTimer_) {
      clearTimeout(this.closeTimer_);
      this.closeTimer_ = null;
    }
  }

  open(anchor: HTMLElement) {
    openMenu(this.$.lazyMenu.get(), anchor);
    window.addEventListener('keydown', this.keyDownCallback_, {capture: true});
    this.interceptedEvents_.forEach(eventType => {
      window.addEventListener(
          eventType, this.pointerEventCallback_, {capture: true});
    });
    this.fire(ToolbarEvent.SETTINGS_OPENED);
  }

  protected onClose_() {
    this.close();
  }

  close() {
    this.clearTimers_();
    this.$.lazyMenu.get().close();
    window.removeEventListener(
        'keydown', this.keyDownCallback_, {capture: true});
    this.interceptedEvents_.forEach(eventType => {
      window.removeEventListener(
          eventType, this.pointerEventCallback_, {capture: true});
    });
    this.currentOpenId_ = null;
    this.fire(ToolbarEvent.SETTINGS_CLOSED);
  }

  // Immersive design uses non-modal menus to support submenus. Since non-modal
  // menus don't automatically block background interactions, this handler
  // manually intercepts events to:
  // 1. Prevent interactions with elements outside the menu.
  // 2. Close the menu when clicking outside (simulating modal behavior).
  private onPointerEvent_(e: Event) {
    // Whenever the user moves or clicks the mouse, reset state for
    // isOnPreviewPlayButton.
    this.isOnPreviewPlayButton = false;
    if (e.type === 'pointermove') {
      const menu = this.$.lazyMenu.get();
      if (menu.classList.contains(KEYBOARD_NAV_CLASS)) {
        menu.classList.remove(KEYBOARD_NAV_CLASS);
      }
    }

    // TODO (crbug.com/470381025): Fix cursor style when settings menu is open
    let isInsideSubmenu = false;
    let isInsideMain = false;
    const path = e.composedPath();
    for (const el of path) {
      if (!(el instanceof HTMLElement)) {
        continue;
      }

      isInsideSubmenu = el.classList.contains('settings-submenu');
      isInsideMain = el.id === 'settingsMenu';

      if (isInsideSubmenu || isInsideMain) {
        break;
      }
    }

    // When the user moves out of an item, we start a close timer that
    // closes any opened submenu. If the user moves from an item into a submenu
    // we should cancel the close timer, as the user intentionally moved into
    // the submenu.
    if (e.type === 'pointermove' && isInsideSubmenu) {
      if (this.currentOpenId_) {
        const activeItem = this.shadowRoot?.querySelector<HTMLElement>(
            `#${this.currentOpenId_}`);
        if (activeItem) {
          activeItem.classList.add('active');
        }
      }
      this.clearCloseTimer_();
    }

    if (isInsideSubmenu || isInsideMain) {
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

  private onKeyDown_(e: KeyboardEvent) {
    const key = e.key;
    if (isVerticalArrow(key) || isForwardArrow(key) || isActivationKey(key)) {
      this.clearOpenTimer_();

      const menu = this.$.lazyMenu.get();
      if (!menu.classList.contains(KEYBOARD_NAV_CLASS)) {
        menu.classList.add(KEYBOARD_NAV_CLASS);
      }
    }

    // Handle Escape or horizontal backward navigation to close the currently
    // open submenu. We consume the event to stop further propagation.
    if (this.currentOpenId_ &&
        (key === 'Escape' || (isBackwardArrow(key) && !isVerticalArrow(key)))) {
      // if backward horizontal arrow is pressed and focus is on the preview
      // play button, let the VOICE_SELECTION submenu handle backwards arrow.
      if (key !== 'Escape' && this.isOnPreviewPlayButton) {
        this.isOnPreviewPlayButton = false;
        return;
      }
      e.stopPropagation();
      e.preventDefault();
      this.fire(
          ToolbarEvent.CLOSE_SUBMENU_REQUESTED,
          {previousId: this.currentOpenId_});
      this.currentOpenId_ = null;
      return;
    }

    if (isForwardArrow(key) && !isVerticalArrow(key)) {
      const focused = this.shadowRoot.activeElement as HTMLElement;
      // If focus is null, do nothing.
      if (!focused) {
        return;
      }
      // If forward-horizontal arrow is pressed and we are on VOICE_SELECTION
      // submenu, set isOnPreviewPlayButton to true to indicate current focus.
      if (this.currentOpenId_ &&
          this.currentOpenId_ === SettingsOption.VOICE_SELECTION &&
          !focused?.classList.contains('menu-row')) {
        this.isOnPreviewPlayButton = true;
        return;
      }
      e.stopPropagation();
      e.preventDefault();

      const index = Number.parseInt(focused.dataset['index']!);
      const item = this.options_[index];
      if (!item || item.itemType === SettingsItemType.TOGGLE) {
        return;
      }

      focused.click();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-menu': SettingsMenuElement;
  }
}

customElements.define(SettingsMenuElement.is, SettingsMenuElement);
