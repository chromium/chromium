// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  LoadModelResult,
  Model,
  ModelExecutionError,
  ModelLoader as ModelLoaderBase,
  ModelLoadError,
  ModelResponse,
  ModelState,
} from '../../core/on_device_model/types.js';
import {signal} from '../../core/reactive/signal.js';
import {LanguageCode} from '../../core/soda/language_info.js';
import {
  assertExhaustive,
  assertExists,
  assertNotReached,
} from '../../core/utils/assert.js';
import {
  chunkContentByWord,
} from '../../core/utils/utils.js';

import {PlatformHandler} from './handler.js';
import {
  isCannedResponse,
  isInvalidFormatResponse,
  parseResponse,
  trimRepeatedBulletPoints,
} from './on_device_model_utils.js';
import {
  FormatFeature,
  LoadModelResult as MojoLoadModelResult,
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


// The minimum transcript token length for title generation and summarization.
const MIN_TOKEN_LENGTH = 200;

// The maximum content input length for T&S model.
// Based on tests, 1k input tokens is the most efficient.
// According to the Gemini tokenizer documentation
// (https://ai.google.dev/gemini-api/docs/tokens), 100 tokens are roughly
// equivalent to 60-80 English words, chose 700 since it is average.
const MAX_TS_MODEL_INPUT_WORD_LENGTH = 700;

// The config for model repetition judgement.
// We split model response into bullet points and check string length and LCS
// (longest common subsequence) scores by words.
// If conditions are over threshold, show invalid or trim repetition.
// Thresholds are designed based on real model responses.
const MODEL_REPETITION_CONFIG = {
  maxLength: 1000,
  lcsScoreThreshold: 0.9,
};

abstract class OnDeviceModel<T> implements Model<T> {
  constructor(
    private readonly remote: OnDeviceModelRemote,
    private readonly pageRemote: PageHandlerRemote,
    protected readonly modelInfo: ModelInfo,
  ) {
    // TODO(pihsun): Handle disconnection error
  }

  async execute(content: string, language: LanguageCode):
    Promise<ModelResponse<T>> {
    const session = new SessionRemote();
    this.remote.startSession(session.$.bindNewPipeAndPassReceiver(), null);
    const result =
      await this.executeInRemoteSession(content, language, session);
    session.$.close();
    return result;
  }

  /**
   * Execute in one remote session for performance concern.
   * Each model should override this function and share the session for all
   * model actions.
   */
  abstract executeInRemoteSession(
    content: string, language: LanguageCode, session: SessionRemote
  ): Promise<ModelResponse<T>>;

  /**
   * Get input token size through the model.
   * Share the session from params without creating new session.
   */
  protected async getInputTokenSize(text: string, session: SessionRemote):
    Promise<number> {
    const inputPieces = {pieces: [{text}]};
    const {size} = await session.getSizeInTokens(inputPieces);
    return size;
  }

  /**
   * Conduct the model execute.
   * Check input token size first and then execute.
   * Share the session from params without creating new session.
   */
  private async executeRaw(
    text: string,
    session: SessionRemote,
    expectedBulletPointCount: number,
    language: LanguageCode,
  ): Promise<ModelResponse<string>> {
    const inputPieces = {pieces: [{text}]};
    const size = await this.getInputTokenSize(text, session);

    if (size < MIN_TOKEN_LENGTH) {
      return {
        kind: 'error',
        error: ModelExecutionError.UNSUPPORTED_TRANSCRIPTION_IS_TOO_SHORT,
      };
    }
    if (size > this.modelInfo.inputTokenLimit) {
      return {
        kind: 'error',
        error: ModelExecutionError.UNSUPPORTED_TRANSCRIPTION_IS_TOO_LONG,
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
        resolve(response.join('').trimStart());
      },
    );
    session.append(
      {
        maxTokens: 0,
        input: inputPieces,
      },
      null,
    );
    session.generate(
      {
        maxOutputTokens: 0,
        constraint: null,
      },
      responseRouter.$.bindNewPipeAndPassRemote(),
    );
    const result = await promise;

    // When the model returns the canned response, show the same UI as
    // unsafe content for now.
    if (isCannedResponse(result)) {
      return {kind: 'error', error: ModelExecutionError.UNSAFE};
    }

    const parsedResult = parseResponse(result);
    // TODO(yuanchieh): retry inference with higher temperature.
    if (isInvalidFormatResponse(
          parsedResult,
          expectedBulletPointCount,
        )) {
      return {kind: 'error', error: ModelExecutionError.UNSAFE};
    }

    const finalBulletPoints = trimRepeatedBulletPoints(
      parsedResult,
      MODEL_REPETITION_CONFIG.maxLength,
      language,
      MODEL_REPETITION_CONFIG.lcsScoreThreshold,
    );

    // Show unsafe content if no valid bullet point.
    if (finalBulletPoints.length === 0) {
      return {kind: 'error', error: ModelExecutionError.UNSAFE};
    }

    // To align with model response type, concatenated bullet points back to one
    // string.
    const finalResult = finalBulletPoints.join('\n');

    return {kind: 'success', result: finalResult};
  }

  private async contentIsUnsafe(
    content: string,
    safetyFeature: SafetyFeature,
    language: LanguageCode,
  ): Promise<boolean> {
    // Split the content into chunks due to model performance considerations.
    const contentChunks =
      chunkContentByWord(content, MAX_TS_MODEL_INPUT_WORD_LENGTH, language);
    for (const chunk of contentChunks) {
      const {safetyInfo} = await this.remote.classifyTextSafety(chunk);
      if (safetyInfo === null) {
        continue;
      }

      const {isSafe} = await this.pageRemote.validateSafetyResult(
        safetyFeature,
        chunk,
        safetyInfo,
      );
      if (!isSafe) {
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
    session: SessionRemote,
    language: LanguageCode,
    expectedBulletPointCount: number,
  ): Promise<ModelResponse<string>> {
    const prompt = await this.formatInput(formatFeature, fields);
    if (prompt === null) {
      console.error('formatInput returns null, wrong model?');
      return {kind: 'error', error: ModelExecutionError.GENERAL};
    }
    if (await this.contentIsUnsafe(prompt, requestSafetyFeature, language)) {
      return {kind: 'error', error: ModelExecutionError.UNSAFE};
    }
    const response = await this.executeRaw(
      prompt,
      session,
      expectedBulletPointCount,
      language,
    );
    if (response.kind === 'error') {
      return response;
    }
    if (await this.contentIsUnsafe(
          response.result,
          responseSafetyFeature,
          language,
        )) {
      return {kind: 'error', error: ModelExecutionError.UNSAFE};
    }
    return {kind: 'success', result: response.result};
  }
}

export class SummaryModel extends OnDeviceModel<string> {
  override async executeInRemoteSession(
    content: string,
    language: LanguageCode,
    session: SessionRemote,
  ): Promise<ModelResponse<string>> {
    const inputTokenSize = await this.getInputTokenSize(content, session);
    // For large model, we use v2 safety feature. It only affects on response.
    const safetyFeatureOnResponse = this.modelInfo.isLargeModel ?
      SafetyFeature.kAudioSummaryResponseV2 :
      SafetyFeature.kAudioSummaryResponse;

    const expectedBulletPointCount =
      this.getExpectedBulletPoints(inputTokenSize);
    const bulletPointsRequest =
      this.formatBulletPointRequest(expectedBulletPointCount);
    const resp = await this.formatAndExecute(
      FormatFeature.kAudioSummary,
      SafetyFeature.kAudioSummaryRequest,
      safetyFeatureOnResponse,
      {
        transcription: content,
        language,
        /**
         * Param format is requested by model.
         * See
         * http://google3/chromeos/odml_foundations/lib/inference/features/models/audio_summary_v2.cc.
         */
        /* eslint-disable-next-line @typescript-eslint/naming-convention */
        bullet_points_request: bulletPointsRequest,
      },
      session,
      language,
      expectedBulletPointCount,
    );
    // TODO(pihsun): `Result` monadic helper class?
    if (resp.kind === 'error') {
      return resp;
    }
    return {kind: 'success', result: resp.result};
  }

  /**
   * Get expected bullet points by input token size.
   */
  private getExpectedBulletPoints(inputTokenSize: number): number {
    // For Xss model, return fixed 3 bullet points.
    if (!this.modelInfo.isLargeModel) {
      return 3;
    }

    if (inputTokenSize < 250) {
      return 1;
    } else if (inputTokenSize < 600) {
      return 2;
    } else if (inputTokenSize < 4000) {
      return 3;
    } else if (inputTokenSize < 6600) {
      return 4;
    } else if (inputTokenSize < 9300) {
      return 5;
    } else {
      return 6;
    }
  }

  /**
   * Format bullet point request to fit model prompt format.
   */
  private formatBulletPointRequest(request: number): string {
    if (request <= 0) {
      assertNotReached('Got non-positive bullet point request.');
    }
    return `${request} bullet point` + (request > 1 ? 's' : '');
  }
}

export class TitleSuggestionModel extends OnDeviceModel<string[]> {
  // For title suggestion, model input only needs transcription.
  override async executeInRemoteSession(
    content: string,
    language: LanguageCode,
    session: SessionRemote,
  ): Promise<ModelResponse<string[]>> {
    // For large model, we use v2 safety feature. It only affects on response.
    const safetyFeatureOnResponse = this.modelInfo.isLargeModel ?
      SafetyFeature.kAudioTitleResponseV2 :
      SafetyFeature.kAudioTitleResponse;

    const resp = await this.formatAndExecute(
      FormatFeature.kAudioTitle,
      SafetyFeature.kAudioTitleRequest,
      safetyFeatureOnResponse,
      {
        transcription: content,
        language,
      },
      session,
      language,
      3,  // always return 3 bullet points
    );
    if (resp.kind === 'error') {
      return resp;
    }
    const lines = resp.result.split('\n');

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
    case ModelStateType.kNeedsReboot:
      return {kind: 'needsReboot'};
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

function mojoLoadModelResultToModelLoadError(result: MojoLoadModelResult):
  ModelLoadError {
  switch (result) {
    case MojoLoadModelResult.kFailedToLoadLibrary:
    case MojoLoadModelResult.kGpuBlocked:
      return ModelLoadError.LOAD_FAILURE;
    case MojoLoadModelResult.kCrosNeedReboot:
      return ModelLoadError.NEEDS_REBOOT;
    case MojoLoadModelResult.kSuccess:
      return assertNotReached(`Try transforming success load result to error`);
    case MojoLoadModelResult.MIN_VALUE:
    case MojoLoadModelResult.MAX_VALUE:
      return assertNotReached(
        `Got MIN_VALUE or MAX_VALUE from mojo LoadModelResult: ${result}`,
      );
    default:
      assertExhaustive(result);
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

  override async load(): Promise<LoadModelResult<T>> {
    const newModel = new OnDeviceModelRemote();
    const {result} = await this.remote.loadModel(
      this.modelInfo.modelId,
      newModel.$.bindNewPipeAndPassReceiver(),
    );
    if (result !== MojoLoadModelResult.kSuccess) {
      console.error('Load model failed:', result);
      return {
        kind: 'error',
        error: mojoLoadModelResultToModelLoadError(result),
      };
    }
    return {kind: 'success', model: this.createModel(newModel)};
  }

  override async loadAndExecute(content: string, language: LanguageCode):
    Promise<ModelResponse<T>> {
    if (!this.platformHandler.getLangPackInfo(language).isGenAiSupported) {
      return {kind: 'error', error: ModelExecutionError.UNSUPPORTED_LANGUAGE};
    }

    const loadResult = await this.load();
    if (loadResult.kind === 'error') {
      return loadResult;
    }
    try {
      return await loadResult.model.execute(content, language);
    } finally {
      loadResult.model.close();
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
