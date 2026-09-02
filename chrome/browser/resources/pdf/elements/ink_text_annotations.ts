// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './ink_text_box.js';

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TextAnnotation, TextBoxRect} from '../constants.js';
import {Ink2Manager} from '../ink2_manager.js';
import type {TextBoxInit} from '../ink2_manager.js';
import {pageToScreenCoordinates} from '../ink_text_annotation_utils.js';
import {hasCtrlModifierOnly} from '../pdf_viewer_utils.js';
import type {Viewport, ViewportRect} from '../viewport.js';

import {getCss} from './ink_text_annotations.css.js';
import {getHtml} from './ink_text_annotations.html.js';
import type {InkTextBoxElement} from './ink_text_box.js';
import {TextBoxState} from './ink_text_box.js';

export interface InkTextAnnotationsElement {
  $: {
    container: HTMLElement,
    textBox: InkTextBoxElement,
  };
}

interface Placeholder {
  screenRect: TextBoxRect|null;
  rotations: number;
  label: string;
}

export class InkTextAnnotationsElement extends CrLitElement {
  static get is() {
    return 'ink-text-annotations';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      viewport: {type: Object},
      activeAnnotation_: {type: Object},
      activePageDimensions_: {type: Object},
      focusedIndex_: {type: Number},
      isPaste_: {type: Boolean},
      placeholders_: {type: Array},
    };
  }

  accessor viewport: Viewport|null = null;
  protected accessor activeAnnotation_: TextAnnotation|null = null;
  protected accessor activePageDimensions_: ViewportRect|null = null;
  protected accessor focusedIndex_: number = -1;
  protected accessor isPaste_: boolean = false;
  protected accessor placeholders_: Placeholder[] = [];
  private annotations_: TextAnnotation[] = [];
  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();

    const manager = Ink2Manager.getInstance();
    this.eventTracker_.add(
        manager, 'annotations-updated', () => this.updateAnnotations_());
    this.eventTracker_.add(
        manager, 'initialize-text-box',
        (e: Event) =>
            this.onInitializeTextBox_((e as CustomEvent<TextBoxInit>).detail));
    this.eventTracker_.add(
        this, 'wheel', (e: Event) => this.onWheel_(e as WheelEvent));
    this.eventTracker_.add(
        document, 'keydown', (e: Event) => this.onKeyDown_(e as KeyboardEvent));
    this.updateAnnotations_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('focusedIndex_')) {
      this.updatePlaceholders_();
    }
  }

  private onKeyDown_(e: KeyboardEvent) {
    if (!hasCtrlModifierOnly(e) || e.key.toLowerCase() !== 'v') {
      return;
    }

    // Ignore if event occurred on a <textarea> or <input> that
    // should handle it (e.g. the ink-text-box textarea)
    const target = e.composedPath()[0] ?? null;
    if (target instanceof HTMLTextAreaElement ||
        target instanceof HTMLInputElement) {
      return;
    }

    this.pasteAnnotation();
  }

  async pasteAnnotation() {
    if (this.activeAnnotation_) {
      await this.$.textBox.commitTextAnnotation();
    }
    Ink2Manager.getInstance().pasteAnnotation();
  }

  viewportChanged() {
    this.updatePlaceholders_();
    this.$.textBox.viewportChanged();
  }

  private updateAnnotations_() {
    const manager = Ink2Manager.getInstance();
    const allAnnotations: TextAnnotation[] = [];
    const sortedPages =
        Array.from(manager.annotations.keys()).sort((a, b) => a - b);

    for (const page of sortedPages) {
      const pageAnnotationsMap = manager.annotations.get(page);
      if (!pageAnnotationsMap || pageAnnotationsMap.size === 0) {
        continue;
      }
      const pageAnnotations = Array.from(pageAnnotationsMap.values());
      // Sort visually: top-to-bottom, then left-to-right
      pageAnnotations.sort((a, b) => {
        if (a.textBoxRect.locationY !== b.textBoxRect.locationY) {
          return a.textBoxRect.locationY - b.textBoxRect.locationY;
        }
        return a.textBoxRect.locationX - b.textBoxRect.locationX;
      });
      allAnnotations.push(...pageAnnotations);
    }
    this.annotations_ = allAnnotations;
    this.updatePlaceholders_();
  }

  private getActiveWindowIndices_(): Set<number> {
    const indices = new Set<number>();
    const total = this.annotations_.length;
    if (total === 0) {
      return indices;
    }

    if (this.focusedIndex_ === -1) {
      // When no placeholder is focused, pre-position the first and last
      // items so tabbing from top or bottom immediately lands on a positioned
      // element.
      indices.add(0);
      indices.add(total - 1);
      return indices;
    }

    if (this.focusedIndex_ > 0) {
      indices.add(this.focusedIndex_ - 1);
    }
    indices.add(this.focusedIndex_);
    if (this.focusedIndex_ < total - 1) {
      indices.add(this.focusedIndex_ + 1);
    }
    return indices;
  }

  private updatePlaceholders_() {
    const viewport = this.viewport;
    assert(viewport);
    const activeIndices = this.getActiveWindowIndices_();
    const clockwiseRotations = viewport.getClockwiseRotations();
    this.placeholders_ = this.annotations_.map((annotation, index) => {
      const screenRect = activeIndices.has(index) ?
          pageToScreenCoordinates(
              annotation.pageIndex, annotation.textBoxRect, viewport) :
          null;
      return {
        screenRect,
        label: annotation.text,
        rotations: (clockwiseRotations + annotation.textOrientation) % 4,
      };
    });
  }

  protected getStyles_(placeholder: Placeholder) {
    if (!placeholder.screenRect) {
      return 'opacity: 0;';
    }
    return `
      --left: ${placeholder.screenRect.locationX}px;
      --top: ${placeholder.screenRect.locationY}px;
      --width: ${placeholder.screenRect.width}px;
      --height: ${placeholder.screenRect.height}px;
    `;
  }

  protected onPlaceholderFocus_(e: FocusEvent) {
    const currentTarget = e.currentTarget as HTMLElement;
    const index = Number(currentTarget.dataset['index']);
    this.focusedIndex_ = index;
    const placeholder = this.placeholders_[index];
    assert(placeholder);
    assert(placeholder.screenRect);

    this.scrollToShowTextBox_(placeholder.screenRect);
  }

  protected onContainerFocusout_(e: FocusEvent) {
    const relatedTarget = e.relatedTarget as Node | null;
    if (!this.$.container.contains(relatedTarget)) {
      this.focusedIndex_ = -1;
    }
  }

  protected async onPlaceholderKeydown_(e: KeyboardEvent) {
    if (e.key !== 'Enter' && e.key !== ' ') {
      return;
    }
    e.preventDefault();
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const annotation = this.annotations_[index];
    assert(annotation);

    if (this.activeAnnotation_) {
      if (this.activeAnnotation_.id === annotation.id) {
        return;
      }
      await this.$.textBox.commitTextAnnotation();
    }

    Ink2Manager.getInstance().activateAnnotationById(annotation.id);
  }

  commitActiveAnnotation(): Promise<void> {
    return this.$.textBox.commitTextAnnotation();
  }

  blurActiveAnnotation() {
    this.$.textBox.blur();
  }

  protected onTextBoxStateChanged_(e: CustomEvent<TextBoxState>) {
    if (e.detail === TextBoxState.INACTIVE) {
      this.isPaste_ = false;
      this.activeAnnotation_ = null;
      this.activePageDimensions_ = null;
      this.focusedIndex_ = -1;
    }
    this.fire('state-changed', e.detail);
  }

  private async onInitializeTextBox_(data: TextBoxInit) {
    if (this.activeAnnotation_) {
      await this.$.textBox.commitTextAnnotation();
    }
    this.isPaste_ = !!data.isPaste;
    this.activeAnnotation_ = data.annotation;
    this.activePageDimensions_ = data.pageDimensions;
  }

  protected onTextboxFocused_(e: CustomEvent<TextBoxRect>) {
    this.scrollToShowTextBox_(e.detail);
  }

  protected getPlaceholderTabIndex_(placeholder: Placeholder): number {
    if (this.activeAnnotation_ || !placeholder.screenRect) {
      return -1;
    }
    return 0;
  }

  protected isPlaceholderAriaHidden_(placeholder: Placeholder): string {
    return this.activeAnnotation_ || !placeholder.screenRect ? 'true' : 'false';
  }

  // Child placeholder elements intercept all pointer-events. Manually forward
  // wheel events to the viewport.
  private onWheel_(e: WheelEvent) {
    assert(this.viewport);
    if (e.ctrlKey) {
      return;
    }
    e.preventDefault();
    this.viewport.scrollBy({x: e.deltaX, y: e.deltaY});
  }

  private scrollToShowTextBox_(textBoxRect: TextBoxRect) {
    // The viewport handles scrolling, so prevent the browser from
    // auto-scrolling.
    this.$.container.scrollTop = 0;
    this.$.container.scrollLeft = 0;
    this.scrollTop = 0;
    this.scrollLeft = 0;

    assert(this.viewport);
    const viewportPosition = this.viewport.position;
    const viewportSize = this.viewport.size;

    let scrollX: number|undefined;
    let scrollY: number|undefined;
    if (textBoxRect.locationX < 0 ||
        textBoxRect.locationX + textBoxRect.width > viewportSize.width) {
      // Adjusting by 10% of viewport, rather than putting the text box on the
      // exact edge of the viewport.
      scrollX = viewportPosition.x + textBoxRect.locationX -
          Math.floor(viewportSize.width / 10);
    }

    if (textBoxRect.locationY < 0 ||
        textBoxRect.locationY + textBoxRect.height > viewportSize.height) {
      // Adjusting by 10% of viewport, rather than putting the text box on the
      // exact edge of the viewport.
      scrollY = viewportPosition.y + textBoxRect.locationY -
          Math.floor(viewportSize.height / 10);
    }

    if (scrollX !== undefined || scrollY !== undefined) {
      // TODO(crbug.com/40218278): Re-enable smooth scrolling for all codepaths.
      this.viewport.scrollTo({
        x: scrollX,
        y: scrollY,
      });
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ink-text-annotations': InkTextAnnotationsElement;
  }
}

customElements.define(InkTextAnnotationsElement.is, InkTextAnnotationsElement);
