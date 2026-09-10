// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './topic_card.css.js';
import {getHtml} from './topic_card.html.js';

export interface TopicItem {
  id: string;
  title: string;
  description: string;
  icon?: string;
  backgroundColor?: string;
  destinationUrl?: string;
}

export type BadgeShape = 'cloud' | 'flower' | 'circle' | 'diamond';

const BADGE_SHAPES: readonly BadgeShape[] = [
  'cloud',
  'flower',
  'circle',
  'diamond',
];

const DEFAULT_BACKGROUND_COLOR =
    'var(--topic-card-fallback-bg, var(--google-blue-100))';

const DEFAULT_ICON = 'cr:insert-drive-file';

const FLOWER_PATH =
    'M28 2C31.5 2 34 5.5 37.5 6.5C41 7.5 44.5 7 47.5 9.5C50.5 12 50.5 15.5 ' +
    '52.5 18.5C54.5 21.5 56 24.5 56 28C56 31.5 54.5 34.5 52.5 37.5C50.5 40.5 ' +
    '50.5 44 47.5 46.5C44.5 49 41 48.5 37.5 49.5C34 50.5 31.5 54 28 54C24.5 ' +
    '54 22 50.5 18.5 49.5C15 48.5 11.5 49 8.5 46.5C5.5 44 5.5 40.5 3.5 ' +
    '37.5C1.5 34.5 0 31.5 0 28C0 24.5 1.5 21.5 3.5 18.5C5.5 15.5 5.5 12 8.5 ' +
    '9.5C11.5 7 15 7.5 18.5 6.5C22 5.5 24.5 2 28 2Z';

const CLOUD_PATH =
    'M28 2C34.5 2 39 6.5 43.5 10.5C47.5 14.5 54 19 54 28C54 37 47.5 41.5 ' +
    '43.5 45.5C39 49.5 34.5 54 28 54C21.5 54 17 49.5 12.5 45.5C8.5 41.5 2 ' +
    '37 2 28C2 19 8.5 14.5 12.5 10.5C17 6.5 21.5 2 28 2Z';

// Cycles through badge shapes by index rotation.
function getBadgeShapeForIndex(index: number): BadgeShape {
  return BADGE_SHAPES[index % BADGE_SHAPES.length]!;
}

// TODO(crbug.com/558572977): Use internationalized strings once GRD strings are added.
export class TopicCardElement extends CrLitElement {
  static get is() {
    return 'topic-card';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      topic: {type: Object},
      index: {type: Number},
    };
  }

  accessor topic: TopicItem|null = null;
  accessor index: number = 0;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('topic')) {
      this.toggleAttribute('hidden', !this.hasContent_());
    }
  }

  protected hasContent_(): boolean {
    return Boolean(
        this.topic &&
        (this.topic.title?.trim() || this.topic.description?.trim()));
  }

  protected getBadgeShape_(): BadgeShape {
    return getBadgeShapeForIndex(this.index);
  }

  protected getBackgroundColor_(): string {
    return this.topic?.backgroundColor || DEFAULT_BACKGROUND_COLOR;
  }

  protected getFlowerPath_(): string {
    return FLOWER_PATH;
  }

  protected getCloudPath_(): string {
    return CLOUD_PATH;
  }

  protected getIcon_(): string {
    return this.topic?.icon || DEFAULT_ICON;
  }

  // Assumes icon strings containing a colon (e.g. 'cr:insert-drive-file')
  // are cr-icon iconset identifiers, while others are treated as text/emojis.
  protected isCrIcon_(): boolean {
    return this.getIcon_().includes(':');
  }

  protected getActionAriaLabel_(): string {
    return this.topic?.title?.trim() ?
        `Jump back in to ${this.topic.title}` :
        'Jump back in';
  }

  protected onJumpBackInClick_() {
    if (!this.topic) {
      return;
    }
    this.fire('jump-back-in', {topic: this.topic});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'topic-card': TopicCardElement;
  }
}

customElements.define(TopicCardElement.is, TopicCardElement);
