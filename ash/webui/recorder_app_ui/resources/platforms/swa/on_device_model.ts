// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  MAX_WORD_LENGTH,
  MIN_WORD_LENGTH,
} from '../../core/on_device_model/ai_feature_constants.js';
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
import {getWordCount} from '../../core/utils/utils.js';

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
  SafetyFeature,
  SessionRemote,
  StreamingResponderCallbackRouter,
} from './types.js';

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

  // TODO(hsuanling): Have a loadAndExecute method so that input check can be
  // done before loading models.
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
        // TODO: b/363288363 - Migrate to `input`.
        text,
        ignoreContext: false,
        maxTokens: null,
        tokenOffset: null,
        maxOutputTokens: null,
        unusedSafetyInterval: null,
        topK: 1,
        temperature: 0,
        input: null,
      },
      responseRouter.$.bindNewPipeAndPassRemote(),
    );
    return promise;
  }

  private async contentIsUnsafe(
    content: string,
    safetyFeature: SafetyFeature,
  ): Promise<boolean> {
    const {safetyInfo} = await this.remote.classifyTextSafety(content);
    if (safetyInfo === null) {
      return false;
    }
    const {isSafe} = await this.pageRemote.validateSafetyResult(
      safetyFeature,
      content,
      safetyInfo,
    );
    return !isSafe;
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
    requestSafetyFeature: SafetyFeature,
    responseSafetyFeature: SafetyFeature,
    fields: Record<string, string>,
  ): Promise<ModelResponse<string>> {
    const prompt = await this.formatInput(formatFeature, fields);
    if (prompt === null) {
      console.error('formatInput returns null, wrong model?');
      return {kind: 'error', error: ModelResponseError.GENERAL};
    }
    if (await this.contentIsUnsafe(prompt, requestSafetyFeature)) {
      return {kind: 'error', error: ModelResponseError.UNSAFE};
    }
    const result = await this.executeRaw(prompt);
    if (await this.contentIsUnsafe(result, responseSafetyFeature)) {
      return {kind: 'error', error: ModelResponseError.UNSAFE};
    }
    return {kind: 'success', result};
  }
}

export class SummaryModel extends OnDeviceModel<string> {
  override async execute(content: string): Promise<ModelResponse<string>> {
    const resp = await this.formatAndExecute(
      FormatFeature.kAudioSummary,
      SafetyFeature.kAudioSummaryRequest,
      SafetyFeature.kAudioSummaryResponse,
      {
        transcription: content,
      },
    );
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
    const resp = await this.formatAndExecute(
      FormatFeature.kAudioTitle,
      SafetyFeature.kAudioTitleRequest,
      SafetyFeature.kAudioTitleResponse,
      {
        transcription: content,
      },
    );
    if (resp.kind === 'error') {
      return resp;
    }
    const lines = parseResponse(resp.result).split('\n');

    const titles: string[] = [];
    for (const line of lines) {
      // Each line should start with `- ` and the title.
      const lineStart = '- ';
      if (line.startsWith(lineStart)) {
        titles.push(line.substring(lineStart.length));
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

  override async load(): Promise<Model<T>|null> {
    const newModel = new OnDeviceModelRemote();
    const {result} = await this.remote.loadModel(
      {value: this.modelId},
      newModel.$.bindNewPipeAndPassReceiver(),
    );
    if (result !== LoadModelResult.kSuccess) {
      console.error('Load model failed:', result);
      // TODO(pihsun): Have dedicated error type.
      return null;
    }
    return this.createModel(newModel);
  }

  override async loadAndExecute(content: string): Promise<ModelResponse<T>> {
    const wordCount = getWordCount(content);
    if (wordCount < MIN_WORD_LENGTH) {
      return {
        kind: 'error',
        error: ModelResponseError.UNSUPPORTED_TRANSCRIPTION_IS_TOO_SHORT,
      };
    }

    if (wordCount > MAX_WORD_LENGTH) {
      return {
        kind: 'error',
        error: ModelResponseError.UNSUPPORTED_TRANSCRIPTION_IS_TOO_LONG,
      };
    }

    const model = await this.load();
    if (model === null) {
      // TODO(pihsun): Specific error type / message for model loading error.
      return {
        kind: 'error',
        error: ModelResponseError.GENERAL,
      };
    }
    try {
      return await model.execute(content);
    } finally {
      model.close();
    }
  }
}

export class SummaryModelLoader extends ModelLoader<string> {
  protected override modelId = '73caa678-45cb-4007-abb9-f04e431376da';

  override createModel(remote: OnDeviceModelRemote): SummaryModel {
    return new SummaryModel(remote, this.remote, this.modelId);
  }
}

export class TitleSuggestionModelLoader extends ModelLoader<string[]> {
  protected override modelId = '1bdd5282-2d14-413c-bf43-9ea6d55c38a6';

  override createModel(remote: OnDeviceModelRemote): TitleSuggestionModel {
    return new TitleSuggestionModel(remote, this.remote, this.modelId);
  }
}
