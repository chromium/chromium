// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isTranslatePdfEnabled} from '../shared/common.js';

import type {ReadingModeSelection} from './read_anything_types.js';

// The name of the CSS highlight that shows the highlighted blocks. Styled in
// app.css, which only shows it in the side panel, where the main panel is
// shown next to reading mode.
export const BLOCK_SELECTION_HIGHLIGHT_NAME = 'read-anything-block-selection';

// The class added to the content container while blocks are highlighted. It
// makes the regular selection transparent, so that the selection isn't painted
// on top of the highlighted blocks. Styled in app.css.
export const BLOCK_HIGHLIGHT_SHOWN_CLASS = 'block-highlight-shown';

// Highlights whole blocks (e.g. paragraphs or headings) of the reading mode
// content so that they look selected. When the user selects translated text,
// the main panel selects the whole blocks that contain it (see
// SelectionController), and this shows those blocks here without changing the
// user's selection, which would interfere with the user's drag or keyboard
// selection.
//
// Unlike Read Aloud's highlights, which wrap the highlighted text in spans
// with a CSS class, this uses the CSS Custom Highlight API, because:
// - It doesn't change the DOM. The translate script rewrites the blocks'
//   content, so it could replace added spans or be confused by them. Spans
//   would also change the text nodes that the NodeStore maps to AXNodeIDs.
// - It paints only the text, like a selection. A CSS class on the blocks
//   would paint their whole boxes instead.
// - One range covers all of the blocks, including nested ones. Its endpoints
//   are in the blocks' parents rather than in their text, so they don't move
//   when the translate script replaces the blocks' content.
//
// Blocks are only highlighted for TranslatePdf, so the methods do nothing when
// the feature is disabled. They're guarded by the feature rather than by
// whether the page is a PDF, so that a highlight is still cleared after
// navigating from a PDF to another page.
export class BlockSelectionHighlighter {
  // The highlighted range, or null if nothing is highlighted.
  private range_: Range|null = null;
  // The content container with BLOCK_HIGHLIGHT_SHOWN_CLASS while blocks are
  // highlighted.
  private container_: Element|null = null;

  // Highlights the whole blocks selected by the given selection in the content
  // container, replacing any previous highlight. The selection must be
  // forward and have its endpoints in the blocks' parents, like the
  // selections returned by TranslatedSelectionMapper.getReadingModeSelection().
  highlight(blocks: ReadingModeSelection, contentContainer: Node) {
    if (!isTranslatePdfEnabled()) {
      return;
    }
    // The selection is forward, so its anchor is the start of the range.
    const range = new Range();
    range.setStart(blocks.anchorNode, blocks.anchorOffset);
    range.setEnd(blocks.focusNode, blocks.focusOffset);
    if (this.range_ && areSameRanges(this.range_, range)) {
      return;
    }
    this.range_ = range;
    CSS.highlights.set(BLOCK_SELECTION_HIGHLIGHT_NAME, new Highlight(range));
    this.container_?.classList.remove(BLOCK_HIGHLIGHT_SHOWN_CLASS);
    this.container_ =
        contentContainer instanceof Element ? contentContainer : null;
    this.container_?.classList.add(BLOCK_HIGHLIGHT_SHOWN_CLASS);
  }

  // Removes the highlight, if any.
  clear() {
    if (!isTranslatePdfEnabled() || !this.range_) {
      return;
    }
    this.range_ = null;
    CSS.highlights.delete(BLOCK_SELECTION_HIGHLIGHT_NAME);
    this.container_?.classList.remove(BLOCK_HIGHLIGHT_SHOWN_CLASS);
    this.container_ = null;
  }

  static getInstance(): BlockSelectionHighlighter {
    return instance || (instance = new BlockSelectionHighlighter());
  }

  static setInstance(obj: BlockSelectionHighlighter) {
    instance = obj;
  }
}

function areSameRanges(first: Range, second: Range): boolean {
  return first.startContainer === second.startContainer &&
      first.startOffset === second.startOffset &&
      first.endContainer === second.endContainer &&
      first.endOffset === second.endOffset;
}

let instance: BlockSelectionHighlighter|null = null;
