// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const TRUSTED_ORIGIN = 'chrome://focus-mode-media';

interface Track {
  // The URL of the audio data.
  mediaUrl: string;
  // The track thumbnail in the form of a data URL.
  thumbnailUrl: string;
  // The title.
  title: string;
  // The artist.
  artist: string;
}

function isTrack(a: any): a is Track {
  return (
      a && typeof a == 'object' && typeof a.mediaUrl == 'string' &&
      typeof a.thumbnailUrl == 'string' && typeof a.title == 'string' &&
      typeof a.artist == 'string');
}

interface Command {
  cmd: string;
  arg: any;
}

function isCommand(a: any): a is Command {
  return a && typeof a == 'object' && typeof a.cmd == 'string';
}

function sendTrackRequest() {
  parent.postMessage({cmd: 'gettrack'}, TRUSTED_ORIGIN);
}

interface PlaybackStatus {
  state: string;
  position: number;
  initial: boolean;
}
let playbackStatus: PlaybackStatus|null = null;

function replyPlaybackStatus(newState: string|null) {
  // Do not send status update if the track has not been loaded yet.
  if (!playbackStatus || (playbackStatus.state == 'none' && newState == null)) {
    return;
  }

  if (newState != null) {
    playbackStatus.state = newState;
  }
  playbackStatus.position = getPlayerElement().currentTime;

  parent.postMessage(
      {
        cmd: 'replyplaybackstatus',
        state: playbackStatus.state,
        position: playbackStatus.position,
        initial: playbackStatus.initial,
      },
      TRUSTED_ORIGIN);

  playbackStatus.initial = false;
}

function getPlayerElement(): HTMLAudioElement {
  return document.getElementById('player') as HTMLAudioElement;
}

function loadTrack(track: Track) {
  const p = getPlayerElement();
  p.src = track.mediaUrl;

  const metadata: any = {
    title: track.title,
    artist: track.artist,
  };
  if (track.thumbnailUrl) {
    metadata.artwork = [{src: track.thumbnailUrl}];
  }
  navigator.mediaSession.metadata = new MediaMetadata(metadata);
  playbackStatus = {
    state: 'none',
    position: 0,
    initial: true,
  };
}

globalThis.addEventListener('load', () => {
  getPlayerElement().addEventListener('play', () => {
    replyPlaybackStatus('playing');
  });

  getPlayerElement().addEventListener('pause', () => {
    replyPlaybackStatus('paused');
  });

  getPlayerElement().addEventListener('ended', () => {
    replyPlaybackStatus('ended');
    sendTrackRequest();
  });

  // Registering this makes the "next track" button show up in the media
  // controls. We do not support going to the previous track.
  navigator.mediaSession.setActionHandler('nexttrack', () => {
    replyPlaybackStatus('switchedtonext');
    sendTrackRequest();
  });

  sendTrackRequest();
});

globalThis.addEventListener('message', (event: MessageEvent) => {
  if (event.origin != TRUSTED_ORIGIN) {
    return;
  }

  const data = event.data;
  if (isCommand(data)) {
    if (data.cmd == 'play' && isTrack(data.arg)) {
      loadTrack(data.arg);
    } else if (data.cmd == 'queryplaybackstatus') {
      replyPlaybackStatus(null);
    }
  }
});
