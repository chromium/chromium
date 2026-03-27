// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReachedCase} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {CrLitElement, html} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {type Range as MojomRange} from '//resources/mojo/ui/gfx/range/mojom/range.mojom-webui.js';

import type {OmniboxViewState} from './browser_proxy.js';
import {getCss} from './readonly_omnibox.css.js';
import {getHtml} from './readonly_omnibox.html.js';
import type {OmniboxTextPortion} from './toolbar_ui_api_data_model.mojom-webui.js';
import {OmniboxTextColor} from './toolbar_ui_api_data_model.mojom-webui.js';

enum UserSelectionStatus {
  NOT_SET = 0,
  EXPLICITLY_CLEARED = 1
}

export interface ReadonlyOmniboxElement {
  $: {
    textContainer: HTMLElement,
    textContainerWrap: HTMLElement,
  };
}

export class ReadonlyOmniboxElement extends CrLitElement {
  static get is() {
    return 'readonly-omnibox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      omniboxViewState: {type: Object},
    };
  }

  accessor omniboxViewState: OmniboxViewState = {
    textPieces: [],
    selection: null,
    textIsUrl: false,
  };

  private eventTracker: EventTracker = new EventTracker();

  // Selection state set here, rather than gotten from the browser.
  private userSelection: MojomRange|UserSelectionStatus =
      UserSelectionStatus.NOT_SET;

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker.add(
        document, 'selectionchange', this.onSelectionChange.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker.removeAll();
  }

  override firstUpdated(changedProperties: PropertyValues<this>): void {
    super.firstUpdated(changedProperties);
    this.addEventListener('blur', this.onBlur.bind(this));
    this.addEventListener('focus', this.onFocus.bind(this));
    this.$.textContainerWrap.addEventListener(
        'focus', this.onWrapFocus.bind(this));
  }

  override updated(changedProperties: PropertyValues<this>): void {
    super.updated(changedProperties);
    if (changedProperties.has('omniboxViewState')) {
      // We got fresh state from browser, so drop saved user selection ---
      // either the browser changed the URL, so it's not gonna make sense,
      // or it specifically wants specific things selected (or not).
      this.userSelection = UserSelectionStatus.NOT_SET;
      this.updateSelection();

      this.$.textContainer.classList.toggle(
          'force-ltr', this.omniboxViewState.textIsUrl);
    }
  }

  // Update displayed selection/caret based on browser-provided state
  // (`this.omniboxViewState`) and saved user selection (`this.userSelection`),
  // clearing it if the omnibox is not focused.
  // In the latter case, also makes sure to scroll long URLs to beginning.
  private updateSelection() {
    const textContainer = this.$.textContainer;
    let selectionState: MojomRange|null;
    if (this.userSelection === UserSelectionStatus.EXPLICITLY_CLEARED) {
      selectionState = null;
    } else if (this.userSelection === UserSelectionStatus.NOT_SET) {
      selectionState = this.omniboxViewState.selection;
    } else {
      selectionState = this.userSelection;
    }

    const selection = document.getSelection();
    if (!selection) {
      return;
    }

    // Range doesn't have Views's backwards selection feature, so normalize.
    if (selectionState && selectionState.start > selectionState.end) {
      selectionState = {start: selectionState.end, end: selectionState.start};
    }

    if (this.hasFocus() && selectionState) {
      selection.removeAllRanges();
      const textChildren = this.textChildren();
      if (textChildren.length) {
        const range = document.createRange();
        const start =
            this.globalOffsetToNodeRel(textChildren, selectionState.start);
        const end =
            this.globalOffsetToNodeRel(textChildren, selectionState.end);

        assert(start[0]);
        assert(end[0]);
        range.setStart(start[0], start[1]);
        range.setEnd(end[0], end[1]);
        selection.addRange(range);
      }
    } else if (selection.containsNode(
                   textContainer, /*allowPartialContainment=*/ true)) {
      // Make sure we make the beginning of the line visible.
      textContainer.scrollLeft = 0;

      // We should clear selection if it's with us and we're not focused or
      // it's blank in state.
      selection.removeAllRanges();
    }
  }

  private hasFocus(): boolean {
    return document.hasFocus() &&
        this.shadowRoot.activeElement === this.$.textContainer;
  }

  private onBlur(): void {
    this.updateSelection();
  }

  private onFocus(): void {
    // TODO(crbug.com/474060468): The Views behavior is pretty subtle here: it
    // mostly selects all but you can drag-select to get that specific
    // selection. Focus restore only seems to happen on keyboard focus.
    this.updateSelection();
  }

  // Called when the selection in the document is changed. If this happened when
  // this has the focus, update `state.userSelection` to match.
  //
  // TODO(crbug.com/474060468): We need to push selection to the browser. This
  // needs to worry about asynchrony.
  private onSelectionChange(): void {
    // TODO(crbug.com/474060468): This does weird stuff if other things can get
    // selection, which can hypothetically happen much down the line.

    // Ignore changes if we're not focused; this helps ignore our own
    // clearing of it in this case.
    if (!this.hasFocus()) {
      return;
    }

    const selection = document.getSelection();
    if (!selection) {
      return;
    }
    const ranges =
        selection.getComposedRanges({shadowRoots: [this.shadowRoot]});
    if (ranges.length === 0) {
      this.userSelection = UserSelectionStatus.EXPLICITLY_CLEARED;
      return;
    }

    assert(ranges.length === 1);
    assert(ranges[0]);
    const range = ranges[0];
    const textChildren = this.textChildren();
    if (textChildren.length === 0 || !range.startContainer ||
        !range.endContainer) {
      return;
    }

    // Convert from StaticRange to full DOM Range for the API access.
    const domRange = document.createRange();
    domRange.setStart(range.startContainer, range.startOffset);
    domRange.setEnd(range.endContainer, range.endOffset);

    this.userSelection = {
      start: ReadonlyOmniboxElement.nodeRelToGlobalOffsetStart(
          textChildren, domRange),
      end: ReadonlyOmniboxElement.nodeRelToGlobalOffsetEnd(
          textChildren, domRange),
    };
  }

  private onWrapFocus(): void {
    // We forward focus requests from the entirety of textContainerWrap to
    // textContainer.
    this.$.textContainer.focus();
  }

  // Given character offset `offset`, returns which of `textChildren`
  // (representing pieces of the overall text) and the offset into that
  // child, that correspond to it. Will return null for the node if `offset`
  // is out of range.
  private globalOffsetToNodeRel(textChildren: Text[], offset: number):
      [Text|null, number] {
    for (const child of textChildren) {
      if (offset <= child.length) {
        return [child, offset];
      }
      offset -= child.length;
    }
    return [null, 0];
  }

  // Assuming the overall text is represented as concatenation of
  // `textChildren`, returns the offset into the text corresponding to the
  // beginning of `range`.
  static nodeRelToGlobalOffsetStart(textChildren: Text[], range: Range):
      number {
    let offset = 0;
    for (const child of textChildren) {
      if (child === range.startContainer) {
        return offset + range.startOffset;
      }

      const rel = range.comparePoint(child, 0);
      if (rel === -1) {
        // The child is before the range (and isn't startContainer),
        // so the text offset should skip it.
        offset += child.length;
      } else {
        // The text child is within the range (and isn't startContainer), or
        // after, so we skipped over everything that needs to be skipped.
        break;
      }
    }
    return offset;
  }

  // Assuming the overall text is represented as concatenation of
  // `textChildren`, returns the offset into the text corresponding to the
  // end of `range`.
  static nodeRelToGlobalOffsetEnd(textChildren: Text[], range: Range): number {
    let offset = 0;
    for (const child of textChildren) {
      offset += child.length;
    }

    for (let i = textChildren.length - 1; i >= 0; --i) {
      const child = textChildren[i]!;

      if (child === range.endContainer) {
        return offset - child.length + range.endOffset;
      }

      const rel = range.comparePoint(child, 0);
      if (rel === 1) {
        // The child is after the range (and isn't endContainer),
        // so the text offset should skip it.
        offset -= child.length;
      } else {
        // The text child is within the range (and isn't endContainer) or
        // before, so we skipped over everything that needs to be skipped.
        break;
      }
    }
    return offset;
  }

  // Return all Text nodes that are descendants of the textContainer.
  textChildren(): Text[] {
    const result: Text[] = [];
    this.collectTextChildren(this.$.textContainer, result);
    return result;
  }

  // Helper to recursively accumulate Text nodes into `out` starting from
  // `node`.
  private collectTextChildren(node: Node|null, out: Text[]): void {
    if (!node) {
      return;
    }

    if (node.nodeType === Node.TEXT_NODE) {
      out.push(node as Text);
      return;
    }

    if (node.nodeType === Node.ELEMENT_NODE) {
      node = node.firstChild;
      while (node) {
        this.collectTextChildren(node, out);
        node = node.nextSibling;
      }
    }
  }

  // Returns an html template for rendering the given text piece.
  static renderTextPiece(piece: OmniboxTextPortion) {
    let classes = '';
    switch (piece.color) {
      case OmniboxTextColor.kOmniboxTextDimmed:
        classes = 'color-dim ';
        break;
      case OmniboxTextColor.kOmniboxForegroundDisabled:
        classes = 'color-foreground-disabled ';
        break;
      case OmniboxTextColor.kOmniboxSecurityChipDangerous:
        classes = 'color-danger ';
        break;
      case OmniboxTextColor.kUnspecified:
        console.error('Unexected kUnspecified for text color');
        break;
      case OmniboxTextColor.kOmniboxText:
        // The default is fine.
        break;
      default:
        assertNotReachedCase(piece.color);
    }
    if (piece.strikethrough) {
      classes += 'strikethrough';
    }
    return html`<span class='${classes}'>${piece.text}</span>`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'readonly-omnibox': ReadonlyOmniboxElement;
  }
}

customElements.define(ReadonlyOmniboxElement.is, ReadonlyOmniboxElement);
