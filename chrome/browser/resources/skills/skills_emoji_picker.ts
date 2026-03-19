// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {debounceEnd} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {emojiData as rawEmojiData} from './emoji_data.js';
import {getCss} from './skills_emoji_picker.css.js';
import {getHtml} from './skills_emoji_picker.html.js';

export interface Emoji {
  emoji: string;
  name: string;
  shortcodes: string[];
  category: string;
}

interface EmojiInstance {
  base: number[];
  shortcodes: string[];
}

interface EmojiGroup {
  group: string;
  emoji: EmojiInstance[];
}

interface ProcessedData {
  categoryMap: Map<string, Emoji[]>;
}

function processEmojiData(data: EmojiGroup[]): ProcessedData {
  const categoryMap = new Map<string, Emoji[]>();

  data.forEach(categoryGroup => {
    const categoryEmojis: Emoji[] = [];
    categoryGroup.emoji.forEach(emoji => {
      categoryEmojis.push({
        emoji: String.fromCodePoint(...emoji.base),
        name: emoji.shortcodes[0] ?
            emoji.shortcodes[0].replace(/:/g, '').replace(/-/g, ' ') :
            '',
        shortcodes: emoji.shortcodes,
        category: categoryGroup.group,
      });
    });
    categoryMap.set(categoryGroup.group, categoryEmojis);
  });

  return {categoryMap};
}

let defaultProcessedData: ProcessedData|null = null;
function getDefaultProcessedData(): ProcessedData {
  if (!defaultProcessedData) {
    defaultProcessedData =
        processEmojiData(rawEmojiData as unknown as EmojiGroup[]);
  }
  return defaultProcessedData;
}

export interface SkillsEmojiPickerElement {
  $: {
    container: HTMLElement,
    emojiGrid: HTMLElement,
    searchContainer: HTMLElement,
    searchInput: CrInputElement,
  };
}

export class SkillsEmojiPickerElement extends CrLitElement {
  static get is() {
    return 'skills-emoji-picker';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      searchQuery_: {type: String},
      groupedEmojis_: {type: Object},
    };
  }

  protected accessor searchQuery_: string = '';
  protected accessor groupedEmojis_: Map<string, Emoji[]> = new Map();

  private categoryMap_: Map<string, Emoji[]> = new Map();
  private debouncedUpdateGroups_: () => void;
  private documentClickListener_: (event: MouseEvent) => void;
  private searchDebounceDelayMs_: number = 200;

  constructor() {
    super();
    this.documentClickListener_ = this.onOutsideClick_.bind(this);
    this.debouncedUpdateGroups_ =
        debounceEnd(() => this.updateGroups_(), this.searchDebounceDelayMs_);
  }

  override connectedCallback() {
    super.connectedCallback();
    if (this.categoryMap_.size === 0) {
      const processed = getDefaultProcessedData();
      this.categoryMap_ = processed.categoryMap;
      this.updateGroups_();
    }
    requestAnimationFrame(() => {
      if (this.isConnected) {
        document.addEventListener('click', this.documentClickListener_);
      }
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    document.removeEventListener('click', this.documentClickListener_);
  }

  protected override firstUpdated() {
    this.$.searchInput.focus();
  }

  setSearchDebounceDelayMsForTesting(delay: number) {
    this.searchDebounceDelayMs_ = delay;
    this.debouncedUpdateGroups_ =
        debounceEnd(() => this.updateGroups_(), this.searchDebounceDelayMs_);
  }

  setEmojiData(value: EmojiGroup[]) {
    const processed = processEmojiData(value);
    this.categoryMap_ = processed.categoryMap;
    this.updateGroups_();
  }

  private onOutsideClick_(event: MouseEvent) {
    if (!event.composedPath().includes(this)) {
      this.fire('picker-close');
    }
  }

  private updateGroups_() {
    if (!this.searchQuery_) {
      this.groupedEmojis_ = this.categoryMap_;
      return;
    }

    const query = this.searchQuery_.toLowerCase();
    const groups = new Map<string, Emoji[]>();

    for (const [category, emojis] of this.categoryMap_) {
      const filtered = emojis.filter(
          emoji => emoji.name.toLowerCase().includes(query) ||
              emoji.shortcodes.some(s => s.toLowerCase().includes(query)));
      if (filtered.length > 0) {
        groups.set(category, filtered);
      }
    }
    this.groupedEmojis_ = groups;
  }

  protected onSearchInputValueChanged_(event: CustomEvent<{value: string}>) {
    this.searchQuery_ = event.detail.value;
    this.debouncedUpdateGroups_();
  }

  protected onEmojiClick_(event: Event) {
    const emoji = (event.currentTarget as HTMLElement).dataset['emoji']!;
    this.fire('emoji-selected', {emoji});
  }

  protected onKeydown_(event: KeyboardEvent) {
    if (event.key === 'Escape') {
      event.stopPropagation();
      event.preventDefault();
      this.fire('picker-close');
      return;
    }

    const buttons = Array.from(
        this.shadowRoot.querySelectorAll<HTMLButtonElement>('.emoji-button'));
    if (buttons.length === 0) {
      return;
    }

    const currentIndex =
        buttons.indexOf(this.shadowRoot.activeElement as HTMLButtonElement);
    let nextIndex = -1;

    if (currentIndex === -1) {
      if (event.key === 'ArrowDown' || event.key === 'ArrowRight') {
        nextIndex = 0;
      }
    } else {
      switch (event.key) {
        case 'ArrowLeft':
        case 'ArrowUp':
          nextIndex = Math.max(0, currentIndex - 1);
          break;
        case 'ArrowRight':
        case 'ArrowDown':
          nextIndex = Math.min(buttons.length - 1, currentIndex + 1);
          break;
        default:
          break;
      }
    }

    const nextButton = buttons[nextIndex];
    if (nextButton) {
      event.preventDefault();
      nextButton.focus();
      nextButton.scrollIntoView({block: 'nearest'});
    }
  }

  protected getEmojiCategories_() {
    return Array.from(this.groupedEmojis_.keys());
  }

  protected getEmojisByCategory_(category: string) {
    return this.groupedEmojis_.get(category) || [];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'skills-emoji-picker': SkillsEmojiPickerElement;
  }
}

customElements.define(SkillsEmojiPickerElement.is, SkillsEmojiPickerElement);
