// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MediaClientInterface, MediaClientReceiver, PlaybackState, TrackDefinition, TrackProvider, TrackProviderInterface} from './focus_mode.mojom-webui.js';

const UNTRUSTED_ORIGIN = 'chrome-untrusted://focus-mode-player';

// Implemented here, used by Ash to directly start a different track.
let clientInstance: MediaClientImpl|null = null;

// Implemented by Ash, provides the tracks that we will play.
let providerInstance: TrackProviderInterface|null = null;

interface PlaybackReportingConfig {
  intervalShort: number;
  intervalLong: number;
  intervalThreshold: number;
  intervalId: number;
}
const reportConfig: PlaybackReportingConfig = {
  intervalShort: 10,
  intervalLong: 40,
  intervalThreshold: 40,
  intervalId: setInterval(() => postQueryPlaybackStatusRequest(), 2000),
};

interface PlaybackStatus {
  state: string;
  position: number;
  loadTime: Date;
}
let playbackStatus: PlaybackStatus|null = null;

let currentTrack: TrackDefinition|null = null;

let clientTimeLastReport: Date;

// Valid playback state string values.
const validStates = ['playing', 'paused', 'ended', 'switchedtonext'];

// Check if the playback state comes with valid values.
function isValidPlaybackStatus(playbackStatus: PlaybackStatus): boolean {
  return validStates.includes(playbackStatus.state) &&
      playbackStatus.position >= 0 && playbackStatus.position <= 18000;
}

function getPlaybackState(playbackStateString: string): PlaybackState {
  switch (playbackStateString) {
    case 'playing':
      return PlaybackState.kPlaying;
    case 'paused':
      return PlaybackState.kPaused;
    case 'switchedtonext':
      return PlaybackState.kSwitchedToNext;
    case 'ended':
      return PlaybackState.kEnded;
  }
  return PlaybackState.kNone;
}

function getDuration(
    oldPlaybackStatus: PlaybackStatus|null,
    newPlaybackStatus: PlaybackStatus|null): [number, number] {
  return [
    Math.floor(oldPlaybackStatus?.position ?? 0),
    Math.floor(newPlaybackStatus?.position ?? 0),
  ];
}

function shouldReportPlayback(newPlaybackStatus: PlaybackStatus): boolean {
  const [start, end] = getDuration(playbackStatus, newPlaybackStatus);
  const interval = end <= reportConfig.intervalThreshold ?
      reportConfig.intervalShort :
      reportConfig.intervalLong;

  // The condition for minimal interval needs to be a little more permissive by
  // 1s in case the previous timer fires at 30.01s and the current timer fires
  // at 39.98s.
  return start < end &&
      (end - start + 1 >= interval || newPlaybackStatus.state == 'ended' ||
       newPlaybackStatus.state == 'switchedtonext');
}

function onReceiveNewPlaybackStatus(newPlaybackStatus: PlaybackStatus) {
  if (currentTrack == null || !currentTrack.enablePlaybackReporting ||
      !isValidPlaybackStatus(newPlaybackStatus)) {
    return;
  }

  const clientCurrentTime: Date = new Date();
  const playbackStartOffset: number = Math.floor(
      (clientCurrentTime.getTime() - newPlaybackStatus.loadTime.getTime()) /
      1000);
  const initial = (playbackStatus == null);

  if (shouldReportPlayback(newPlaybackStatus)) {
    const [start, end] = getDuration(playbackStatus, newPlaybackStatus);
    getProvider().reportPlayback({
      state: getPlaybackState(newPlaybackStatus.state),
      title: currentTrack.title,
      url: currentTrack.mediaUrl,
      clientCurrentTime: {msec: clientCurrentTime.getTime()},
      playbackStartOffset: playbackStartOffset,
      mediaTimeCurrent: newPlaybackStatus.position,
      mediaStart: start,
      mediaEnd: end,
      clientStartTime: {
        msec: (initial ? newPlaybackStatus.loadTime : clientTimeLastReport)
                  .getTime(),
      },
      initialPlayback: initial,
    });
    playbackStatus = newPlaybackStatus;
    clientTimeLastReport = clientCurrentTime;
  }

  // Track playback is complete. Reset `currentTrack` to null until the new
  // track is loaded.
  if (newPlaybackStatus.state == 'ended' ||
      newPlaybackStatus.state == 'switchedtonext') {
    currentTrack = null;
  }
}

function isEventData(data: any): boolean {
  return data && typeof data == 'object' && typeof data.cmd == 'string';
}

function isNextTrackEventData(data: any): boolean {
  return isEventData(data) && data.cmd == 'gettrack';
}

function isPlaybackStatus(data: any): boolean {
  return (
      isEventData(data) && data.cmd == 'replyplaybackstatus' &&
      typeof data.state == 'string' && typeof data.position == 'number' &&
      typeof data.loadTime == 'object' && data.loadTime instanceof Date);
}

function isMediaErrorEventData(data: any): boolean {
  return isEventData(data) && data.cmd == 'mediaErrorEvent';
}

function getProvider(): TrackProviderInterface {
  if (!providerInstance) {
    providerInstance = TrackProvider.getRemote();
  }
  return providerInstance;
}

// Post a track play request to the iframe.
function postPlayRequest(track: TrackDefinition) {
  if (!track.mediaUrl.url) {
    // If there is no valid URL, then there's no point in continuing.
    getProvider().reportPlayerError();
    return;
  }

  const child = document.getElementById('child') as HTMLIFrameElement;
  if (child.contentWindow) {
    playbackStatus = null;
    currentTrack = track;
    child.contentWindow.postMessage(
        {
          cmd: 'play',
          arg: {
            mediaUrl: track.mediaUrl.url,
            thumbnailUrl: track.thumbnailUrl.url,
            title: track.title,
            artist: track.artist,
          },
        },
        UNTRUSTED_ORIGIN);
  }
}

// Post a query request for playback status to the iframe.
function postQueryPlaybackStatusRequest() {
  if (currentTrack == null || !currentTrack.enablePlaybackReporting) {
    return;
  }

  const child = document.getElementById('child') as HTMLIFrameElement;
  if (child.contentWindow) {
    child.contentWindow.postMessage(
        {
          cmd: 'queryplaybackstatus',
        },
        UNTRUSTED_ORIGIN);
  }
}

class MediaClientImpl implements MediaClientInterface {
  static init() {
    if (!clientInstance) {
      clientInstance = new MediaClientImpl();
    }
  }

  static shutdown() {
    if (clientInstance) {
      clientInstance.receiver_.$.close();
      clientInstance = null;
    }
  }

  startPlay(track: TrackDefinition): void {
    postPlayRequest(track);
  }

  private receiver_: MediaClientReceiver = this.initReceiver_();

  private initReceiver_(): MediaClientReceiver {
    const receiver = new MediaClientReceiver(this);
    getProvider().setMediaClient(receiver.$.bindNewPipeAndPassRemote());
    return receiver;
  }
}

globalThis.addEventListener('load', async () => {
  MediaClientImpl.init();
});

// Tracks whether we are currently requesting a track.
let requestInProgress = false;

globalThis.addEventListener('message', async (event: MessageEvent) => {
  if (event.origin != UNTRUSTED_ORIGIN) {
    return;
  }

  const data = event.data;
  if (isEventData(data)) {
    if (isNextTrackEventData(data)) {
      if (requestInProgress) {
        // There is no point in doing concurrent requests, so if we get a new
        // request while another is pending (this can happen if the user hammers
        // the next track button in the media controls), then the request is
        // simply dropped.
        return;
      }

      requestInProgress = true;
      const result = await getProvider().getTrack();
      requestInProgress = false;

      postPlayRequest(result.track);
    } else if (isPlaybackStatus(data)) {
      onReceiveNewPlaybackStatus({
        state: data.state,
        position: data.position,
        loadTime: data.loadTime,
      });
    } else if (isMediaErrorEventData(data)) {
      getProvider().reportPlayerError();
    }
  }
});
