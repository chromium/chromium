// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './ink_text_box.js';

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TextAnnotation, TextBoxRect} from '../constants.js';
import {Ink2Manager} from '../ink2_manager.js';
import type {TextBoxInit} from '../ink2_manager.js';
import {pageToScreenCoordinates} from '../ink_text_annotation_utils.js';
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
  screenRect: TextBoxRect;
  rotations: number;
  label: string;
  zIndex: number;
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
      placeholders_: {type: Array},
    };
  }

  accessor viewport: Viewport|null = null;
  protected accessor activeAnnotation_: TextAnnotation|null = null;
  protected accessor activePageDimensions_: ViewportRect|null = null;
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
    this.updateAnnotations_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
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
      if (!pageAnnotationsMap) {
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

  private updatePlaceholders_() {
    assert(this.viewport);
    this.placeholders_ = this.annotations_.map(annotation => {
      const screenRect = pageToScreenCoordinates(
          annotation.pageIndex, annotation.textBoxRect, this.viewport!);
      return {
        screenRect,
        label: annotation.text,
        rotations: (this.viewport!.getClockwiseRotations() +
                    annotation.textOrientation) %
            4,
        zIndex: annotation.id,
      };
    });
  }

  protected getStyles_(placeholder: Placeholder) {
    return `
      --left: ${placeholder.screenRect.locationX}px;
      --top: ${placeholder.screenRect.locationY}px;
      --width: ${placeholder.screenRect.width}px;
      --height: ${placeholder.screenRect.height}px;
      z-index: ${placeholder.zIndex};
    `;
  }

  protected onPlaceholderFocus_(e: FocusEvent) {
    const currentTarget = e.currentTarget as HTMLElement;
    const index = Number(currentTarget.dataset['index']);
    const placeholder = this.placeholders_[index];
    assert(placeholder);

    this.scrollToShowTextBox_(placeholder.screenRect);
  }

  protected async onPlaceholderClick_(e: MouseEvent) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    await this.activateAnnotationByIndex_(index);
  }

  protected async onPlaceholderKeydown_(e: KeyboardEvent) {
    if (e.key !== 'Enter' && e.key !== ' ') {
      return;
    }
    e.preventDefault();
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    await this.activateAnnotationByIndex_(index);
  }

  private async activateAnnotationByIndex_(index: number) {
    // Grab the annotation first, since committing may update the annotations
    // list and make `index` refer to a different annotation than intended.
    const annotation = this.annotations_[index];
    assert(annotation);

    if (this.activeAnnotation_) {
      // The requested annotation is already active. This also ensures that if
      // committing deletes an annotation, it isn't the one being activated.
      if (this.activeAnnotation_.id === annotation.id) {
        return;
      }
      await this.$.textBox.commitTextAnnotation();
    }

    assert(this.viewport);

    // Convert box to screen coordinates.
    const screenRect = pageToScreenCoordinates(
        annotation.pageIndex, annotation.textBoxRect, this.viewport);

    // Create a copy of the annotation with screen coordinates for the textbox.
    const annotationToActivate = structuredClone(annotation);
    annotationToActivate.textBoxRect = screenRect;

    // Notify the backend.
    Ink2Manager.getInstance().reactivateTextAnnotation(annotation);
    this.activeAnnotation_ = annotationToActivate;
    this.activePageDimensions_ =
        this.viewport.getPageScreenRect(annotation.pageIndex);
  }

  commitActiveAnnotation(): Promise<void> {
    return this.$.textBox.commitTextAnnotation();
  }

  blurActiveAnnotation() {
    this.$.textBox.blur();
  }

  protected onTextBoxStateChanged_(e: CustomEvent<TextBoxState>) {
    if (e.detail === TextBoxState.INACTIVE) {
      this.activeAnnotation_ = null;
      this.activePageDimensions_ = null;
    }
    this.fire('state-changed', e.detail);
  }

  private async onInitializeTextBox_(data: TextBoxInit) {
    if (this.activeAnnotation_) {
      await this.$.textBox.commitTextAnnotation();
    }
    this.activeAnnotation_ = data.annotation;
    this.activePageDimensions_ = data.pageDimensions;
  }

  protected onTextboxFocused_(e: CustomEvent<TextBoxRect>) {
    this.scrollToShowTextBox_(e.detail);
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
