// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {log} from './logging.js';

const FILE = 'CaptionBlockManager';

export interface CaptionBlockDelegate {
  onCaptionBlockUpdated: (blockText: string) => void;
}

export class CaptionBlockManager {
  private captionBlocks: string[] = [];
  private currentBlockIndex: number = 0;
  private blockDisplayTimer: number = 0;
  private lastBlockSwitchTime: number = 0;
  private delegate: CaptionBlockDelegate;

  constructor(delegate: CaptionBlockDelegate) {
    this.delegate = delegate;
  }

  getLastBlockSwitchTime(): number {
    return this.lastBlockSwitchTime;
  }

  formatCaptionsText(text: string, usePersona: boolean): string {
    const maxLineLength = usePersona ? 50 : 60;
    return formatCaptions(text, maxLineLength);
  }

  updateBlocks(
      text: string, usePersona: boolean, isPlayingAudio: boolean,
      audioPlaybackStartTime: number) {
    const maxLineLength = usePersona ? 50 : 60;
    const newBlocks = getCaptionBlocks(text, maxLineLength);

    if (newBlocks.length === 0) {
      return;
    }

    if (newBlocks.length > this.captionBlocks.length) {
      const addedBlockCount = newBlocks.length - this.captionBlocks.length;
      log(FILE,
          `Generated ${addedBlockCount} new block(s). Total blocks: ${
              newBlocks.length}`);

      this.captionBlocks = newBlocks;

      if (!isPlayingAudio) {
        this.currentBlockIndex = this.captionBlocks.length - 1;
        this.delegate.onCaptionBlockUpdated(
            this.captionBlocks[this.currentBlockIndex] || '');
        return;
      }

      if (this.currentBlockIndex === 0 && this.blockDisplayTimer === 0) {
        const timeSinceAudioStarted = Date.now() - audioPlaybackStartTime;
        const remainingInitialDelay = Math.max(0, 1000 - timeSinceAudioStarted);
        log(FILE,
            `Audio playing. Scheduling block 1 in ${remainingInitialDelay}ms`);

        this.lastBlockSwitchTime = Date.now();
        this.blockDisplayTimer = setTimeout(() => {
          this.blockDisplayTimer = 0;
          this.advanceToNextBlock();
        }, remainingInitialDelay);
      } else if (
          this.blockDisplayTimer === 0 &&
          this.currentBlockIndex < this.captionBlocks.length - 1) {
        log(FILE,
            `Timer not active. Advancing from block ${
                this.currentBlockIndex + 1} immediately`);
        this.advanceToNextBlock();
      }
    } else {
      this.captionBlocks = newBlocks;
      if (this.currentBlockIndex < this.captionBlocks.length) {
        this.delegate.onCaptionBlockUpdated(
            this.captionBlocks[this.currentBlockIndex] || '');
      }
    }
  }

  private advanceToNextBlock() {
    if (this.currentBlockIndex < this.captionBlocks.length - 1) {
      this.currentBlockIndex++;
      this.lastBlockSwitchTime = Date.now();
      const blockText = this.captionBlocks[this.currentBlockIndex] || '';
      log(FILE,
          `Advancing to block ${this.currentBlockIndex + 1}/${
              this.captionBlocks.length}: "${blockText.replace(/\n/g, ' ')}"`);
      this.delegate.onCaptionBlockUpdated(blockText);

      // If more blocks remain, schedule the next one at ~4 seconds per block
      if (this.currentBlockIndex < this.captionBlocks.length - 1) {
        this.blockDisplayTimer = setTimeout(() => {
          this.blockDisplayTimer = 0;
          this.advanceToNextBlock();
        }, 4000);
      }
    }
  }

  reset() {
    if (this.blockDisplayTimer !== 0) {
      clearTimeout(this.blockDisplayTimer);
      this.blockDisplayTimer = 0;
    }
    this.captionBlocks = [];
    this.currentBlockIndex = 0;
    this.lastBlockSwitchTime = 0;
  }
}

export function getCaptionBlocks(
    text: string, maxLineLength: number = 40): string[] {
  const words = text.split(/\s+/);
  const lines: string[] = [];
  let currentLine = '';

  for (const word of words) {
    if (!word)
      continue;
    if (currentLine.length + word.length + 1 > maxLineLength) {
      lines.push(currentLine);
      currentLine = word;
    } else {
      currentLine = currentLine ? `${currentLine} ${word}` : word;
    }
  }
  if (currentLine) {
    lines.push(currentLine);
  }

  const blocks: string[] = [];
  for (let i = 0; i < lines.length; i += 2) {
    const line1 = lines[i] || '';
    const line2 = lines[i + 1] || '';
    blocks.push(line2 ? `${line1}\n${line2}` : line1);
  }
  return blocks;
}

export function formatCaptions(
    text: string, maxLineLength: number = 40, maxLines: number = 2): string {
  const words = text.split(/\s+/);
  const lines: string[] = [];
  let currentLine = '';

  for (const word of words) {
    if (!word)
      continue;
    if (currentLine.length + word.length + 1 > maxLineLength) {
      lines.push(currentLine);
      currentLine = word;
    } else {
      currentLine = currentLine ? `${currentLine} ${word}` : word;
    }
  }
  if (currentLine) {
    lines.push(currentLine);
  }

  const recentLines = lines.slice(-maxLines);
  return recentLines.join('\n');
}
