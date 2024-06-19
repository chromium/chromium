// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DataDir} from './data_dir.js';
import {computed, ReadonlySignal, signal} from './reactive/signal.js';
import {textTokenSchema} from './soda/soda.js';
import {AsyncJobQueue} from './utils/async_job_queue.js';
import {Infer, z} from './utils/schema.js';
import {ulid} from './utils/ulid.js';

const recordingMetadataSchema = z.object({
  id: z.string(),
  title: z.string(),
  durationMs: z.number(),

  // Use number instead of Date to make metadata JSON-serializable.
  // The number here is same as the value returned by Date.now(), which is the
  // number of milliseconds elapsed since the epoch.
  recordedAt: z.number(),

  // TODO(pihsun): Since metadata are cached in memory and can be edited,
  // ideally we'd want it to be small and takes constant memory. Move powers
  // and textTokens to a separate file similar to how we handle audio data.

  // Array of integer in [0, 255] representing the powers. Each data point
  // corresponds to a slice passed to the audio worklets with SAMPLES_PER_SLICE
  // audio samples.
  // TODO(pihsun): Compression. Use a UInt8Array and CompressionStream?
  powers: z.array(z.number()),

  // Transcriptions in form of text tokens.
  textTokens: z.array(textTokenSchema),
});

export type RecordingMetadata = Infer<typeof recordingMetadataSchema>;

/**
 * The recording metadata without id field, used for creation.
 */
export type RecordingMetadataCreateParams = Omit<RecordingMetadata, 'id'>;

function metadataName(id: string) {
  return `${id}.json`;
}

function audioName(id: string) {
  return `${id}.webm`;
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
            filenames.filter((x) => x.endsWith('.json')).map(async (x) => {
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
      meta: RecordingMetadataCreateParams,
      audio: Blob,
      ): Promise<string> {
    const id = ulid();
    const fullMeta = {id, ...meta};
    this.setMetadata(id, fullMeta);
    await this.dataDir.write(audioName(id), audio);
    return id;
  }

  /**
   * Gets the current cached metadata list.
   */
  getAllMetadata(): ReadonlySignal<RecordingMetadataMap> {
    return this.cachedMetadataMap;
  }

  getMetadata(id: string): ReadonlySignal<RecordingMetadata|null> {
    return computed(() => this.cachedMetadataMap.value[id] ?? null);
  }

  setMetadata(id: string, meta: RecordingMetadata): void {
    this.cachedMetadataMap.mutate((m) => {
      m[id] = meta;
    });
    this.getWriteQueueFor(id).push(async () => {
      await this.dataDir.write(metadataName(id), JSON.stringify(meta));
    });
  }

  async getAudioFile(id: string): Promise<File> {
    const name = audioName(id);
    const file = await this.dataDir.read(name);
    return file;
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
  }
}
