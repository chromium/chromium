// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Model} from '../../core/platform_handler.js';
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
          safetyInterval: null,
          topK: 1,
          temperature: 0,
        },
        responseRouter.$.bindNewPipeAndPassRemote(),
    );
    return promise;
  }

  async suggestTitles(content: string): Promise<string[]> {
    content = shorten(content, MAX_CONTENT_WORDS);
    const prompt = TITLE_SUGGESTION_PROMPT_TEMPLATE.replace(
        PLACEHOLDER,
        content,
    );
    const res = await this.execute(prompt);
    // Note this is NOT an underscore: ▁(U+2581)
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
    return titles.slice(0, 3);
  }

  async summarize(content: string): Promise<string> {
    content = shorten(content, MAX_CONTENT_WORDS);
    const prompt = SUMMARIZATION_PROMPT_TEMPLATE.replace(PLACEHOLDER, content);
    const res = await this.execute(prompt);
    const summary = parseResponse(res);
    return summary;
  }

  close(): void {
    this.remote.$.close();
  }
}
