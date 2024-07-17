// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  Model,
  ModelResponse,
  ModelResponseError,
} from '../../core/on_device_model/types.js';
import {shorten} from '../../core/utils/utils.js';

import {
  OnDeviceModelRemote,
  ResponseChunk,
  ResponseSummary,
  SessionRemote,
  StreamingResponderCallbackRouter,
} from './types.js';

// TODO(shik): Extract the common prompting logic into a better place.
// The input token limit is 2048 and 3 words roughly equals to 4
// tokens. Having a conservative limit here and leaving some room for the
// template.
// TODO: b/336477498 - Get the token limit from server and accurately count the
// token size with the same tokenizer.
// TODO(shik): Make this configurable.
const MAX_CONTENT_WORDS = Math.floor(((2048 - 100) / 4) * 3);

const PLACEHOLDER = 'PLACEHOLDER';

const TITLE_SUGGESTION_PROMPT_TEMPLATE = `
Please help suggest 3 titles for the audio transcription below.

Transcription:
\`\`\`
${PLACEHOLDER}
\`\`\`

Please reply 3 titles separated with a newline between them in the following
format:
1. TITLE 1 (short, concise, ~10 words)
2. TITLE 2 (formal, ~12 words)
3. TITLE 3 (descriptive, ~15 words)

Reply:
<ctrl23>
`;

export const SUMMARIZATION_PROMPT_TEMPLATE = `${PLACEHOLDER}
Write an abstractive summary in 3-bullet points: <ctrl23>`;

/**
 * The keys are id of the safety classes.
 *
 * The safety class that each id corresponds to can be found at
 * //google3/chrome/intelligence/ondevice/data/example_text_safety.txtpb.
 *
 * TODO: b/349723775 - Adjust the threshold to the final one.
 */
const REQUEST_SAFETY_SCORE_THRESHOLDS = new Map([
  [4, 0.65],
  [23, 0.65],
]);

const RESPONSE_SAFETY_SCORE_THRESHOLDS = new Map([
  [0, 0.65],
  [1, 0.65],
  [2, 0.65],
  [3, 0.65],
  [4, 0.65],
  [5, 0.65],
  [6, 0.65],
  [7, 0.65],
  [9, 0.65],
  [10, 0.65],
  [11, 0.65],
  [12, 0.8],
  [18, 0.7],
  [20, 0.65],
  [21, 0.8],
  [23, 0.65],
]);

function parseResponse(res: string): string {
  // Note this is NOT an underscore: ▁(U+2581)
  return res.replaceAll('▁', ' ').replaceAll(/\n+/g, '\n').trim();
}

export class OnDeviceModel implements Model {
  constructor(private readonly remote: OnDeviceModelRemote) {
    // TODO(pihsun): Handle disconnection error
  }

  // TODO(pihsun): Streaming? We can return signal<string> if needed.
  execute(text: string): Promise<string> {
    const session = new SessionRemote();
    this.remote.startSession(session.$.bindNewPipeAndPassReceiver());
    const responseRouter = new StreamingResponderCallbackRouter();
    // TODO(pihsun): Error handling.
    const {promise, resolve} = Promise.withResolvers<string>();
    const response: string[] = [];
    const onResponseId = responseRouter.onResponse.addListener(
      (chunk: ResponseChunk) => {
        response.push(chunk.text);
      },
    );
    const onCompleteId = responseRouter.onComplete.addListener(
      (_: ResponseSummary) => {
        responseRouter.removeListener(onResponseId);
        responseRouter.removeListener(onCompleteId);
        responseRouter.$.close();
        session.$.close();
        resolve(response.join('').trimStart());
      },
    );
    session.execute(
      {
        text,
        ignoreContext: false,
        maxTokens: null,
        tokenOffset: null,
        maxOutputTokens: null,
        unusedSafetyInterval: null,
        topK: 1,
        temperature: 0,
      },
      responseRouter.$.bindNewPipeAndPassRemote(),
    );
    return promise;
  }

  async suggestTitles(content: string): Promise<ModelResponse<string[]>> {
    if (await this.contentIsUnsafe(content, REQUEST_SAFETY_SCORE_THRESHOLDS)) {
      return {kind: 'error', error: ModelResponseError.UNSAFE};
    }
    content = shorten(content, MAX_CONTENT_WORDS);
    const prompt = TITLE_SUGGESTION_PROMPT_TEMPLATE.replace(
      PLACEHOLDER,
      content,
    );
    const res = await this.execute(prompt);
    if (await this.contentIsUnsafe(res, RESPONSE_SAFETY_SCORE_THRESHOLDS)) {
      return {kind: 'error', error: ModelResponseError.UNSAFE};
    }
    const lines = parseResponse(res)
                    .replaceAll(/^\s*\d\.\s*/gm, '')
                    .replaceAll(/TITLE\s*\d*:?\s*/gim, '')
                    .split('\n');

    const titles: string[] = [];
    for (const line of lines) {
      // Find the longest title-like substring.
      const m = line.match(/\w.*\w/);
      if (m !== null && !titles.includes(m[0])) {
        titles.push(m[0]);
      }
    }
    return {kind: 'success', result: titles.slice(0, 3)};
  }

  async summarize(content: string): Promise<ModelResponse> {
    if (await this.contentIsUnsafe(content, REQUEST_SAFETY_SCORE_THRESHOLDS)) {
      return {kind: 'error', error: ModelResponseError.UNSAFE};
    }
    content = shorten(content, MAX_CONTENT_WORDS);
    const prompt = SUMMARIZATION_PROMPT_TEMPLATE.replace(PLACEHOLDER, content);
    const res = await this.execute(prompt);
    if (await this.contentIsUnsafe(res, RESPONSE_SAFETY_SCORE_THRESHOLDS)) {
      return {kind: 'error', error: ModelResponseError.UNSAFE};
    }
    const summary = parseResponse(res);
    return {kind: 'success', result: summary};
  }

  private async contentIsUnsafe(
    content: string,
    thresholds: Map<number, number>,
  ): Promise<boolean> {
    const info = await this.remote.classifyTextSafety(content);
    const scores = info.safetyInfo?.classScores ?? null;
    if (scores === null) {
      return false;
    }
    for (const [idx, threshold] of thresholds.entries()) {
      const score = scores[idx];
      if (score !== undefined && score >= threshold) {
        return true;
      }
    }
    return false;
  }

  close(): void {
    this.remote.$.close();
  }
}
