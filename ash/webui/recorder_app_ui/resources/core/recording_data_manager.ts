// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MAX_SPEAKER_COLORS} from '../components/styles/speaker_label.js';

import {SAMPLE_RATE, SAMPLES_PER_SLICE} from './audio_constants.js';
import {DataDir} from './data_dir.js';
import {computed, ReadonlySignal, signal} from './reactive/signal.js';
import {Transcription, transcriptionSchema} from './soda/soda.js';
import {
  ExportAudioFormat,
  ExportSettings,
  ExportTranscriptionFormat,
} from './state/settings.js';
import {assert, assertExhaustive, assertExists} from './utils/assert.js';
import {AsyncJobQueue} from './utils/async_job_queue.js';
import {Infer, z} from './utils/schema.js';
import {ulid} from './utils/ulid.js';
import {asyncLazyInit, downloadFile} from './utils/utils.js';

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

// Note that a recalculation should be triggered if this value is changed, by
// changing the version value in the versionedTimelineSegmentsSchema.
// TODO(pihsun): Test threshold and see what the normal background noise level
// is.
const NO_AUDIO_POWER_THRESHOLD = 10;

/**
 * The type of a segment in the timeline.
 *
 * The values are set to numbers to save space when written on disk, and the
 * values are explicitly written out since it's written to disk and need to be
 * backward compatible.
 */
export enum TimelineSegmentKind {
  // No audio (power < NO_AUDIO_POWER_THRESHOLD)
  NO_AUDIO = 0,

  // Some audio but it's not speech or transcription is not enabled.
  AUDIO = 1,

  // Audio is speech (defined by the transcription timestamp), but speaker
  // label is not enabled.
  SPEECH = 2,

  // Audio is speech with the "first" to "fifth" speaker label. Note that this
  // is only used for the color which cycles after the fifth speaker, so the
  // sixth speaker will be labeled as `SPEECH_SPEAKER_COLOR_1` here.
  SPEECH_SPEAKER_COLOR_1 = 3,
  SPEECH_SPEAKER_COLOR_2 = 4,
  SPEECH_SPEAKER_COLOR_3 = 5,
  SPEECH_SPEAKER_COLOR_4 = 6,
  SPEECH_SPEAKER_COLOR_5 = 7,
}

// Note that to avoid floating point calculation error, all "time length" /
// "time offset" of the timeline segment calculations are done in number of
// samples instead of seconds.
const timelineSegmentSchema = z.tuple([
  // The length of the segment in number of samples.
  z.number(),
  // The type of the segment.
  z.nativeEnum(TimelineSegmentKind),
]);

type TimelineSegment = Infer<typeof timelineSegmentSchema>;

const TIMELINE_SEGMENT_VERSION = 1;

const vesionedTimelineSegmentsSchema = z.object({
  version: z.literal(TIMELINE_SEGMENT_VERSION),
  segments: z.array(timelineSegmentSchema),
});

type VersionedTimelineSegments = Infer<typeof vesionedTimelineSegmentsSchema>;

/**
 * The recording metadata that are derived from other data.
 *
 * These are used in recording list.
 */
const derivedRecordingMetadataSchema = z.object({
  // Total number of speakers in the recording. `null` if the transcription
  // isn't enabled in the recording. `undefined` or missing field when it's not
  // computed yet.
  numSpeakers: z.optional(z.nullable(z.number())),

  // Length in number of samples, and type of each segment of the recording.
  // Used by the recording list.
  timelineSegments: z.catch(
    z.optional(vesionedTimelineSegmentsSchema),
    undefined,
  ),

  // Description of the recording used by the recording list. This contains a
  // truncated version of the transcription with maximum length
  // `MAX_DESCRIPTION_LENGTH`.
  description: z.string(),

  // There was a field named `powerAverages` here with type number[]. If we
  // ever want to add a field with a same name back, we need to ensure the
  // schema is not compatible.
  // TODO(pihsun): Have some kind of "deprecated" field marker in schema, that
  // just decode everything into undefined.
});

const recordingMetadataSchema = z.intersection([
  baseRecordingMetadataSchema,
  derivedRecordingMetadataSchema,
]);

export type RecordingMetadata = Infer<typeof recordingMetadataSchema>;

const audioPowerSchema = z.object({
  // Array of integer in [0, 255] representing the powers. Each data point
  // corresponds to a slice passed to the audio worklets with SAMPLES_PER_SLICE
  // audio samples.
  // TODO(pihsun): Compression. Use a UInt8Array and CompressionStream?
  powers: z.array(z.number()),
});

type AudioPower = Infer<typeof audioPowerSchema>;

/**
 * The recording create parameters without id field, used for creating new
 * recordings.
 */
export type RecordingCreateParams = Omit<
  AudioPower&BaseRecordingMetadata&{transcription: Transcription | null}, 'id'>;

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

function calculatePowerSegments(powers: number[]): TimelineSegment[] {
  const segments: TimelineSegment[] = [];
  for (const power of powers) {
    const label = power < NO_AUDIO_POWER_THRESHOLD ?
      TimelineSegmentKind.NO_AUDIO :
      TimelineSegmentKind.AUDIO;
    if (segments.length === 0 || assertExists(segments.at(-1))[1] !== label) {
      segments.push([SAMPLES_PER_SLICE, label]);
    } else {
      assertExists(segments.at(-1))[0] += SAMPLES_PER_SLICE;
    }
  }
  return segments;
}

function speakerLabelToTimelineSegmentKind(
  speakerLabels: string[],
  speakerLabel: string|null,
): TimelineSegmentKind {
  if (speakerLabel === null) {
    return TimelineSegmentKind.SPEECH;
  }
  const speakerLabelIdx = speakerLabels.indexOf(speakerLabel);
  assert(speakerLabelIdx !== -1);
  return assertExists(
    [
      TimelineSegmentKind.SPEECH_SPEAKER_COLOR_1,
      TimelineSegmentKind.SPEECH_SPEAKER_COLOR_2,
      TimelineSegmentKind.SPEECH_SPEAKER_COLOR_3,
      TimelineSegmentKind.SPEECH_SPEAKER_COLOR_4,
      TimelineSegmentKind.SPEECH_SPEAKER_COLOR_5,
    ][speakerLabelIdx % MAX_SPEAKER_COLORS],
  );
}

interface OverlayTimelineSegment {
  /**
   * Start time in number of samples.
   */
  start: number;
  /**
   * End time in number of samples.
   */
  end: number;
  /**
   * Type of the segment.
   */
  kind: TimelineSegmentKind;
}

function calculateSpeechSegments(
  transcription: Transcription|null,
): OverlayTimelineSegment[] {
  if (transcription === null) {
    return [];
  }
  const speakerLabels = transcription.getSpeakerLabels();
  let currentSampleOffset = 0;
  const segments: OverlayTimelineSegment[] = [];
  for (const paragraph of transcription.getParagraphs()) {
    const firstPart = assertExists(paragraph[0]);
    const lastPart = assertExists(paragraph.at(-1));

    const startMs = firstPart.timeRange?.startMs ?? null;
    const endMs = lastPart.timeRange?.endMs ?? null;
    if (startMs === null || endMs === null) {
      // TODO(pihsun): Check if there's any possibility that the timestamp is
      // missing. Asserting that timestamp exists at type level, and assert
      // that the ranges are incrasing would simplify many code.
      continue;
    }
    // The timestamps should be increasing.
    const start = Math.round((startMs / 1000) * SAMPLE_RATE);
    const end = Math.round((endMs / 1000) * SAMPLE_RATE);
    assert(currentSampleOffset <= start && start <= end);
    if (end > start) {
      segments.push({
        start,
        end,
        kind: speakerLabelToTimelineSegmentKind(
          speakerLabels,
          firstPart.speakerLabel,
        ),
      });
    }
    currentSampleOffset = end;
  }
  return segments;
}

/**
 * Overlays the `segments` onto `baseSegments`, splitting the segments if
 * necessary.
 *
 * The total length of the returned segment will be the same as `baseSegments`.
 */
function overlaySegments(
  baseSegments: TimelineSegment[],
  overlaySegments: OverlayTimelineSegment[],
): TimelineSegment[] {
  let overlayIdx = 0;
  const ret: TimelineSegment[] = [];
  let time = 0;

  function forwardTime(end: number, kind: TimelineSegmentKind) {
    assert(end >= time);
    const length = end - time;
    if (length > 0) {
      ret.push([length, kind]);
    }
    time = end;
  }

  for (const [baseLength, baseKind] of baseSegments) {
    const baseEnd = time + baseLength;
    while (overlayIdx < overlaySegments.length) {
      const {start, end, kind} = assertExists(overlaySegments[overlayIdx]);
      if (start >= baseEnd) {
        // Next overlay after current segment.
        break;
      }
      if (start >= time) {
        forwardTime(start, baseKind);
      }
      if (end >= baseEnd) {
        // TODO(pihsun): This split the overlay segment into multiple based on
        // the boundary of the baseSegments. We don't necessary need to do this
        // but the code is easier to write with this, and the resulting number
        // of segments is still the same order of magnitude.
        forwardTime(baseEnd, kind);
        break;
      }
      forwardTime(end, kind);
      overlayIdx++;
    }
    forwardTime(baseEnd, baseKind);
  }
  return ret;
}

// Note that this is not an exact upper limit of the number of segments
// returned from `simplifySegments`, and The current heuristic result in at
// most `2 * SIMPLIFY_SEGMENT_RESOLUTION - 1` segments.
const SIMPLIFY_SEGMENT_RESOLUTION = 512;

/**
 * Simplifies the segments by merging the shorter segments, so the total number
 * of segments are at most constant regardless of the recording length.
 *
 * The herustic to achieve this is:
 * * Define `threshold` to be `max(total length / SIMPLIFY_SEGMENT_RESOLUTION, 1
 *   second)`.
 * * Segments longer than the `threshold` are preserved as is.
 * * Consecutive segments shorter than `threshold` are merge together,
 *   until it's longer than the `threshold`. The new kind will be the most
 *   frequent kind in the range.
 *
 * Note that this doesn't guarantee that all returned segments are longer than
 * `threshold`, as there might be some segments shorter than `threshold` left
 * between longer segments.
 */
function simplifySegments(segments: TimelineSegment[]): TimelineSegment[] {
  const totalLength =
    segments.map(([length]) => length).reduce((a, b) => a + b, 0);
  const threshold = Math.max(
    totalLength / SIMPLIFY_SEGMENT_RESOLUTION,
    SAMPLE_RATE,
  );

  const ret: TimelineSegment[] = [];
  let currentGroup: TimelineSegment[] = [];
  let currentGroupLength = 0;

  function endCurrentGroup() {
    if (currentGroup.length === 0) {
      return;
    }
    const kindLengths = new Map<TimelineSegmentKind, number>();
    for (const [length, kind] of currentGroup) {
      kindLengths.set(kind, (kindLengths.get(kind) ?? 0) + length);
    }
    let kind: TimelineSegmentKind|null = null;
    let maxLength = 0;
    for (const [k, length] of kindLengths.entries()) {
      if (length >= maxLength) {
        maxLength = length;
        kind = k;
      }
    }
    ret.push([currentGroupLength, assertExists(kind)]);
    currentGroup = [];
    currentGroupLength = 0;
  }

  function push(segment: TimelineSegment) {
    currentGroup.push(segment);
    currentGroupLength += segment[0];
  }

  for (const segment of segments) {
    const [length] = segment;
    if (length >= threshold) {
      endCurrentGroup();
    }
    push(segment);
    if (currentGroupLength >= threshold) {
      endCurrentGroup();
    }
  }
  endCurrentGroup();
  return ret;
}

function calculateTimelineSegments(
  powers: number[],
  transcription: Transcription|null,
): VersionedTimelineSegments {
  const powerSegments = calculatePowerSegments(powers);
  const speechSegments = calculateSpeechSegments(transcription);

  const segments = simplifySegments(
    overlaySegments(powerSegments, speechSegments),
  );
  return {
    version: TIMELINE_SEGMENT_VERSION,
    segments,
  };
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

    for (const [id, meta] of Object.entries(metadata)) {
      this.fillDerivedMetadata(id, meta).catch((e) => {
        console.error(`error while filling derived data for ${id}`, e);
      });
    }
  }

  private async fillDerivedMetadata(id: string, meta: RecordingMetadata) {
    const getTranscription = asyncLazyInit(() => this.getTranscription(id));
    const getPowers = asyncLazyInit(() => this.getAudioPower(id));
    let changed = false;

    if (meta.numSpeakers === undefined) {
      changed = true;
      const transcription = await getTranscription();
      meta = {
        ...meta,
        numSpeakers: transcription?.getSpeakerLabels().length ?? null,
      };
    }

    if (meta.timelineSegments === undefined) {
      changed = true;
      const transcription = await getTranscription();
      const {powers} = await getPowers();
      meta = {
        ...meta,
        timelineSegments: calculateTimelineSegments(powers, transcription),
      };
    }

    if (changed) {
      this.setMetadata(id, meta);
    }
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
    {transcription, powers, ...meta}: RecordingCreateParams,
    audio: Blob,
  ): Promise<string> {
    const id = ulid();
    const numSpeakers = transcription?.getSpeakerLabels().length ?? null;
    const description = transcription?.toShortDescription() ?? '';
    const timelineSegments = calculateTimelineSegments(powers, transcription);
    const fullMeta = {
      id,
      description,
      timelineSegments,
      numSpeakers,
      ...meta,
    };
    this.setMetadata(id, fullMeta);
    await Promise.all([
      this.dataDir.write(
        audioPowerName(id),
        audioPowerSchema.stringifyJson({powers}),
      ),
      this.dataDir.write(
        transcriptionName(id),
        transcriptionSchema.stringifyJson(transcription),
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

  /**
   * Gets the transcription of the given recording.
   *
   * Since transcription can be enabled / disabled during the recording, the
   * transcription might only contain part of the transcription when
   * transcription is enabled.
   *
   * If the transcription is never enabled while recording, the function will
   * return null, which is different from returning an empty transcription
   * (transcription was enabled but no speech is detected).
   */
  async getTranscription(id: string): Promise<Transcription|null> {
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
    const transcription = await this.getTranscription(id);
    if (transcription === null || transcription.isEmpty()) {
      return;
    }

    switch (format) {
      case ExportTranscriptionFormat.TXT: {
        const text = transcription.toExportText();
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
