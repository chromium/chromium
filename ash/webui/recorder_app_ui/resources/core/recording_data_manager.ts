// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DataDir} from './data_dir.js';
import {computed, ReadonlySignal, signal} from './reactive/signal.js';
import {concatTextTokens, TextToken, textTokenSchema} from './soda/soda.js';
import {
  ExportAudioFormat,
  ExportSettings,
  ExportTranscriptionFormat,
} from './state/settings.js';
import {assertExhaustive, assertExists} from './utils/assert.js';
import {AsyncJobQueue} from './utils/async_job_queue.js';
import {Infer, z} from './utils/schema.js';
import {ulid} from './utils/ulid.js';
import {downloadFile} from './utils/utils.js';

/**
 * The base recording metadata.
 *
 * Since metadata are all read in the main page, cached in memory, and can be
 * edited, it should be small and takes constant memory.
 */
const baseRecordingMetadataSchema = z.object({
  id: z.string(),
  title: z.string(),
  durationMs: z.number(),

  // Use number instead of Date to make metadata JSON-serializable.
  // The number here is same as the value returned by Date.now(), which is the
  // number of milliseconds elapsed since the epoch.
  recordedAt: z.number(),
});

type BaseRecordingMetadata = Infer<typeof baseRecordingMetadataSchema>;

const MAX_POWER_AVERAGES = 512;

const MAX_DESCRIPTION_LENGTH = 512;

/**
 * The recording metadata that are derived from other data.
 *
 * These are used in recording list.
 */
const derivedRecordingMetadataSchema = z.object({
  // Average of the powers used by the recording list. This contains at most
  // `MAX_POWER_AVERAGES` samples.
  powerAverages: z.array(z.number()),

  // Description of the recording used by the recording list. This contains a
  // truncated version of the transcription with maximum length
  // `MAX_DESCRIPTION_LENGTH`.
  description: z.string(),
});

const recordingMetadataSchema = z.intersection([
  baseRecordingMetadataSchema,
  derivedRecordingMetadataSchema,
] as const);

export type RecordingMetadata = Infer<typeof recordingMetadataSchema>;

const audioPowerSchema = z.object({
  // Array of integer in [0, 255] representing the powers. Each data point
  // corresponds to a slice passed to the audio worklets with SAMPLES_PER_SLICE
  // audio samples.
  // TODO(pihsun): Compression. Use a UInt8Array and CompressionStream?
  powers: z.array(z.number()),
});

type AudioPower = Infer<typeof audioPowerSchema>;

const transcriptionSchema = z.object({
  // Transcriptions in form of text tokens.
  //
  // Since transcription can be enabled / disabled during the recording, the
  // `textTokens` might only contain part of the transcription when
  // transcription is enabled.
  //
  // If the transcription is never enabled while recording, `textTokens` will
  // be null (to show a different state in playback view).
  textTokens: z.nullable(z.array(textTokenSchema)),
});

type Transcription = Infer<typeof transcriptionSchema>;

/**
 * The recording create parameters without id field, used for creating new
 * recordings.
 */
export type RecordingCreateParams =
  Omit<AudioPower&BaseRecordingMetadata&Transcription, 'id'>;

// TODO(pihsun): Type-safe wrapper for reading / writing specific type of file?
function metadataName(id: string) {
  return `${id}.meta.json`;
}

function audioPowerName(id: string) {
  return `${id}.powers.json`;
}

function transcriptionName(id: string) {
  return `${id}.transcription.json`;
}

function audioName(id: string) {
  return `${id}.webm`;
}

function calculateDescription(textTokens: TextToken[]|null): string {
  if (textTokens === null) {
    return '';
  }
  const transcription = concatTextTokens(textTokens);
  if (transcription.length <= MAX_DESCRIPTION_LENGTH - 3) {
    return transcription;
  }
  return transcription.substring(0, MAX_DESCRIPTION_LENGTH - 3) + '...';
}

function calculatePowerAverages(powers: number[]): number[] {
  if (powers.length <= MAX_POWER_AVERAGES) {
    return powers;
  }
  const averages: number[] = [];
  for (let i = 0; i < MAX_POWER_AVERAGES; i++) {
    const start = Math.floor((i * powers.length) / MAX_POWER_AVERAGES);
    const end = Math.floor(((i + 1) * powers.length) / MAX_POWER_AVERAGES);
    let sum = 0;
    for (let j = start; j < end; j++) {
      sum += assertExists(powers[j]);
    }
    averages.push(sum / (end - start));
  }
  return averages;
}

/**
 * Gets the default filename without extension for the exported file.
 *
 * The returned filename is used as a suggested name to Chrome and Chrome will
 * further remove all characters that are not safe for filename, so we don't
 * need to filter any characters here.
 */
export function getDefaultFileNameWithoutExtension(
  meta: RecordingMetadata,
): string {
  // Replace all ":" with ".", since Chrome defaults to replacing ':' with '_',
  // and we want to align with screen capture/recording default name which use
  // '.' as separator.
  // Example default title: `Audio recording 2024-12-04 12:34:56`
  // Example default filename: `Audio recording 2024-12-04 12.34.56.<extension>`
  return meta.title.replaceAll(':', '.');
}

// TODO(pihsun): Use a Map when draft.ts supports it.
export type RecordingMetadataMap = Record<string, RecordingMetadata>;

/**
 * The recording data manager to manage the audio file (.webm) and metadata
 * file (.json) pairs.
 */
export class RecordingDataManager {
  /**
   * Cached metadata for all recordings.
   *
   * The cache might be inconsistent to the file on disk if there are multiple
   * recorder instance running, but since we're only allowing a single instance
   * of recorder app, that shouldn't be an issue.
   *
   * TODO(pihsun): Check if there's any case we'll have more than one
   * instance running.
   */
  private readonly cachedMetadataMap = signal<RecordingMetadataMap>({});

  /**
   * Queues for writing metadata to the disk. Each recording ID gets it own
   * queue.
   *
   * TODO(pihsun): Have an on exit handler that waits for all writes to finish
   * before exiting app.
   */
  private readonly metadataWriteQueues = new Map<string, AsyncJobQueue>();

  static async create(dataDir: DataDir): Promise<RecordingDataManager> {
    async function getMetadataFromFilename(
      name: string,
    ): Promise<RecordingMetadata> {
      const file = await dataDir.read(name);
      const text = await file.text();
      // TODO(shik): Add versioning, and error handling.
      // Currently the JSON is not exposed in the real filesystem and we simply
      // use the parsed result.
      const data = recordingMetadataSchema.parseJson(text);
      return data;
    }

    const filenames = await dataDir.list();
    const metadataMap = Object.fromEntries(
      await Promise.all(
        filenames.filter((x) => x.endsWith('.meta.json')).map(async (x) => {
          const meta = await getMetadataFromFilename(x);
          return [meta.id, meta] as const;
        }),
      ),
    );
    return new RecordingDataManager(dataDir, metadataMap);
  }

  private constructor(
    private readonly dataDir: DataDir,
    metadata: RecordingMetadataMap,
  ) {
    this.cachedMetadataMap.value = metadata;
  }

  private getWriteQueueFor(id: string) {
    const queue = this.metadataWriteQueues.get(id);
    if (queue !== undefined) {
      return queue;
    }
    const newQueue = new AsyncJobQueue('keepLatest');
    this.metadataWriteQueues.set(id, newQueue);
    return newQueue;
  }

  /**
   * @return The created recording id.
   */
  async createRecording(
    {textTokens, powers, ...meta}: RecordingCreateParams,
    audio: Blob,
  ): Promise<string> {
    const id = ulid();
    const description = calculateDescription(textTokens);
    const powerAverages = calculatePowerAverages(powers);
    const fullMeta = {id, description, powerAverages, ...meta};
    this.setMetadata(id, fullMeta);
    await Promise.all([
      this.dataDir.write(
        audioPowerName(id),
        audioPowerSchema.stringifyJson({powers}),
      ),
      this.dataDir.write(
        transcriptionName(id),
        transcriptionSchema.stringifyJson({textTokens}),
      ),
      this.dataDir.write(audioName(id), audio),
    ]);
    return id;
  }

  /**
   * Gets the current cached metadata list.
   */
  getAllMetadata(): ReadonlySignal<RecordingMetadataMap> {
    return this.cachedMetadataMap;
  }

  private getMetadataRaw(id: string): RecordingMetadata|null {
    return this.cachedMetadataMap.value[id] ?? null;
  }

  getMetadata(id: string): ReadonlySignal<RecordingMetadata|null> {
    return computed(() => this.getMetadataRaw(id));
  }

  setMetadata(id: string, meta: RecordingMetadata): void {
    this.cachedMetadataMap.mutate((m) => {
      m[id] = meta;
    });
    this.getWriteQueueFor(id).push(async () => {
      await this.dataDir.write(
        metadataName(id),
        recordingMetadataSchema.stringifyJson(meta),
      );
    });
  }

  async getAudioFile(id: string): Promise<File> {
    const name = audioName(id);
    const file = await this.dataDir.read(name);
    return file;
  }

  async getTranscription(id: string): Promise<Transcription> {
    const name = transcriptionName(id);
    const file = await this.dataDir.read(name);
    const text = await file.text();
    return transcriptionSchema.parseJson(text);
  }

  async getAudioPower(id: string): Promise<AudioPower> {
    const name = audioPowerName(id);
    const file = await this.dataDir.read(name);
    const text = await file.text();
    return audioPowerSchema.parseJson(text);
  }

  async clear(): Promise<void> {
    // TODO(pihsun): There's definitely race condition between this and the
    // background writes in this.metadataWriteQueues, but since this is only
    // used in the /dev page it might not worth the extra effort to make it
    // 100% correct.
    await this.dataDir.clear();
    this.cachedMetadataMap.value = {};
  }

  remove(id: string): void {
    this.cachedMetadataMap.mutate((m) => {
      delete m[id];
    });
    this.getWriteQueueFor(id).push(async () => {
      await this.dataDir.remove(metadataName(id));
    });
    // TODO(pihsun): Since all removal of audio should be independent of each
    // other, there's not much reason to put this in a queue.
    void this.dataDir.remove(audioName(id));
    void this.dataDir.remove(transcriptionName(id));
    void this.dataDir.remove(audioPowerName(id));
  }

  private async exportAudio(id: string, format: ExportAudioFormat) {
    const metadata = this.getMetadataRaw(id);
    if (metadata === null) {
      return;
    }

    const file = await this.getAudioFile(id);
    switch (format) {
      case ExportAudioFormat.WEBM_ORIGINAL: {
        const filename = getDefaultFileNameWithoutExtension(metadata) + '.webm';
        downloadFile(filename, file);
        break;
      }
      default:
        assertExhaustive(format);
    }
  }

  private async exportTranscription(
    id: string,
    format: ExportTranscriptionFormat,
  ) {
    const metadata = this.getMetadataRaw(id);
    if (metadata === null) {
      return;
    }
    const {textTokens} = await this.getTranscription(id);
    // TODO(pihsun): This "transcription available" logic exists at multiple
    // places, consolidate them.
    if (textTokens === null || textTokens.length === 0) {
      return;
    }

    switch (format) {
      case ExportTranscriptionFormat.TXT: {
        const text = concatTextTokens(textTokens);
        const blob = new Blob([text], {type: 'text/plain'});
        const filename = getDefaultFileNameWithoutExtension(metadata) + '.txt';
        downloadFile(filename, blob);
        break;
      }
      default:
        assertExhaustive(format);
    }
  }

  async exportRecording(id: string, settings: ExportSettings): Promise<void> {
    if (settings.transcription) {
      await this.exportTranscription(id, settings.transcriptionFormat);
    }
    if (settings.audio) {
      await this.exportAudio(id, settings.audioFormat);
    }
  }
}
