// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import './emoji_variants.js';

import {assertInstanceof} from 'chrome://resources/js/assert.js';
import {PaperTooltipElement} from 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import {beforeNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {VISUAL_CONTENT_WIDTH} from './constants.js';
import {getTemplate} from './emoji_group.html.js';
import {EmojiImageComponent} from './emoji_image.js';
import {EmojiPickerApiProxy} from './emoji_picker_api_proxy.js';
import {createCustomEvent, EMOJI_CLEAR_RECENTS_CLICK, EMOJI_IMG_BUTTON_CLICK, EMOJI_TEXT_BUTTON_CLICK, EMOJI_VARIANTS_SHOWN, EmojiClearRecentClickEvent, EmojiTextButtonClickEvent} from './events.js';
import {CategoryEnum, EmojiVariants, Gender, PreferenceMapping, Tone} from './types.js';

// Note - grid-layout and flex-layout names are used directly in CSS.
export enum EmojiGroupLayoutType {
  GRID_LAYOUT = 'grid-layout',
  FLEX_LAYOUT = 'flex-layout',
  TWO_COLUMN_LAYOUT = 'two-column-layout',
}

enum SideEnum {
  LEFT = 'left',
  RIGHT = 'right',
}

const DEFAULT_CATEGORY_LAYOUTS = {
  [CategoryEnum.EMOJI]: EmojiGroupLayoutType.GRID_LAYOUT,
  [CategoryEnum.EMOTICON]: EmojiGroupLayoutType.FLEX_LAYOUT,
  [CategoryEnum.SYMBOL]: EmojiGroupLayoutType.GRID_LAYOUT,
  [CategoryEnum.GIF]: EmojiGroupLayoutType.TWO_COLUMN_LAYOUT,
};

export interface EmojiGroupComponent {
  $: {
    tooltip: PaperTooltipElement,
  };
}

export class EmojiGroupComponent extends PolymerElement {
  static get is() {
    return 'emoji-group' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      data: {type: Array, readonly: true},
      group: {type: String, value: null, readonly: true},
      globalTone: {type: Number, value: null, readonly: true},
      globalGender: {type: Number, value: null, readonly: true},
      preferred: {type: Object, value: () => ({})},
      clearable: {type: Boolean, value: false},
      useGroupedPreference: {type: Boolean, value: false},
      category: {
        type: String,
        value: CategoryEnum.EMOJI,
        readonly: true,
      },
      layoutType: {
        type: String,
        value: null,
      },
      showClearRecents: {type: Boolean, value: false},
      focusedEmoji: {type: Object, value: null},
      shownEmojiVariantIndex: {type: Number, value: null},
      isLangEnglish: {type: Boolean, value: false},
      gifSupport: {type: Boolean, value: false},
    };
  }
  data: EmojiVariants[];
  group: string|null;
  private globalTone: Tone|null = null;
  private globalGender: Gender|null = null;
  preferred: PreferenceMapping;
  clearable: boolean;
  useGroupedPreference: boolean;
  category: CategoryEnum;
  layoutType: string|null;
  showClearRecents: boolean;
  private focusedEmoji: EmojiVariants|null;
  private shownEmojiVariantIndex: number|null;
  private isLangEnglish: boolean;
  private gifSupport: boolean;

  constructor() {
    super();

    // TODO(crbug/1227852): Remove after setting arial label to emoji.
    this.isLangEnglish =
        navigator.languages.some(lang => lang.startsWith('en'));

    // Some methods will be passed down to child elements and thus we need
    // `bind(this)`.
    this.onEmojiClick = this.onEmojiClick.bind(this);
    this.showTooltip = this.showTooltip.bind(this);
  }

  /**
   * Handles the click event for show-clear button which results
   * in showing "clear recently used emojis" button.
   */
  onClearClick(ev: Event): void {
    ev.preventDefault();
    ev.stopPropagation();
    this.showClearRecents = true;
  }

  /**
   * Handles the event for clicking on the "clear recently used" button.
   * It makes "show-clear" button disappear and fires an event
   * indicating that the "clear recently used" is clicked.
   */
  private onClearRecentsClick(ev: Event): void {
    ev.preventDefault();
    ev.stopPropagation();
    this.showClearRecents = false;
    this.dispatchEvent(createCustomEvent(
        EMOJI_CLEAR_RECENTS_CLICK, {category: this.category}));
  }

  /**
   * Sets and shows tooltips for an emoji button based on a mouse/focus event.
   * By handling the on-focus and on-mouseenter events using this function,
   * one single paper-tool is reused for all emojis in the emoji-group.
   *
   */
  private showTooltip(event: MouseEvent|FocusEvent): void {
    // Target must always exist since this is triggered by a mouse/focus on an
    // element.
    const emoji = this.findEmojiOfEmojiButton(event.target as HTMLElement);

    // If the event is for an emoji button that is not already
    // focused, then replace the target of paper-tooltip with the new
    // emoji button.
    if (emoji && this.focusedEmoji !== emoji) {
      this.focusedEmoji = emoji;

      // Set the target of paper-tooltip to the focused emoji button.
      // Paper-tooltip will un-listen the events of the previous target and
      // starts listening the events for the focused emoji button to hide and
      // show the tooltip at the right time.
      this.$.tooltip.target = event.target;
      this.$.tooltip.show();
    }
  }

  /**
   * Handles event of clicking on an emoji button. It finds the emoji details
   * for the clicked emoji and fires another event including these the details.
   * Note: Initially, it validates and returns if the event is not for an
   * emoji button.
   *
   */
  private onEmojiClick(event: MouseEvent): void {
    const emoji =
        this.findEmojiOfEmojiButton(event.target as HTMLElement | null);

    // Ensure target is an emoji button.
    if (!emoji) {
      return;
    }

    // Text-based emoji clicked
    if (emoji.base.string) {
      const text = this.getDisplayEmojiForEmoji(emoji.base.string, emoji);

      this.dispatchEvent(createCustomEvent(EMOJI_TEXT_BUTTON_CLICK, {
        name: emoji.base.name,
        category: this.category,
        text,
        baseEmoji: emoji.base.string,
        isVariant: text !== emoji.base.string,
        groupedTone: false,
        groupedGender: false,
        alternates: emoji.alternates ?? [],
      }));
    } else {
      if (emoji.base.visualContent) {
        // Visual-based emoji clicked
        this.dispatchEvent(createCustomEvent(EMOJI_IMG_BUTTON_CLICK, {
          name: emoji.base.name,
          visualContent: emoji.base.visualContent,
          category: this.category,
        }));
      }
    }
  }

  private onHelpClick(): void {
    EmojiPickerApiProxy.getInstance().openHelpCentreArticle();
  }

  /**
   * Handles event of opening context menu of an emoji button. Emoji variants
   * are shown as the context menu.
   * Note: Initially, it validates and returns if the event is not for an
   * emoji button.
   */
  private onEmojiContextMenu(event: Event): void {
    const emoji =
        this.findEmojiOfEmojiButton(event.target as HTMLElement | null);

    // Ensure target is an emoji button.
    if (!emoji) {
      return;
    }
    event.preventDefault();

    assertInstanceof(event.target, HTMLElement);
    const dataIndex = Number(
        // This assert is safe as this can only be triggered via right click on
        // an html element.
        event.target.getAttribute('data-index'));

    // If the variants of the emoji is already shown, then hide it.
    // Otherwise, show the variants if there are some.
    if (emoji.alternates && emoji.alternates.length &&
        dataIndex !== this.shownEmojiVariantIndex) {
      this.shownEmojiVariantIndex = dataIndex;
    } else {
      this.shownEmojiVariantIndex = null;
    }

    // Send event so emoji-picker knows to close other variants.
    // need to defer this until <emoji-variants> is created and sized by
    // Polymer.
    beforeNextRender(this, () => {
      const variants = this.shownEmojiVariantIndex ?
          this.shadowRoot!.getElementById(`emoji-variant-${dataIndex}`) ??
              undefined :
          undefined;

      this.dispatchEvent(createCustomEvent(EMOJI_VARIANTS_SHOWN, {
        owner: this,
        variants: variants,
        baseEmoji: emoji.base.string,
      }));
    });
  }

  /**
   * Returns whether the emoji has variants or not.
   * Does not use `this`.
   */
  private hasVariants(emoji: EmojiVariants): boolean {
    // TODO: b/322909764 - The type of `EmojiVariants.alternates` cannot be
    // null/undefined, so the `!== undefined` check should be redundant. Either
    // add undefined to the type, or remove the below check.
    return emoji.alternates !== undefined && emoji.alternates.length > 0;
  }

  /**
   * Returns HTML class attribute of an emoji groups.
   */
  private getLayoutClassName(
      layoutType: EmojiGroupLayoutType,
      category: CategoryEnum): EmojiGroupLayoutType {
    if (layoutType) {
      return layoutType;
    }

    // If layout type is not provided then choose a default value based
    // on the category.
    return DEFAULT_CATEGORY_LAYOUTS[category] ||
        EmojiGroupLayoutType.GRID_LAYOUT;
  }

  /**
   * Returns the arial label of an emoji.
   */
  private getEmojiAriaLabel(emoji: EmojiVariants): string {
    // TODO(crbug/1227852): Just use emoji as the tooltip once ChromeVox can
    // announce them properly.
    if (emoji.base.string) {
      const emojiLabel = this.isLangEnglish ?
          emoji.base.name :
          (this.getDisplayEmojiForEmoji(emoji.base.string, emoji));
      if (emoji.alternates && emoji.alternates.length > 0) {
        return emojiLabel + ' with variants.';
      } else {
        return emojiLabel ?? '';
      }
    }
    return '';
  }

  /**
   * Returns the character to be shown for the emoji.
   */
  private getDisplayEmojiForEmoji(text: string, emoji: EmojiVariants): string {
    const {alternates, groupedTone, groupedGender} = emoji;
    const individualPreference = this.preferred[text];

    if (!this.useGroupedPreference || !(groupedTone || groupedGender)) {
      return individualPreference ?? text;
    }

    const preference =
        alternates.find(variant => variant.string === individualPreference);
    const tone = this.globalTone ?? preference?.tone ?? Tone.DEFAULT;
    const gender = this.globalGender ?? preference?.gender ?? Gender.DEFAULT;

    const variant = alternates.find(variant => {
      return (variant.tone ?? tone) === tone &&
          (variant.gender ?? gender) === gender;
    });

    return variant?.string ?? text;
  }

  /**
   * Return whether variants of an emoji is visible or not.
   */
  private isEmojiVariantVisible(
      emojiIndex: number, shownEmojiVariantIndex: number): boolean {
    return emojiIndex === shownEmojiVariantIndex;
  }

  /**
   * Hides emoji variants if any is visible.
   */
  hideEmojiVariants(): void {
    this.shownEmojiVariantIndex = null;
  }

  /**
   * Finds emoji details for an HTML button based on the attribute of
   * data-index and event target information.
   * The result will be null if the target is not for a button element
   * or it does not have data-index attribute.
   */
  private findEmojiOfEmojiButton(target: HTMLElement|null): EmojiVariants
      |undefined {
    const dataIndex = target?.getAttribute('data-index');

    if (!(target?.nodeName === 'BUTTON' || target?.nodeName === 'IMG') ||
        !dataIndex) {
      return undefined;
    }
    return this.data[Number(dataIndex)];
  }

  /**
   * Returns the first emoji button in the group.
   */
  firstEmojiButton(): HTMLElement|null {
    // !. is safe for shadowRoot as it always exists
    const elem: HTMLElement|null = this.shadowRoot!.querySelector<HTMLElement>('.emoji-button, emoji-image');
    if (elem instanceof EmojiImageComponent) {
      return elem.shadowRoot!.querySelector('img');
    }
    return elem;
  }

  /**
   * Returns whether the given element group is visual or not.
   */
  isVisual(category: CategoryEnum): boolean {
    return category === CategoryEnum.GIF;
  }

  /**
   * Returns whether any emoji in the array has variants or not.
   */
  private hasAnyVariants(data: EmojiVariants[]): boolean {
    // `hasVariants` does not use `this`, so there is no need to bind `this`
    // here.
    return data.some(this.hasVariants);
  }

  /**
   * Filters visual content to be displayed in the given column based on '
   * the height of the given column.
   */
  filterColumn(
      data: EmojiVariants[], columnSide: SideEnum,
      _dataLength: number): EmojiVariants[] {
    let leftColHeight = 0;
    let rightColHeight = 0;

    const colData = data.filter((item) => {
      if (item.base.visualContent) {
        const contentHeight = item.base.visualContent.previewSize.height *
            VISUAL_CONTENT_WIDTH / item.base.visualContent.previewSize.width;

        // Filter visual content to be displayed in the given column if it's
        // currently the shortest
        if (leftColHeight <= rightColHeight) {
          leftColHeight += contentHeight;
          return columnSide === SideEnum.LEFT;
        } else {
          rightColHeight += contentHeight;
          return columnSide === SideEnum.RIGHT;
        }
      }

      return false;
    });

    return colData;
  }

  /**
   * Returns the index of a visual based EmojiVariant.
   */
  getIndex(item: EmojiVariants): number {
    return this.data.indexOf(item);
  }

  formatCategory(category: CategoryEnum): string {
    return category === CategoryEnum.GIF ? 'GIF' : category;
  }

  getMoreOptionsAriaLabel(gifSupport: boolean): string|undefined {
    // TODO(b/281609806): Remove this condition once GIF support is fully
    // launched; make sure related node finder in tast test is updated before
    // removing this condition.
    return gifSupport ? 'More options' : undefined;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EmojiGroupComponent.is]: EmojiGroupComponent;
  }
  interface HTMLElementEventMap {
    [EMOJI_CLEAR_RECENTS_CLICK]: EmojiClearRecentClickEvent;
    [EMOJI_TEXT_BUTTON_CLICK]: EmojiTextButtonClickEvent;
  }
}

customElements.define(EmojiGroupComponent.is, EmojiGroupComponent);
