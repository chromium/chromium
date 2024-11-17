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
import {LanguageCode} from '../../core/soda/language_info.js';
import {
  assertExhaustive,
  assertExists,
  assertNotReached,
} from '../../core/utils/assert.js';

import {PlatformHandler} from './handler.js';
import {
  FormatFeature,
  LoadModelResult,
  ModelInfo,
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

// The minimum transcript token length for title generation and summarization.
const MIN_TOKEN_LENGTH = 200;

abstract class OnDeviceModel<T> implements Model<T> {
  constructor(
    private readonly remote: OnDeviceModelRemote,
    private readonly pageRemote: PageHandlerRemote,
    private readonly modelInfo: ModelInfo,
  ) {
    // TODO(pihsun): Handle disconnection error
  }

  abstract execute(content: string): Promise<ModelResponse<T>>;

  private async executeRaw(text: string): Promise<ModelResponse<string>> {
    const session = new SessionRemote();
    this.remote.startSession(session.$.bindNewPipeAndPassReceiver());
    const inputPiece = {text};
    const {size} = await session.getSizeInTokens({pieces: [inputPiece]});

    if (size < MIN_TOKEN_LENGTH) {
      return {
        kind: 'error',
        error: ModelResponseError.UNSUPPORTED_TRANSCRIPTION_IS_TOO_SHORT,
      };
    }
    if (size > this.modelInfo.inputTokenLimit) {
      return {
        kind: 'error',
        error: ModelResponseError.UNSUPPORTED_TRANSCRIPTION_IS_TOO_LONG,
      };
    }

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
    const result = await promise;
    return {kind: 'success', result};
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
      this.modelInfo.modelId,
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
    const response = await this.executeRaw(prompt);
    if (response.kind === 'error') {
      return response;
    }
    if (await this.contentIsUnsafe(response.result, responseSafetyFeature)) {
      return {kind: 'error', error: ModelResponseError.UNSAFE};
    }
    return {kind: 'success', result: response.result};
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

  protected modelInfoInternal: ModelInfo|null = null;

  protected abstract readonly featureType: FormatFeature;

  abstract createModel(remote: OnDeviceModelRemote): OnDeviceModel<T>;

  constructor(
    protected readonly remote: PageHandlerRemote,
    private readonly platformHandler: PlatformHandler,
  ) {
    super();
  }

  get modelInfo(): ModelInfo {
    return assertExists(this.modelInfoInternal);
  }

  async init(): Promise<void> {
    this.modelInfoInternal =
      (await this.remote.getModelInfo(this.featureType)).modelInfo;
    const update = (state: MojoModelState) => {
      this.state.value = mojoModelStateToModelState(state);
    };
    const monitor = new ModelStateMonitorReceiver({update});

    // This should be relatively quick since in recorder_app_ui.cc we just
    // return the cached state here, but we await here to avoid UI showing
    // temporary unavailable state.
    const {state} = await this.remote.addModelMonitor(
      this.modelInfo.modelId,
      monitor.$.bindNewPipeAndPassRemote(),
    );
    update(state);
  }

  override async load(): Promise<Model<T>|null> {
    const newModel = new OnDeviceModelRemote();
    const {result} = await this.remote.loadModel(
      this.modelInfo.modelId,
      newModel.$.bindNewPipeAndPassReceiver(),
    );
    if (result !== LoadModelResult.kSuccess) {
      console.error('Load model failed:', result);
      // TODO(pihsun): Have dedicated error type.
      return null;
    }
    return this.createModel(newModel);
  }

  override async loadAndExecute(content: string, language: LanguageCode):
    Promise<ModelResponse<T>> {
    if (!this.platformHandler.getLangPackInfo(language).isGenAiSupported) {
      return {kind: 'error', error: ModelResponseError.UNSUPPORTED_LANGUAGE};
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
  protected override featureType = FormatFeature.kAudioSummary;

  override createModel(remote: OnDeviceModelRemote): SummaryModel {
    return new SummaryModel(remote, this.remote, this.modelInfo);
  }
}

export class TitleSuggestionModelLoader extends ModelLoader<string[]> {
  protected override featureType = FormatFeature.kAudioTitle;

  override createModel(remote: OnDeviceModelRemote): TitleSuggestionModel {
    return new TitleSuggestionModel(
      remote,
      this.remote,
      this.modelInfo,
    );
  }
}
