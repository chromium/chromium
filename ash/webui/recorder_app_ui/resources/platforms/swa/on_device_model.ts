// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  Model,
  ModelLoader as ModelLoaderBase,
  ModelResponse,
  ModelResponseError,
  ModelState,
} from '../../core/on_device_model/types.js';
import {signal} from '../../core/reactive/signal.js';
import {
  assertExhaustive,
  assertExists,
  assertNotReached,
} from '../../core/utils/assert.js';
import {shorten} from '../../core/utils/utils.js';

import {
  FormatFeature,
  LoadModelResult,
  ModelState as MojoModelState,
  ModelStateMonitorReceiver,
  ModelStateType,
  OnDeviceModelRemote,
  PageHandlerRemote,
  ResponseChunk,
  ResponseSummary,
  SessionRemote,
  StreamingResponderCallbackRouter,
} from './types.js';

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
`;

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

abstract class OnDeviceModel<T> implements Model<T> {
  constructor(
    private readonly remote: OnDeviceModelRemote,
    private readonly pageRemote: PageHandlerRemote,
    private readonly modelId: string,
  ) {
    // TODO(pihsun): Handle disconnection error
  }

  abstract execute(content: string): Promise<ModelResponse<T>>;

  private executeRaw(text: string): Promise<string> {
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

  private async formatInput(
    feature: FormatFeature,
    fields: Record<string, string>,
  ) {
    const {result} = await this.pageRemote.formatModelInput(
      {value: this.modelId},
      feature,
      fields,
    );
    return result;
  }

  /**
   * Formats the prompt with the specified `formatFeature`, runs the prompt
   * through the model, and returns the result.
   *
   * The key of the fields of each different model / formatFeature
   * combination can be found in
   * //google3/chromeos/odml_foundations/lib/inference/features/models/.
   */
  protected async formatAndExecute(
    formatFeature: FormatFeature,
    fields: Record<string, string>,
  ): Promise<ModelResponse<string>> {
    const prompt = await this.formatInput(formatFeature, fields);
    if (prompt === null) {
      console.error('formatInput returns null, wrong model?');
      return {kind: 'error', error: ModelResponseError.GENERAL};
    }
    if (await this.contentIsUnsafe(prompt, REQUEST_SAFETY_SCORE_THRESHOLDS)) {
      return {kind: 'error', error: ModelResponseError.UNSAFE};
    }
    const result = await this.executeRaw(prompt);
    if (await this.contentIsUnsafe(result, RESPONSE_SAFETY_SCORE_THRESHOLDS)) {
      return {kind: 'error', error: ModelResponseError.UNSAFE};
    }
    return {kind: 'success', result};
  }
}

export class SummaryModel extends OnDeviceModel<string> {
  override async execute(content: string): Promise<ModelResponse<string>> {
    content = shorten(content, MAX_CONTENT_WORDS);
    const resp = await this.formatAndExecute(FormatFeature.kAudioSummary, {
      transcription: content,
    });
    // TODO(pihsun): `Result` monadic helper class?
    if (resp.kind === 'error') {
      return resp;
    }
    const summary = parseResponse(resp.result);
    return {kind: 'success', result: summary};
  }
}

export class TitleSuggestionModel extends OnDeviceModel<string[]> {
  override async execute(content: string): Promise<ModelResponse<string[]>> {
    content = shorten(content, MAX_CONTENT_WORDS);
    const resp = await this.formatAndExecute(FormatFeature.kPrompt, {
      prompt: TITLE_SUGGESTION_PROMPT_TEMPLATE.replace(PLACEHOLDER, content),
    });
    if (resp.kind === 'error') {
      return resp;
    }
    const lines = parseResponse(resp.result)
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
}

/**
 * Converts ModelState from mojo to the `ModelState` interface.
 */
export function mojoModelStateToModelState(state: MojoModelState): ModelState {
  switch (state.type) {
    case ModelStateType.kNotInstalled:
      return {kind: 'notInstalled'};
    case ModelStateType.kInstalling:
      return {kind: 'installing', progress: assertExists(state.progress)};
    case ModelStateType.kInstalled:
      return {kind: 'installed'};
    case ModelStateType.kError:
      return {kind: 'error'};
    case ModelStateType.kUnavailable:
      return {kind: 'unavailable'};
    case ModelStateType.MIN_VALUE:
    case ModelStateType.MAX_VALUE:
      return assertNotReached(
        `Got MIN_VALUE or MAX_VALUE from mojo ModelStateType: ${state.type}`,
      );
    default:
      assertExhaustive(state.type);
  }
}

abstract class ModelLoader<T> extends ModelLoaderBase<T> {
  override state = signal<ModelState>({kind: 'unavailable'});

  protected abstract readonly modelId: string;

  abstract createModel(remote: OnDeviceModelRemote): OnDeviceModel<T>;

  constructor(protected readonly remote: PageHandlerRemote) {
    super();
  }

  async init(): Promise<void> {
    const update = (state: MojoModelState) => {
      this.state.value = mojoModelStateToModelState(state);
    };
    const monitor = new ModelStateMonitorReceiver({update});

    // This should be relatively quick since in recorder_app_ui.cc we just
    // return the cached state here, but we await here to avoid UI showing
    // temporary unavailable state.
    const {state} = await this.remote.addModelMonitor(
      {value: this.modelId},
      monitor.$.bindNewPipeAndPassRemote(),
    );
    update(state);
  }

  override async load(): Promise<Model<T>> {
    const newModel = new OnDeviceModelRemote();
    const {result} = await this.remote.loadModel(
      {value: this.modelId},
      newModel.$.bindNewPipeAndPassReceiver(),
    );
    if (result !== LoadModelResult.kSuccess) {
      // TODO(pihsun): Dedicated error type?
      throw new Error(`Load model failed: ${result}`);
    }
    return this.createModel(newModel);
  }
}

export class SummaryModelLoader extends ModelLoader<string> {
  protected override modelId = '73caa678-45cb-4007-abb9-f04e431376da';

  override createModel(remote: OnDeviceModelRemote): SummaryModel {
    return new SummaryModel(remote, this.remote, this.modelId);
  }
}

export class TitleSuggestionModelLoader extends ModelLoader<string[]> {
  protected override modelId = 'ee7c31c2-18e5-405a-b54e-f2607130a15d';

  override createModel(remote: OnDeviceModelRemote): TitleSuggestionModel {
    return new TitleSuggestionModel(remote, this.remote, this.modelId);
  }
}
