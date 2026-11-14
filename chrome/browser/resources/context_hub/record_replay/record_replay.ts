// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {TimeDelta} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {browserProxyFactory} from '../context_hub.mojom-webui.js';
import type {SkillRecordingSessionData} from '../context_hub.mojom-webui.js';

import {getCss} from './record_replay.css.js';
import {getHtml} from './record_replay.html.js';

const MICROSECONDS_PER_MS = 1000;
const MS_PER_SECOND = 1000;
const SECONDS_PER_MINUTE = 60;
const MS_PER_MINUTE = SECONDS_PER_MINUTE * MS_PER_SECOND;
const TIME_UNIT_PAD_LENGTH = 2;
const MS_PAD_LENGTH = 3;

export interface TranscriptEntry {
  text: string;
  timestampStr: string;
  isFinal: boolean;
}

export class RecordReplayElement extends CrLitElement {
  static get is() {
    return 'record-replay';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      recordReplayEnabled_: {type: Boolean},
      isRecording_: {type: Boolean},
      sessionData_: {type: Object},
      transcripts_: {type: Array},
    };
  }

  protected accessor recordReplayEnabled_: boolean =
      loadTimeData.getBoolean('kRecordReplay');
  protected accessor isRecording_: boolean = false;
  protected accessor sessionData_: SkillRecordingSessionData|null = null;
  protected accessor transcripts_: TranscriptEntry[] = [];

  private listenerIds_: number[] = [];

  override connectedCallback() {
    super.connectedCallback();
    if (!this.recordReplayEnabled_) {
      return;
    }
    const callbackRouter = browserProxyFactory.getInstance().callbackRouter;
    this.listenerIds_.push(callbackRouter.onTranscriptUpdate.addListener(
        (text: string, timestamp: TimeDelta, isFinal: boolean) => {
          const tsStr = `[${this.formatTimestamp_(timestamp)}]`;
          // If last transcript is not final, overwrite it, otherwise
          // append.
          const newTranscripts = [...this.transcripts_];
          const last = newTranscripts.at(-1);
          if (last && !last.isFinal) {
            newTranscripts[newTranscripts.length - 1] = {
              text,
              timestampStr: tsStr,
              isFinal,
            };
          } else {
            newTranscripts.push({
              text,
              timestampStr: tsStr,
              isFinal,
            });
          }
          this.transcripts_ = newTranscripts;
        }));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(id => {
      browserProxyFactory.getInstance().callbackRouter.removeListener(id);
    });
    this.listenerIds_ = [];
  }

  protected onStartRecordingClick_() {
    if (!this.recordReplayEnabled_ || this.isRecording_) {
      return;
    }

    this.isRecording_ = true;
    this.sessionData_ = null;
    this.transcripts_ = [];
    browserProxyFactory.getInstance().handler.startSkillRecording();
  }

  protected async onStopRecordingClick_() {
    if (!this.recordReplayEnabled_ || !this.isRecording_) {
      return;
    }

    this.isRecording_ = false;
    const {data} =
        await browserProxyFactory.getInstance().handler.stopSkillRecording();
    this.sessionData_ = data;
  }

  protected formatTimestamp_(timestamp: TimeDelta|null): string {
    if (!timestamp) {
      return 'N/A';
    }
    const totalMs = Number(timestamp.microseconds) / MICROSECONDS_PER_MS;
    const minutes = Math.floor(totalMs / MS_PER_MINUTE)
                        .toString()
                        .padStart(TIME_UNIT_PAD_LENGTH, '0');
    const seconds = Math.floor((totalMs % MS_PER_MINUTE) / MS_PER_SECOND)
                        .toString()
                        .padStart(TIME_UNIT_PAD_LENGTH, '0');
    const millis = Math.floor(totalMs % MS_PER_SECOND)
                       .toString()
                       .padStart(MS_PAD_LENGTH, '0');
    return `${minutes}:${seconds}.${millis}`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'record-replay': RecordReplayElement;
  }
}

customElements.define(RecordReplayElement.is, RecordReplayElement);
